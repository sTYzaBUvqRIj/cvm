/*
 * vm.c  —  C/C++ Register VM  —  Implementation
 *
 * Compile with -DVM_DEBUG to enable the ctx->debug runtime tracing flag.
 */

#include "vm.h"

#include <math.h>       /* isnan(), fabsf(), sqrtf(), etc.  */
#include <stdio.h>      /* fprintf(), stderr  (VM_DEBUG)    */
#include <stdlib.h>     /* qsort()                          */
#include <string.h>     /* memset(), memcpy(), memmove()    */

/* =========================================================================
 * Portability helpers — bit manipulation and wide multiplication
 *
 * CLZ/CTZ/POPCNT use compiler intrinsics where available, with pure-C
 * fallbacks.  MULH_I64/U64 use __int128 on GCC/Clang and the MSVC
 * intrinsics __mulh/__umulh on MSVC x64.
 * ====================================================================== */

/* --- CLZ (count leading zeros) ------------------------------------------ */
#if defined(__GNUC__) || defined(__clang__)
#  define VM_CLZ32(x)    ((uint32_t)((x) ? __builtin_clz(x)   : 32))
#  define VM_CLZ64(x)    ((uint32_t)((x) ? __builtin_clzll(x) : 64))
#elif defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
#  include <intrin.h>
static uint32_t vm_clz32_msvc(uint32_t x) {
    if (!x) return 32;
    unsigned long idx; _BitScanReverse(&idx, x); return 31u - (uint32_t)idx;
}
static uint32_t vm_clz64_msvc(uint64_t x) {
    if (!x) return 64;
    unsigned long idx; _BitScanReverse64(&idx, x); return 63u - (uint32_t)idx;
}
#  define VM_CLZ32(x)  vm_clz32_msvc(x)
#  define VM_CLZ64(x)  vm_clz64_msvc(x)
#else
static uint32_t vm_clz32_fallback(uint32_t x) {
    if (!x) return 32;
    uint32_t n = 0;
    if (!(x & 0xFFFF0000u)) { n += 16; x <<= 16; }
    if (!(x & 0xFF000000u)) { n += 8;  x <<= 8;  }
    if (!(x & 0xF0000000u)) { n += 4;  x <<= 4;  }
    if (!(x & 0xC0000000u)) { n += 2;  x <<= 2;  }
    if (!(x & 0x80000000u)) { n += 1; }
    return n;
}
static uint32_t vm_clz64_fallback(uint64_t x) {
    if (!x) return 64;
    uint32_t hi = (uint32_t)(x >> 32);
    return hi ? vm_clz32_fallback(hi) : 32 + vm_clz32_fallback((uint32_t)x);
}
#  define VM_CLZ32(x)  vm_clz32_fallback((uint32_t)(x))
#  define VM_CLZ64(x)  vm_clz64_fallback((uint64_t)(x))
#endif

/* --- CTZ (count trailing zeros) ----------------------------------------- */
#if defined(__GNUC__) || defined(__clang__)
#  define VM_CTZ32(x)    ((uint32_t)((x) ? __builtin_ctz(x)   : 32))
#  define VM_CTZ64(x)    ((uint32_t)((x) ? __builtin_ctzll(x) : 64))
#elif defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
static uint32_t vm_ctz32_msvc(uint32_t x) {
    if (!x) return 32;
    unsigned long idx; _BitScanForward(&idx, x); return (uint32_t)idx;
}
static uint32_t vm_ctz64_msvc(uint64_t x) {
    if (!x) return 64;
    unsigned long idx; _BitScanForward64(&idx, x); return (uint32_t)idx;
}
#  define VM_CTZ32(x)  vm_ctz32_msvc(x)
#  define VM_CTZ64(x)  vm_ctz64_msvc(x)
#else
static uint32_t vm_ctz32_fallback(uint32_t x) {
    if (!x) return 32;
    uint32_t n = 0;
    if (!(x & 0xFFFFu))  { n += 16; x >>= 16; }
    if (!(x & 0xFFu))    { n += 8;  x >>= 8;  }
    if (!(x & 0xFu))     { n += 4;  x >>= 4;  }
    if (!(x & 0x3u))     { n += 2;  x >>= 2;  }
    if (!(x & 0x1u))     { n += 1; }
    return n;
}
static uint32_t vm_ctz64_fallback(uint64_t x) {
    if (!x) return 64;
    uint32_t lo = (uint32_t)x;
    return lo ? vm_ctz32_fallback(lo) : 32 + vm_ctz32_fallback((uint32_t)(x >> 32));
}
#  define VM_CTZ32(x)  vm_ctz32_fallback((uint32_t)(x))
#  define VM_CTZ64(x)  vm_ctz64_fallback((uint64_t)(x))
#endif

/* --- POPCNT (population count) ------------------------------------------ */
#if defined(__GNUC__) || defined(__clang__)
#  define VM_POPCNT32(x) ((uint32_t)__builtin_popcount((uint32_t)(x)))
#  define VM_POPCNT64(x) ((uint32_t)__builtin_popcountll((uint64_t)(x)))
#elif defined(_MSC_VER)
#  include <intrin.h>
#  define VM_POPCNT32(x) ((uint32_t)__popcnt((unsigned int)(x)))
#  define VM_POPCNT64(x) ((uint32_t)__popcnt64((unsigned __int64)(x)))
#else
static uint32_t vm_popcnt32_fallback(uint32_t x) {
    x -= (x >> 1) & 0x55555555u;
    x  = (x & 0x33333333u) + ((x >> 2) & 0x33333333u);
    return (uint32_t)(((x + (x >> 4)) & 0x0F0F0F0Fu) * 0x01010101u) >> 24;
}
#  define VM_POPCNT32(x) vm_popcnt32_fallback((uint32_t)(x))
#  define VM_POPCNT64(x) (VM_POPCNT32((uint32_t)(x)) + VM_POPCNT32((uint32_t)((uint64_t)(x) >> 32)))
#endif

/* --- MULH_I64 / MULH_U64 (128-bit multiply, high half) ------------------- */
#if defined(__GNUC__) || defined(__clang__)
static int64_t  vm_mulh_i64(int64_t  a, int64_t  b) { return (int64_t) ((__int128) a * (__int128) b >> 64); }
static uint64_t vm_mulh_u64(uint64_t a, uint64_t b) { return (uint64_t)((unsigned __int128)a * (unsigned __int128)b >> 64); }
#elif defined(_MSC_VER) && defined(_M_X64)
#  include <intrin.h>
static int64_t  vm_mulh_i64(int64_t  a, int64_t  b) { return __mulh(a, b); }
static uint64_t vm_mulh_u64(uint64_t a, uint64_t b) { return __umulh(a, b); }
#else
/* Portable fallback using 32-bit halves */
static uint64_t vm_mulh_u64(uint64_t a, uint64_t b) {
    uint32_t ah = (uint32_t)(a >> 32), al = (uint32_t)a;
    uint32_t bh = (uint32_t)(b >> 32), bl = (uint32_t)b;
    uint64_t hi = (uint64_t)ah * bh;
    uint64_t m0 = (uint64_t)al * bh;
    uint64_t m1 = (uint64_t)ah * bl;
    uint64_t lo = (uint64_t)al * bl;
    uint64_t carry = ((lo >> 32) + (uint32_t)m0 + (uint32_t)m1) >> 32;
    return hi + (m0 >> 32) + (m1 >> 32) + carry;
}
static int64_t vm_mulh_i64(int64_t a, int64_t b) {
    uint64_t ua = (uint64_t)a, ub = (uint64_t)b;
    uint64_t r = vm_mulh_u64(ua, ub);
    if (a < 0) r -= (uint64_t)b;
    if (b < 0) r -= (uint64_t)a;
    return (int64_t)r;
}
#endif

/* =========================================================================
 * Little-endian bytecode readers
 *
 * All operand decoding goes through these helpers so the VM operates
 * correctly regardless of host endianness.
 * ====================================================================== */

static inline uint8_t  read8LE  (const uint8_t* m) { return m[0]; }
static inline int8_t   read8SLE (const uint8_t* m) { return (int8_t)m[0]; }

static inline uint16_t read16LE(const uint8_t* m)
{
    return (uint16_t)(m[0]) | ((uint16_t)(m[1]) << 8);
}
static inline int16_t  read16SLE(const uint8_t* m) { return (int16_t)read16LE(m); }

static inline uint32_t read32LE(const uint8_t* m)
{
    return (uint32_t)read16LE(m) | ((uint32_t)read16LE(m + 2) << 16);
}
static inline int32_t  read32SLE(const uint8_t* m) { return (int32_t)read32LE(m); }

static inline uint64_t read64LE(const uint8_t* m)
{
    return (uint64_t)read32LE(m) | ((uint64_t)read32LE(m + 4) << 32);
}
static inline int64_t  read64SLE(const uint8_t* m) { return (int64_t)read64LE(m); }

/* =========================================================================
 * Bounds / register / execution exit helpers
 * ====================================================================== */

#if defined(VM_DEBUG)
#define VM_EXIT_ERR(err_code) \
    do { \
        ctx->pc = pc; \
        ctx->flags &= ~VM_FLAG_RUNNING; \
        ctx->flags |= VM_FLAG_HALTED; \
        if (ctx->debug_hook) { \
            ctx->debug_hook(ctx, VM_DEBUG_EVENT_ERROR, pc, op); \
        } \
        return (err_code); \
    } while (0)
#else
#define VM_EXIT_ERR(err_code) \
    do { \
        ctx->pc = pc; \
        ctx->flags &= ~VM_FLAG_RUNNING; \
        ctx->flags |= VM_FLAG_HALTED; \
        return (err_code); \
    } while (0)
#endif

#define CHECK_BOUNDS(needed) \
    do { \
        if ((uint64_t)(pc) + (uint64_t)(needed) > (uint64_t)(bytecode_size)) \
            VM_EXIT_ERR(VM_ERR_OUT_OF_BOUNDS); \
    } while (0)

#define CHECK_REG(r) \
    do { \
        if ((uint32_t)(r) >= reg_count) \
            VM_EXIT_ERR(VM_ERR_INVALID_REGISTER); \
    } while (0)

#define BRANCH_TARGET(next_pc, offset, out_pc) \
    do { \
        const int64_t _t = (int64_t)(next_pc) + (int64_t)(offset); \
        if (_t < 0 || (uint64_t)_t > (uint64_t)(bytecode_size)) \
            VM_EXIT_ERR(VM_ERR_OUT_OF_BOUNDS); \
        (out_pc) = (uint32_t)_t; \
    } while (0)

/* =========================================================================
 * Debug trace
 *
 * vm_opname() maps each opcode to a printable string.  Only compiled in
 * when VM_DEBUG is defined; vm_execute() references it inside the same
 * preprocessor guard so no dead-code warning is generated.
 * ====================================================================== */

#if defined(VM_DEBUG)
static const char* vm_opname(uint16_t op)
{
    switch (op) {
    case OP_NOP:          return "NOP";
    case OP_MOVE:         return "MOVE";
    case OP_CONST_I8:     return "CONST_I8";
    case OP_CONST_I16:    return "CONST_I16";
    case OP_CONST_I32:    return "CONST_I32";
    case OP_CONST_I64:    return "CONST_I64";
    case OP_CONST_F32:    return "CONST_F32";
    case OP_CONST_F64:    return "CONST_F64";
    case OP_ADD_I32:      return "ADD_I32";
    case OP_SUB_I32:      return "SUB_I32";
    case OP_MUL_I32:      return "MUL_I32";
    case OP_DIV_I32:      return "DIV_I32";
    case OP_REM_I32:      return "REM_I32";
    case OP_ADD_I64:      return "ADD_I64";
    case OP_SUB_I64:      return "SUB_I64";
    case OP_MUL_I64:      return "MUL_I64";
    case OP_DIV_I64:      return "DIV_I64";
    case OP_REM_I64:      return "REM_I64";
    case OP_DIV_U32:      return "DIV_U32";
    case OP_REM_U32:      return "REM_U32";
    case OP_DIV_U64:      return "DIV_U64";
    case OP_REM_U64:      return "REM_U64";
    case OP_ADD_F32:      return "ADD_F32";
    case OP_SUB_F32:      return "SUB_F32";
    case OP_MUL_F32:      return "MUL_F32";
    case OP_DIV_F32:      return "DIV_F32";
    case OP_ADD_F64:      return "ADD_F64";
    case OP_SUB_F64:      return "SUB_F64";
    case OP_MUL_F64:      return "MUL_F64";
    case OP_DIV_F64:      return "DIV_F64";
    case OP_NEG_I32:      return "NEG_I32";
    case OP_NEG_I64:      return "NEG_I64";
    case OP_NEG_F32:      return "NEG_F32";
    case OP_NEG_F64:      return "NEG_F64";
    case OP_NOT_I32:      return "NOT_I32";
    case OP_NOT_I64:      return "NOT_I64";
    case OP_AND_I32:      return "AND_I32";
    case OP_OR_I32:       return "OR_I32";
    case OP_XOR_I32:      return "XOR_I32";
    case OP_AND_I64:      return "AND_I64";
    case OP_OR_I64:       return "OR_I64";
    case OP_XOR_I64:      return "XOR_I64";
    case OP_SHL_I32:      return "SHL_I32";
    case OP_SHR_I32:      return "SHR_I32";
    case OP_USHR_I32:     return "USHR_I32";
    case OP_SHL_I64:      return "SHL_I64";
    case OP_SHR_I64:      return "SHR_I64";
    case OP_USHR_I64:     return "USHR_I64";
    case OP_CMP_I32:      return "CMP_I32";
    case OP_CMP_I64:      return "CMP_I64";
    case OP_CMP_F32:      return "CMP_F32";
    case OP_CMP_F64:      return "CMP_F64";
    case OP_CMP_F32_GT:   return "CMP_F32_GT";
    case OP_I32_TO_I8:    return "I32_TO_I8";
    case OP_I32_TO_I16:   return "I32_TO_I16";
    case OP_I32_TO_I64:   return "I32_TO_I64";
    case OP_I32_TO_F32:   return "I32_TO_F32";
    case OP_I32_TO_F64:   return "I32_TO_F64";
    case OP_I64_TO_I32:   return "I64_TO_I32";
    case OP_I64_TO_F32:   return "I64_TO_F32";
    case OP_I64_TO_F64:   return "I64_TO_F64";
    case OP_F32_TO_I32:   return "F32_TO_I32";
    case OP_F32_TO_I64:   return "F32_TO_I64";
    case OP_F32_TO_F64:   return "F32_TO_F64";
    case OP_F64_TO_I32:   return "F64_TO_I32";
    case OP_F64_TO_I64:   return "F64_TO_I64";
    case OP_F64_TO_F32:   return "F64_TO_F32";
    case OP_LOAD8:        return "LOAD8";
    case OP_LOAD8S:       return "LOAD8S";
    case OP_LOAD16:       return "LOAD16";
    case OP_LOAD16S:      return "LOAD16S";
    case OP_LOAD32:       return "LOAD32";
    case OP_LOAD32S:      return "LOAD32S";
    case OP_LOAD64:       return "LOAD64";
    case OP_LOAD_PTR:     return "LOAD_PTR";
    case OP_STORE8:       return "STORE8";
    case OP_STORE16:      return "STORE16";
    case OP_STORE32:      return "STORE32";
    case OP_STORE64:      return "STORE64";
    case OP_STORE_PTR:    return "STORE_PTR";
    case OP_LEA:          return "LEA";
    case OP_GOTO:         return "GOTO";
    case OP_GOTO_16:      return "GOTO_16";
    case OP_GOTO_32:      return "GOTO_32";
    case OP_IF_EQ:        return "IF_EQ";
    case OP_IF_NE:        return "IF_NE";
    case OP_IF_LT:        return "IF_LT";
    case OP_IF_GE:        return "IF_GE";
    case OP_IF_GT:        return "IF_GT";
    case OP_IF_LE:        return "IF_LE";
    case OP_IF_EQZ:       return "IF_EQZ";
    case OP_IF_NEZ:       return "IF_NEZ";
    case OP_IF_LTZ:       return "IF_LTZ";
    case OP_IF_GEZ:       return "IF_GEZ";
    case OP_IF_GTZ:       return "IF_GTZ";
    case OP_IF_LEZ:       return "IF_LEZ";
    case OP_CALL:         return "CALL";
    case OP_CALL_VOID:    return "CALL_VOID";
    case OP_RETURN_VOID:  return "RETURN_VOID";
    case OP_RETURN:       return "RETURN";
    case OP_CALL_BC:      return "CALL_BC";
    case OP_RET:          return "RET";
    case OP_CMP_U32:      return "CMP_U32";
    case OP_CMP_U64:      return "CMP_U64";
    case OP_IF_ULT:       return "IF_ULT";
    case OP_IF_UGE:       return "IF_UGE";
    case OP_IF_UGT:       return "IF_UGT";
    case OP_IF_ULE:       return "IF_ULE";
    case OP_SELECT:       return "SELECT";
    case OP_CLZ_I32:      return "CLZ_I32";
    case OP_CLZ_I64:      return "CLZ_I64";
    case OP_CTZ_I32:      return "CTZ_I32";
    case OP_CTZ_I64:      return "CTZ_I64";
    case OP_POPCNT_I32:   return "POPCNT_I32";
    case OP_POPCNT_I64:   return "POPCNT_I64";
    case OP_ROTL_I32:     return "ROTL_I32";
    case OP_ROTR_I32:     return "ROTR_I32";
    case OP_ROTL_I64:     return "ROTL_I64";
    case OP_ROTR_I64:     return "ROTR_I64";
    case OP_ABS_I32:      return "ABS_I32";
    case OP_ABS_I64:      return "ABS_I64";
    case OP_MIN_I32:      return "MIN_I32";
    case OP_MAX_I32:      return "MAX_I32";
    case OP_MIN_U32:      return "MIN_U32";
    case OP_MAX_U32:      return "MAX_U32";
    case OP_MIN_I64:      return "MIN_I64";
    case OP_MAX_I64:      return "MAX_I64";
    case OP_MIN_U64:      return "MIN_U64";
    case OP_MAX_U64:      return "MAX_U64";
    case OP_MULH_I32:     return "MULH_I32";
    case OP_MULH_U32:     return "MULH_U32";
    case OP_MULH_I64:     return "MULH_I64";
    case OP_MULH_U64:     return "MULH_U64";
    case OP_BOOL_I32:     return "BOOL_I32";
    case OP_BOOL_I64:     return "BOOL_I64";
    case OP_ABS_F32:      return "ABS_F32";
    case OP_ABS_F64:      return "ABS_F64";
    case OP_SQRT_F32:     return "SQRT_F32";
    case OP_SQRT_F64:     return "SQRT_F64";
    case OP_FLOOR_F32:    return "FLOOR_F32";
    case OP_FLOOR_F64:    return "FLOOR_F64";
    case OP_CEIL_F32:     return "CEIL_F32";
    case OP_CEIL_F64:     return "CEIL_F64";
    case OP_TRUNC_F32:    return "TRUNC_F32";
    case OP_TRUNC_F64:    return "TRUNC_F64";
    case OP_ROUND_F32:    return "ROUND_F32";
    case OP_ROUND_F64:    return "ROUND_F64";
    case OP_MIN_F32:      return "MIN_F32";
    case OP_MAX_F32:      return "MAX_F32";
    case OP_MIN_F64:      return "MIN_F64";
    case OP_MAX_F64:      return "MAX_F64";
    case OP_COPYSIGN_F32: return "COPYSIGN_F32";
    case OP_COPYSIGN_F64: return "COPYSIGN_F64";
    case OP_LOAD8_OFF:    return "LOAD8_OFF";
    case OP_LOAD8S_OFF:   return "LOAD8S_OFF";
    case OP_LOAD16_OFF:   return "LOAD16_OFF";
    case OP_LOAD16S_OFF:  return "LOAD16S_OFF";
    case OP_LOAD32_OFF:   return "LOAD32_OFF";
    case OP_LOAD32S_OFF:  return "LOAD32S_OFF";
    case OP_LOAD64_OFF:   return "LOAD64_OFF";
    case OP_LOAD_PTR_OFF: return "LOAD_PTR_OFF";
    case OP_STORE8_OFF:   return "STORE8_OFF";
    case OP_STORE16_OFF:  return "STORE16_OFF";
    case OP_STORE32_OFF:  return "STORE32_OFF";
    case OP_STORE64_OFF:  return "STORE64_OFF";
    case OP_STORE_PTR_OFF:return "STORE_PTR_OFF";
    case OP_LEA_REG:      return "LEA_REG";
    case OP_MEMCPY:       return "MEMCPY";
    case OP_MEMSET:       return "MEMSET";
    case OP_SWITCH:       return "SWITCH";
    default:              return "??";
    }
}
#endif

/* =========================================================================
 * Dynamic Storage Helpers
 * ====================================================================== */

static VMError vm_ensure_registers(VMContext* ctx, uint32_t needed)
{
    if (needed <= ctx->register_capacity) return VM_OK;
    uint32_t cap = (ctx->register_capacity == 0) ? 32 : ctx->register_capacity;
    while (cap < needed) {
        cap *= 2;
    }
    VMRegister* new_buf = (VMRegister*)realloc(ctx->registers, cap * sizeof(VMRegister));
    if (!new_buf) return VM_ERR_OUT_OF_BOUNDS;
    memset(new_buf + ctx->register_capacity, 0, (cap - ctx->register_capacity) * sizeof(VMRegister));
    ctx->registers = new_buf;
    ctx->register_capacity = cap;
    return VM_OK;
}

static VMError vm_ensure_frames(VMContext* ctx, uint32_t needed)
{
    if (needed <= ctx->frame_capacity) return VM_OK;
    uint32_t cap = (ctx->frame_capacity == 0) ? 8 : ctx->frame_capacity;
    while (cap < needed) {
        cap *= 2;
    }
    VMFrame* new_buf = (VMFrame*)realloc(ctx->frames, cap * sizeof(VMFrame));
    if (!new_buf) return VM_ERR_OUT_OF_BOUNDS;
    memset(new_buf + ctx->frame_capacity, 0, (cap - ctx->frame_capacity) * sizeof(VMFrame));
    ctx->frames = new_buf;
    ctx->frame_capacity = cap;
    return VM_OK;
}

static VMError vm_ensure_native_funcs(VMContext* ctx, uint32_t needed)
{
    if (needed <= ctx->native_capacity) return VM_OK;
    uint32_t cap = (ctx->native_capacity == 0) ? 16 : ctx->native_capacity;
    while (cap < needed) {
        cap *= 2;
    }
    VMNativeFn* new_buf = (VMNativeFn*)realloc(ctx->native_funcs, cap * sizeof(VMNativeFn));
    if (!new_buf) return VM_ERR_BAD_FUNCTION;
    memset(new_buf + ctx->native_capacity, 0, (cap - ctx->native_capacity) * sizeof(VMNativeFn));
    ctx->native_funcs = new_buf;
    ctx->native_capacity = cap;
    return VM_OK;
}

/* =========================================================================
 * API
 * ====================================================================== */

void vm_init(VMContext* ctx)
{
    if (!ctx) return;
    memset(ctx, 0, sizeof(VMContext));
}

void vm_cleanup(VMContext* ctx)
{
    if (!ctx) return;
    if (ctx->registers)    { free(ctx->registers);    ctx->registers = NULL; }
    if (ctx->frames)       { free(ctx->frames);       ctx->frames = NULL; }
    if (ctx->native_funcs) { free(ctx->native_funcs); ctx->native_funcs = NULL; }
    ctx->register_capacity = 0;
    ctx->register_count    = 0;
    ctx->frame_capacity    = 0;
    ctx->call_depth        = 0;
    ctx->native_capacity   = 0;
    ctx->native_count      = 0;
}

void vm_destroy(VMContext* ctx)
{
    vm_cleanup(ctx);
}

VMError vm_register_function(VMContext* ctx, uint32_t id, VMNativeFn fn)
{
    if (id >= VM_MAX_NATIVE_FUNCS)
        return VM_ERR_BAD_FUNCTION;
    if (vm_ensure_native_funcs(ctx, id + 1) != VM_OK)
        return VM_ERR_BAD_FUNCTION;
    ctx->native_funcs[id] = fn;
    if (id >= ctx->native_count)
        ctx->native_count = id + 1;
    return VM_OK;
}

/* =========================================================================
 * vm_execute — main dispatch loop
 * ====================================================================== */

VMError vm_execute(
    VMContext*     ctx,
    VMRegister*    host_regs,
    uint32_t       reg_count,
    uint32_t       pc,
    const uint8_t* bytecode,
    uint32_t       bytecode_size)
{
    /* Decide whether to resume from a previous pause or start fresh.
     * VM_FLAG_RESUME is set automatically by the VM whenever execution is
     * paused (breakpoint or single-step).  When set, ctx->pc takes priority
     * over the caller-supplied pc argument, and the flag is consumed here.
     * A caller that wants to restart from an arbitrary address while the VM
     * is paused should clear VM_FLAG_RESUME before calling vm_execute(). */
    if (ctx->flags & VM_FLAG_RESUME) {
        pc = ctx->pc;
        ctx->flags &= ~VM_FLAG_RESUME;
    } else {
        ctx->pc = pc;
    }

    const int single_step = (ctx->flags & VM_FLAG_SINGLE_STEP) != 0;
    uint16_t op = 0;

    ctx->flags |= VM_FLAG_RUNNING;
    ctx->flags &= ~(VM_FLAG_PAUSED | VM_FLAG_HALTED);

    /* Initialize register windowing for execution */
    uint32_t top_reg_count = (reg_count > 0) ? reg_count : 16;
    uint32_t reg_base = 0;
    if (ctx->call_depth > 0 && ctx->frames) {
        reg_base = ctx->frames[ctx->call_depth - 1].reg_base + ctx->frames[ctx->call_depth - 1].reg_count;
    }

    if (vm_ensure_registers(ctx, reg_base + top_reg_count) != VM_OK) {
        return VM_ERR_OUT_OF_BOUNDS;
    }

    if (host_regs && reg_count > 0 && ctx->call_depth == 0) {
        if (!(ctx->flags & VM_FLAG_PAUSED)) {
            memcpy(ctx->registers, host_regs, reg_count * sizeof(VMRegister));
        }
    }

    VMRegister* cur_regs = ctx->registers + reg_base;
#define regs cur_regs

    while (pc < bytecode_size) {
        ctx->pc = pc;

        /* ----------------------------------------------------------------
         * Read the 16-bit opcode.
         * ------------------------------------------------------------ */
        CHECK_BOUNDS(2);
        op = read16LE(bytecode + pc);

#if defined(VM_DEBUG)
        const uint32_t instr_pc = pc;

        /* Profiler accounting */
        if (ctx->profiler_enabled && op < VM_OPCODE_COUNT) {
            ctx->opcode_counts[op]++;
            ctx->total_instructions++;
        }

        /* Breakpoint check — only trigger if not currently single-stepping */
        if (!single_step && vm_has_breakpoint(ctx, instr_pc)) {
            if (ctx->debug_hook) {
                ctx->debug_hook(ctx, VM_DEBUG_EVENT_BREAKPOINT, instr_pc, op);
            }
            ctx->pc = instr_pc;
            ctx->flags &= ~VM_FLAG_RUNNING;
            ctx->flags |= VM_FLAG_PAUSED | VM_FLAG_RESUME;
            if (host_regs && reg_count > 0 && ctx->call_depth == 0) {
                memcpy(host_regs, ctx->registers, reg_count * sizeof(VMRegister));
            }
            return VM_OK;
        }

        /* Debug step hook */
        if (ctx->debug_hook) {
            ctx->debug_hook(ctx, VM_DEBUG_EVENT_STEP, instr_pc, op);
        }

        if (ctx->debug)
            fprintf(stderr, "PC=0x%04X  OP=%s\n", instr_pc, vm_opname(op));
#endif

        pc += 2;

        /* ----------------------------------------------------------------
         * Dispatch
         * ------------------------------------------------------------ */
        switch (op) {

        /* ============================================================== */
        /* NOP                                                             */
        /* ============================================================== */
        case OP_NOP:
            break;

        /* ============================================================== */
        /* MOVE [dst:u8][src:u16]                                          */
        /* ============================================================== */
        case OP_MOVE: {
            CHECK_BOUNDS(3);
            const uint8_t  dst = read8LE (bytecode + pc);
            const uint16_t src = read16LE(bytecode + pc + 1);
            CHECK_REG(dst);
            CHECK_REG(src);
            pc += 3;
            regs[dst] = regs[src];
        } break;

        /* ============================================================== */
        /* Integer constants                                               */
        /*                                                                 */
        /* All integer constants are sign-extended into the full 64-bit   */
        /* register so that every integer field (i8..i64, u8..u64) has a  */
        /* sensible value immediately after the instruction.              */
        /* ============================================================== */

        case OP_CONST_I8: {
            CHECK_BOUNDS(2);
            const uint8_t dst = read8LE (bytecode + pc);
            const int8_t  val = read8SLE(bytecode + pc + 1);
            CHECK_REG(dst);
            pc += 2;
            regs[dst].i64 = (int64_t)val;
        } break;

        case OP_CONST_I16: {
            CHECK_BOUNDS(3);
            const uint8_t dst = read8LE  (bytecode + pc);
            const int16_t val = read16SLE(bytecode + pc + 1);
            CHECK_REG(dst);
            pc += 3;
            regs[dst].i64 = (int64_t)val;
        } break;

        case OP_CONST_I32: {
            CHECK_BOUNDS(5);
            const uint8_t dst = read8LE  (bytecode + pc);
            const int32_t val = read32SLE(bytecode + pc + 1);
            CHECK_REG(dst);
            pc += 5;
            regs[dst].i64 = (int64_t)val;
        } break;

        case OP_CONST_I64: {
            CHECK_BOUNDS(9);
            const uint8_t dst = read8LE  (bytecode + pc);
            const int64_t val = read64SLE(bytecode + pc + 1);
            CHECK_REG(dst);
            pc += 9;
            regs[dst].i64 = val;
        } break;

        /* ============================================================== */
        /* Float constants                                                 */
        /*                                                                 */
        /* Operand is the IEEE-754 bit pattern encoded as an unsigned      */
        /* integer.  memcpy is used to avoid strict-aliasing issues.       */
        /* ============================================================== */

        case OP_CONST_F32: {
            CHECK_BOUNDS(5);
            const uint8_t  dst  = read8LE (bytecode + pc);
            const uint32_t bits = read32LE(bytecode + pc + 1);
            CHECK_REG(dst);
            pc += 5;
            regs[dst].u64 = 0;
            memcpy(&regs[dst].f32, &bits, sizeof(float));
        } break;

        case OP_CONST_F64: {
            CHECK_BOUNDS(9);
            const uint8_t  dst  = read8LE (bytecode + pc);
            const uint64_t bits = read64LE(bytecode + pc + 1);
            CHECK_REG(dst);
            pc += 9;
            memcpy(&regs[dst].f64, &bits, sizeof(double));
        } break;

        /* ============================================================== */
        /* Integer arithmetic – i32 / u32                                  */
        /*                                                                 */
        /* DIV / REM guard against both division by zero and the           */
        /* INT32_MIN / -1 case which is undefined behaviour in C.          */
        /* ============================================================== */

        case OP_ADD_I32:
        case OP_SUB_I32:
        case OP_MUL_I32:
        case OP_DIV_I32:
        case OP_REM_I32:
        case OP_DIV_U32:
        case OP_REM_U32: {
            CHECK_BOUNDS(3);
            const uint8_t dst = read8LE(bytecode + pc);
            const uint8_t lhs = read8LE(bytecode + pc + 1);
            const uint8_t rhs = read8LE(bytecode + pc + 2);
            CHECK_REG(dst); CHECK_REG(lhs); CHECK_REG(rhs);
            pc += 3;
            switch (op) {
            case OP_ADD_I32: regs[dst].i32 = regs[lhs].i32 + regs[rhs].i32; break;
            case OP_SUB_I32: regs[dst].i32 = regs[lhs].i32 - regs[rhs].i32; break;
            case OP_MUL_I32: regs[dst].i32 = regs[lhs].i32 * regs[rhs].i32; break;
            case OP_DIV_I32: {
                const int32_t a = regs[lhs].i32, b = regs[rhs].i32;
                if (b == 0) VM_EXIT_ERR(VM_ERR_DIV_ZERO);
                regs[dst].i32 = (a == INT32_MIN && b == -1) ? INT32_MIN : a / b;
                break;
            }
            case OP_REM_I32: {
                const int32_t a = regs[lhs].i32, b = regs[rhs].i32;
                if (b == 0) VM_EXIT_ERR(VM_ERR_DIV_ZERO);
                regs[dst].i32 = (a == INT32_MIN && b == -1) ? 0 : a % b;
                break;
            }
            case OP_DIV_U32:
                if (regs[rhs].u32 == 0) VM_EXIT_ERR(VM_ERR_DIV_ZERO);
                regs[dst].u32 = regs[lhs].u32 / regs[rhs].u32;
                break;
            case OP_REM_U32:
                if (regs[rhs].u32 == 0) VM_EXIT_ERR(VM_ERR_DIV_ZERO);
                regs[dst].u32 = regs[lhs].u32 % regs[rhs].u32;
                break;
            default: break;
            }
        } break;

        /* ============================================================== */
        /* Integer arithmetic – i64 / u64                                  */
        /* ============================================================== */

        case OP_ADD_I64:
        case OP_SUB_I64:
        case OP_MUL_I64:
        case OP_DIV_I64:
        case OP_REM_I64:
        case OP_DIV_U64:
        case OP_REM_U64: {
            CHECK_BOUNDS(3);
            const uint8_t dst = read8LE(bytecode + pc);
            const uint8_t lhs = read8LE(bytecode + pc + 1);
            const uint8_t rhs = read8LE(bytecode + pc + 2);
            CHECK_REG(dst); CHECK_REG(lhs); CHECK_REG(rhs);
            pc += 3;
            switch (op) {
            case OP_ADD_I64: regs[dst].i64 = regs[lhs].i64 + regs[rhs].i64; break;
            case OP_SUB_I64: regs[dst].i64 = regs[lhs].i64 - regs[rhs].i64; break;
            case OP_MUL_I64: regs[dst].i64 = regs[lhs].i64 * regs[rhs].i64; break;
            case OP_DIV_I64: {
                const int64_t a = regs[lhs].i64, b = regs[rhs].i64;
                if (b == 0) VM_EXIT_ERR(VM_ERR_DIV_ZERO);
                regs[dst].i64 = (a == INT64_MIN && b == (int64_t)(-1)) ? INT64_MIN : a / b;
                break;
            }
            case OP_REM_I64: {
                const int64_t a = regs[lhs].i64, b = regs[rhs].i64;
                if (b == 0) VM_EXIT_ERR(VM_ERR_DIV_ZERO);
                regs[dst].i64 = (a == INT64_MIN && b == (int64_t)(-1)) ? 0 : a % b;
                break;
            }
            case OP_DIV_U64:
                if (regs[rhs].u64 == 0) VM_EXIT_ERR(VM_ERR_DIV_ZERO);
                regs[dst].u64 = regs[lhs].u64 / regs[rhs].u64;
                break;
            case OP_REM_U64:
                if (regs[rhs].u64 == 0) VM_EXIT_ERR(VM_ERR_DIV_ZERO);
                regs[dst].u64 = regs[lhs].u64 % regs[rhs].u64;
                break;
            default: break;
            }
        } break;

        /* ============================================================== */
        /* Float arithmetic – f32                                          */
        /*                                                                 */
        /* IEEE-754 NaN and infinity propagate naturally through all       */
        /* float operations; no special casing is required.                */
        /* ============================================================== */

        case OP_ADD_F32:
        case OP_SUB_F32:
        case OP_MUL_F32:
        case OP_DIV_F32: {
            CHECK_BOUNDS(3);
            const uint8_t dst = read8LE(bytecode + pc);
            const uint8_t lhs = read8LE(bytecode + pc + 1);
            const uint8_t rhs = read8LE(bytecode + pc + 2);
            CHECK_REG(dst); CHECK_REG(lhs); CHECK_REG(rhs);
            pc += 3;
            const float a = regs[lhs].f32;
            const float b = regs[rhs].f32;
            switch (op) {
            case OP_ADD_F32: regs[dst].f32 = a + b; break;
            case OP_SUB_F32: regs[dst].f32 = a - b; break;
            case OP_MUL_F32: regs[dst].f32 = a * b; break;
            case OP_DIV_F32: regs[dst].f32 = a / b; break;
            default: break;
            }
        } break;

        /* ============================================================== */
        /* Float arithmetic – f64                                          */
        /* ============================================================== */

        case OP_ADD_F64:
        case OP_SUB_F64:
        case OP_MUL_F64:
        case OP_DIV_F64: {
            CHECK_BOUNDS(3);
            const uint8_t dst = read8LE(bytecode + pc);
            const uint8_t lhs = read8LE(bytecode + pc + 1);
            const uint8_t rhs = read8LE(bytecode + pc + 2);
            CHECK_REG(dst); CHECK_REG(lhs); CHECK_REG(rhs);
            pc += 3;
            const double a = regs[lhs].f64;
            const double b = regs[rhs].f64;
            switch (op) {
            case OP_ADD_F64: regs[dst].f64 = a + b; break;
            case OP_SUB_F64: regs[dst].f64 = a - b; break;
            case OP_MUL_F64: regs[dst].f64 = a * b; break;
            case OP_DIV_F64: regs[dst].f64 = a / b; break;
            default: break;
            }
        } break;

        /* ============================================================== */
        /* Unary  [dst:u8][src:u8]                                         */
        /* ============================================================== */

        case OP_NEG_I32:
        case OP_NEG_I64:
        case OP_NEG_F32:
        case OP_NEG_F64:
        case OP_NOT_I32:
        case OP_NOT_I64: {
            CHECK_BOUNDS(2);
            const uint8_t dst = read8LE(bytecode + pc);
            const uint8_t src = read8LE(bytecode + pc + 1);
            CHECK_REG(dst); CHECK_REG(src);
            pc += 2;
            switch (op) {
            case OP_NEG_I32: regs[dst].i32 = -regs[src].i32; break;
            case OP_NEG_I64: regs[dst].i64 = -regs[src].i64; break;
            case OP_NEG_F32: regs[dst].f32 = -regs[src].f32; break;
            case OP_NEG_F64: regs[dst].f64 = -regs[src].f64; break;
            case OP_NOT_I32: regs[dst].i32 = ~regs[src].i32; break;
            case OP_NOT_I64: regs[dst].i64 = ~regs[src].i64; break;
            default: break;
            }
        } break;

        /* ============================================================== */
        /* Bitwise – i32  [dst:u8][lhs:u8][rhs:u8]                        */
        /* ============================================================== */

        case OP_AND_I32:
        case OP_OR_I32:
        case OP_XOR_I32: {
            CHECK_BOUNDS(3);
            const uint8_t dst = read8LE(bytecode + pc);
            const uint8_t lhs = read8LE(bytecode + pc + 1);
            const uint8_t rhs = read8LE(bytecode + pc + 2);
            CHECK_REG(dst); CHECK_REG(lhs); CHECK_REG(rhs);
            pc += 3;
            switch (op) {
            case OP_AND_I32: regs[dst].i32 = regs[lhs].i32 & regs[rhs].i32; break;
            case OP_OR_I32:  regs[dst].i32 = regs[lhs].i32 | regs[rhs].i32; break;
            case OP_XOR_I32: regs[dst].i32 = regs[lhs].i32 ^ regs[rhs].i32; break;
            default: break;
            }
        } break;

        /* ============================================================== */
        /* Bitwise – i64  [dst:u8][lhs:u8][rhs:u8]                        */
        /* ============================================================== */

        case OP_AND_I64:
        case OP_OR_I64:
        case OP_XOR_I64: {
            CHECK_BOUNDS(3);
            const uint8_t dst = read8LE(bytecode + pc);
            const uint8_t lhs = read8LE(bytecode + pc + 1);
            const uint8_t rhs = read8LE(bytecode + pc + 2);
            CHECK_REG(dst); CHECK_REG(lhs); CHECK_REG(rhs);
            pc += 3;
            switch (op) {
            case OP_AND_I64: regs[dst].i64 = regs[lhs].i64 & regs[rhs].i64; break;
            case OP_OR_I64:  regs[dst].i64 = regs[lhs].i64 | regs[rhs].i64; break;
            case OP_XOR_I64: regs[dst].i64 = regs[lhs].i64 ^ regs[rhs].i64; break;
            default: break;
            }
        } break;

        /* ============================================================== */
        /* Shifts – i32  [dst:u8][val:u8][amt:u8]                          */
        /*                                                                 */
        /* Shift amount is taken from amt.i32 masked to 5 bits (& 31).    */
        /* SHR_I32 performs an arithmetic (sign-preserving) right shift.   */
        /* USHR_I32 performs a logical (zero-filling) right shift.         */
        /* ============================================================== */

        case OP_SHL_I32:
        case OP_SHR_I32:
        case OP_USHR_I32: {
            CHECK_BOUNDS(3);
            const uint8_t dst = read8LE(bytecode + pc);
            const uint8_t val = read8LE(bytecode + pc + 1);
            const uint8_t amt = read8LE(bytecode + pc + 2);
            CHECK_REG(dst); CHECK_REG(val); CHECK_REG(amt);
            pc += 3;
            const uint32_t shift = (uint32_t)regs[amt].i32 & 31u;
            switch (op) {
            case OP_SHL_I32:
                regs[dst].i32 = (int32_t)((uint32_t)regs[val].i32 << shift);
                break;
            case OP_SHR_I32:
                /* Arithmetic right shift; implementation-defined in C99
                 * but behaves as expected on all two's-complement targets. */
                regs[dst].i32 = regs[val].i32 >> (int)shift;
                break;
            case OP_USHR_I32:
                regs[dst].u32 = regs[val].u32 >> shift;
                break;
            default: break;
            }
        } break;

        /* ============================================================== */
        /* Shifts – i64  [dst:u8][val:u8][amt:u8]                          */
        /* Shift amount masked to 6 bits (& 63).                           */
        /* ============================================================== */

        case OP_SHL_I64:
        case OP_SHR_I64:
        case OP_USHR_I64: {
            CHECK_BOUNDS(3);
            const uint8_t dst = read8LE(bytecode + pc);
            const uint8_t val = read8LE(bytecode + pc + 1);
            const uint8_t amt = read8LE(bytecode + pc + 2);
            CHECK_REG(dst); CHECK_REG(val); CHECK_REG(amt);
            pc += 3;
            const uint64_t shift = (uint64_t)regs[amt].i64 & 63u;
            switch (op) {
            case OP_SHL_I64:
                regs[dst].i64 = (int64_t)((uint64_t)regs[val].i64 << shift);
                break;
            case OP_SHR_I64:
                regs[dst].i64 = regs[val].i64 >> (int)shift;
                break;
            case OP_USHR_I64:
                regs[dst].u64 = regs[val].u64 >> shift;
                break;
            default: break;
            }
        } break;

        /* ============================================================== */
        /* Comparisons  [dst:u8][lhs:u8][rhs:u8]                          */
        /* dst.i32 = -1 (less), 0 (equal), +1 (greater)                  */
        /* CMP_F32 / CMP_F64: returns -1 when either operand is NaN.      */
        /* CMP_F32_GT: returns +2 when either operand is NaN.             */
        /* ============================================================== */

        case OP_CMP_I32:
        case OP_CMP_I64:
        case OP_CMP_F32:
        case OP_CMP_F64:
        case OP_CMP_F32_GT: {
            CHECK_BOUNDS(3);
            const uint8_t dst = read8LE(bytecode + pc);
            const uint8_t lhs = read8LE(bytecode + pc + 1);
            const uint8_t rhs = read8LE(bytecode + pc + 2);
            CHECK_REG(dst); CHECK_REG(lhs); CHECK_REG(rhs);
            pc += 3;
            switch (op) {
            case OP_CMP_I32:
                regs[dst].i32 = (regs[lhs].i32 < regs[rhs].i32) ? -1
                              : (regs[lhs].i32 > regs[rhs].i32) ?  1 : 0;
                break;
            case OP_CMP_I64:
                regs[dst].i32 = (regs[lhs].i64 < regs[rhs].i64) ? -1
                              : (regs[lhs].i64 > regs[rhs].i64) ?  1 : 0;
                break;
            case OP_CMP_F32: {
                const float a = regs[lhs].f32, b = regs[rhs].f32;
                regs[dst].i32 = (isnan(a) || isnan(b)) ? -1
                              : (a < b) ? -1 : (a > b) ? 1 : 0;
                break;
            }
            case OP_CMP_F64: {
                const double a = regs[lhs].f64, b = regs[rhs].f64;
                regs[dst].i32 = (isnan(a) || isnan(b)) ? -1
                              : (a < b) ? -1 : (a > b) ? 1 : 0;
                break;
            }
            case OP_CMP_F32_GT: {
                /* Unordered-greater variant: NaN => +2 (Java fcmpg semantics). */
                const float a = regs[lhs].f32, b = regs[rhs].f32;
                regs[dst].i32 = (isnan(a) || isnan(b)) ? 2
                              : (a < b) ? -1 : (a > b) ? 1 : 0;
                break;
            }
            default: break;
            }
        } break;

        /* ============================================================== */
        /* Type conversions  [dst:u8][src:u8]                              */
        /* ============================================================== */

        case OP_I32_TO_I8:
        case OP_I32_TO_I16:
        case OP_I32_TO_I64:
        case OP_I32_TO_F32:
        case OP_I32_TO_F64:
        case OP_I64_TO_I32:
        case OP_I64_TO_F32:
        case OP_I64_TO_F64:
        case OP_F32_TO_I32:
        case OP_F32_TO_I64:
        case OP_F32_TO_F64:
        case OP_F64_TO_I32:
        case OP_F64_TO_I64:
        case OP_F64_TO_F32: {
            CHECK_BOUNDS(2);
            const uint8_t dst = read8LE(bytecode + pc);
            const uint8_t src = read8LE(bytecode + pc + 1);
            CHECK_REG(dst); CHECK_REG(src);
            pc += 2;
            switch (op) {
            case OP_I32_TO_I8:  regs[dst].i64 = (int64_t)(int8_t) regs[src].i32; break;
            case OP_I32_TO_I16: regs[dst].i64 = (int64_t)(int16_t)regs[src].i32; break;
            case OP_I32_TO_I64: regs[dst].i64 = (int64_t)          regs[src].i32; break;
            case OP_I32_TO_F32: regs[dst].f32 = (float)             regs[src].i32; break;
            case OP_I32_TO_F64: regs[dst].f64 = (double)            regs[src].i32; break;
            case OP_I64_TO_I32: regs[dst].i32 = (int32_t)           regs[src].i64; break;
            case OP_I64_TO_F32: regs[dst].f32 = (float)             regs[src].i64; break;
            case OP_I64_TO_F64: regs[dst].f64 = (double)            regs[src].i64; break;
            case OP_F32_TO_I32: regs[dst].i32 = (int32_t)           regs[src].f32; break;
            case OP_F32_TO_I64: regs[dst].i64 = (int64_t)           regs[src].f32; break;
            case OP_F32_TO_F64: regs[dst].f64 = (double)            regs[src].f32; break;
            case OP_F64_TO_I32: regs[dst].i32 = (int32_t)           regs[src].f64; break;
            case OP_F64_TO_I64: regs[dst].i64 = (int64_t)           regs[src].f64; break;
            case OP_F64_TO_F32: regs[dst].f32 = (float)             regs[src].f64; break;
            default: break;
            }
        } break;

        /* ============================================================== */
        /* Memory loads  [dst:u8][addr:u8]                                 */
        /* ============================================================== */

        case OP_LOAD8:
        case OP_LOAD8S:
        case OP_LOAD16:
        case OP_LOAD16S:
        case OP_LOAD32:
        case OP_LOAD32S:
        case OP_LOAD64:
        case OP_LOAD_PTR: {
            CHECK_BOUNDS(2);
            const uint8_t dst  = read8LE(bytecode + pc);
            const uint8_t addr = read8LE(bytecode + pc + 1);
            CHECK_REG(dst); CHECK_REG(addr);
            pc += 2;
            const void* p = regs[addr].ptr;
            regs[dst].u64 = 0;
            switch (op) {
            case OP_LOAD8:    { uint8_t  v; memcpy(&v, p, 1); regs[dst].u64 = (uint64_t)v; } break;
            case OP_LOAD8S:   { int8_t   v; memcpy(&v, p, 1); regs[dst].i64 = (int64_t) v; } break;
            case OP_LOAD16:   { uint16_t v; memcpy(&v, p, 2); regs[dst].u64 = (uint64_t)v; } break;
            case OP_LOAD16S:  { int16_t  v; memcpy(&v, p, 2); regs[dst].i64 = (int64_t) v; } break;
            case OP_LOAD32:   { uint32_t v; memcpy(&v, p, 4); regs[dst].u64 = (uint64_t)v; } break;
            case OP_LOAD32S:  { int32_t  v; memcpy(&v, p, 4); regs[dst].i64 = (int64_t) v; } break;
            case OP_LOAD64:   {              memcpy(&regs[dst].u64, p, 8);                  } break;
            case OP_LOAD_PTR: {              memcpy(&regs[dst].ptr, p, sizeof(void*));      } break;
            default: break;
            }
        } break;

        /* ============================================================== */
        /* Memory stores  [addr:u8][src:u8]                                */
        /* ============================================================== */

        case OP_STORE8:
        case OP_STORE16:
        case OP_STORE32:
        case OP_STORE64:
        case OP_STORE_PTR: {
            CHECK_BOUNDS(2);
            const uint8_t addr = read8LE(bytecode + pc);
            const uint8_t src  = read8LE(bytecode + pc + 1);
            CHECK_REG(addr); CHECK_REG(src);
            pc += 2;
            void* p = regs[addr].ptr;
            switch (op) {
            case OP_STORE8:    { uint8_t  v = regs[src].u8;  memcpy(p, &v, 1);             } break;
            case OP_STORE16:   { uint16_t v = regs[src].u16; memcpy(p, &v, 2);             } break;
            case OP_STORE32:   { uint32_t v = regs[src].u32; memcpy(p, &v, 4);             } break;
            case OP_STORE64:   { uint64_t v = regs[src].u64; memcpy(p, &v, 8);             } break;
            case OP_STORE_PTR: { void*    v = regs[src].ptr; memcpy(p, &v, sizeof(void*)); } break;
            default: break;
            }
        } break;

        /* ============================================================== */
        /* LEA  [dst:u8][base:u8][offset:i32]                              */
        /* ============================================================== */

        case OP_LEA: {
            CHECK_BOUNDS(6);
            const uint8_t dst    = read8LE  (bytecode + pc);
            const uint8_t base   = read8LE  (bytecode + pc + 1);
            const int32_t offset = read32SLE(bytecode + pc + 2);
            CHECK_REG(dst); CHECK_REG(base);
            pc += 6;
            regs[dst].ptr = (uint8_t*)regs[base].ptr + offset;
        } break;

        /* ============================================================== */
        /* Unconditional branches                                          */
        /* ============================================================== */

        case OP_GOTO: {
            CHECK_BOUNDS(1);
            const int8_t offset = read8SLE(bytecode + pc);
            pc += 1;
            BRANCH_TARGET(pc, offset, pc);
        } break;

        case OP_GOTO_16: {
            CHECK_BOUNDS(2);
            const int16_t offset = read16SLE(bytecode + pc);
            pc += 2;
            BRANCH_TARGET(pc, offset, pc);
        } break;

        case OP_GOTO_32: {
            CHECK_BOUNDS(4);
            const int32_t offset = read32SLE(bytecode + pc);
            pc += 4;
            BRANCH_TARGET(pc, offset, pc);
        } break;

        /* ============================================================== */
        /* Conditional branches — i32 register pair                        */
        /* ============================================================== */

        case OP_IF_EQ:
        case OP_IF_NE:
        case OP_IF_LT:
        case OP_IF_GE:
        case OP_IF_GT:
        case OP_IF_LE: {
            CHECK_BOUNDS(4);
            const uint8_t A      = read8LE  (bytecode + pc);
            const uint8_t B      = read8LE  (bytecode + pc + 1);
            const int16_t offset = read16SLE(bytecode + pc + 2);
            CHECK_REG(A); CHECK_REG(B);
            pc += 4;
            const int32_t lhs = regs[A].i32;
            const int32_t rhs = regs[B].i32;
            int taken;
            switch (op) {
            case OP_IF_EQ: taken = (lhs == rhs); break;
            case OP_IF_NE: taken = (lhs != rhs); break;
            case OP_IF_LT: taken = (lhs <  rhs); break;
            case OP_IF_GE: taken = (lhs >= rhs); break;
            case OP_IF_GT: taken = (lhs >  rhs); break;
            case OP_IF_LE: taken = (lhs <= rhs); break;
            default: taken = 0; break;
            }
            if (taken)
                BRANCH_TARGET(pc, offset, pc);
        } break;

        /* ============================================================== */
        /* Conditional branches — i32 vs zero                             */
        /* ============================================================== */

        case OP_IF_EQZ:
        case OP_IF_NEZ:
        case OP_IF_LTZ:
        case OP_IF_GEZ:
        case OP_IF_GTZ:
        case OP_IF_LEZ: {
            CHECK_BOUNDS(3);
            const uint8_t A      = read8LE  (bytecode + pc);
            const int16_t offset = read16SLE(bytecode + pc + 1);
            CHECK_REG(A);
            pc += 3;
            const int32_t value = regs[A].i32;
            int taken;
            switch (op) {
            case OP_IF_EQZ: taken = (value == 0); break;
            case OP_IF_NEZ: taken = (value != 0); break;
            case OP_IF_LTZ: taken = (value <  0); break;
            case OP_IF_GEZ: taken = (value >= 0); break;
            case OP_IF_GTZ: taken = (value >  0); break;
            case OP_IF_LEZ: taken = (value <= 0); break;
            default: taken = 0; break;
            }
            if (taken)
                BRANCH_TARGET(pc, offset, pc);
        } break;

        /* ============================================================== */
        /* CALL  [dst:u8][id:u32][argc:u8][r0:u8]...[rN:u8]               */
        /* ============================================================== */

        case OP_CALL: {
            CHECK_BOUNDS(6);
            const uint8_t  dst  = read8LE (bytecode + pc);
            const uint32_t id   = read32LE(bytecode + pc + 1);
            const uint8_t  argc = read8LE (bytecode + pc + 5);
            CHECK_BOUNDS(6u + (uint32_t)argc);
            CHECK_REG(dst);
            if (argc > VM_MAX_CALL_ARGC)
                VM_EXIT_ERR(VM_ERR_BAD_ARGC);
            if (id >= ctx->native_capacity || !ctx->native_funcs || !ctx->native_funcs[id])
                VM_EXIT_ERR(VM_ERR_BAD_FUNCTION);

            VMRegister args[VM_MAX_CALL_ARGC];
            {
                uint32_t i;
                for (i = 0; i < (uint32_t)argc; i++) {
                    const uint8_t ai = read8LE(bytecode + pc + 6 + i);
                    CHECK_REG(ai);
                    args[i] = cur_regs[ai];
                }
            }
            pc += 6u + (uint32_t)argc;

            VMRegister call_result;
            memset(&call_result, 0, sizeof(call_result));
            {
                const VMError err = ctx->native_funcs[id](ctx, (uint32_t)argc, args, &call_result);
                if (err != VM_OK) VM_EXIT_ERR(err);
            }
            cur_regs[dst] = call_result;
            ctx->result   = call_result;
        } break;

        /* ============================================================== */
        /* CALL_VOID  [id:u32][argc:u8][r0:u8]...[rN:u8]                  */
        /* ============================================================== */

        case OP_CALL_VOID: {
            CHECK_BOUNDS(5);
            const uint32_t id   = read32LE(bytecode + pc);
            const uint8_t  argc = read8LE (bytecode + pc + 4);
            CHECK_BOUNDS(5u + (uint32_t)argc);
            if (argc > VM_MAX_CALL_ARGC)
                VM_EXIT_ERR(VM_ERR_BAD_ARGC);
            if (id >= ctx->native_capacity || !ctx->native_funcs || !ctx->native_funcs[id])
                VM_EXIT_ERR(VM_ERR_BAD_FUNCTION);

            VMRegister args[VM_MAX_CALL_ARGC];
            {
                uint32_t i;
                for (i = 0; i < (uint32_t)argc; i++) {
                    const uint8_t ai = read8LE(bytecode + pc + 5 + i);
                    CHECK_REG(ai);
                    args[i] = cur_regs[ai];
                }
            }
            pc += 5u + (uint32_t)argc;

            VMRegister call_result;
            memset(&call_result, 0, sizeof(call_result));
            {
                const VMError err = ctx->native_funcs[id](ctx, (uint32_t)argc, args, &call_result);
                if (err != VM_OK) VM_EXIT_ERR(err);
            }
            ctx->result = call_result;
        } break;

        /* ============================================================== */
        /* Return                                                          */
        /* ============================================================== */

        case OP_RETURN_VOID:
            ctx->pc = pc;
            ctx->flags &= ~VM_FLAG_RUNNING;
            ctx->flags |= VM_FLAG_HALTED;
            if (host_regs && reg_count > 0 && ctx->call_depth == 0) {
                memcpy(host_regs, ctx->registers, reg_count * sizeof(VMRegister));
            }
            return VM_OK;

        case OP_RETURN: {
            CHECK_BOUNDS(1);
            const uint8_t src = read8LE(bytecode + pc);
            CHECK_REG(src);
            ctx->result = cur_regs[src];
            ctx->pc = pc;
            ctx->flags &= ~VM_FLAG_RUNNING;
            ctx->flags |= VM_FLAG_HALTED;
            if (host_regs && reg_count > 0 && ctx->call_depth == 0) {
                memcpy(host_regs, ctx->registers, reg_count * sizeof(VMRegister));
            }
            return VM_OK;
        }

        /* ============================================================== */
        /* OP_CALL_BC  [dst:u8][target:u32][argc:u8][arg0..N:u8]          */
        /* ============================================================== */

        case OP_CALL_BC: {
            CHECK_BOUNDS(6);
            const uint8_t  dst       = read8LE (bytecode + pc);
            const uint32_t target_pc = read32LE(bytecode + pc + 1);
            const uint8_t  argc      = read8LE (bytecode + pc + 5);
            CHECK_BOUNDS(6u + (uint32_t)argc);
            CHECK_REG(dst);
            if (argc > VM_MAX_CALL_ARGC)    VM_EXIT_ERR(VM_ERR_BAD_ARGC);
            if (target_pc >= bytecode_size) VM_EXIT_ERR(VM_ERR_OUT_OF_BOUNDS);

            VMRegister call_args[VM_MAX_CALL_ARGC];
            for (uint32_t i = 0; i < (uint32_t)argc; i++) {
                const uint8_t ai = read8LE(bytecode + pc + 6 + i);
                CHECK_REG(ai);
                call_args[i] = cur_regs[ai];
            }

            uint32_t callee_reg_count = (reg_count > 0) ? reg_count : 16;
            uint32_t callee_base = reg_base + reg_count;

            if (vm_ensure_registers(ctx, callee_base + callee_reg_count) != VM_OK) {
                VM_EXIT_ERR(VM_ERR_OUT_OF_BOUNDS);
            }
            if (vm_ensure_frames(ctx, ctx->call_depth + 1) != VM_OK) {
                VM_EXIT_ERR(VM_ERR_STACK_OVERFLOW);
            }

            /* Push 16-byte metadata VMFrame */
            VMFrame* frame = &ctx->frames[ctx->call_depth];
            frame->return_pc = pc + 6u + (uint32_t)argc;
            frame->reg_base  = reg_base;
            frame->dst_reg   = dst;
            frame->reg_count = (uint16_t)reg_count;

            ctx->call_depth++;

            /* Switch window base & count */
            reg_base  = callee_base;
            reg_count = callee_reg_count;
            cur_regs  = ctx->registers + reg_base;

            memset(cur_regs, 0, callee_reg_count * sizeof(VMRegister));
            for (uint32_t i = 0; i < (uint32_t)argc; i++) {
                cur_regs[i] = call_args[i];
            }

            pc = target_pc;
        } break;

        /* ============================================================== */
        /* OP_RET  [src:u8] (src = 0xFF for void return)                  */
        /* ============================================================== */

        case OP_RET: {
            CHECK_BOUNDS(1);
            const uint8_t src = read8LE(bytecode + pc);
            if (src != 0xFF) {
                CHECK_REG(src);
            }

            if (ctx->call_depth == 0) {
                if (src != 0xFF) {
                    ctx->result = cur_regs[src];
                }
                ctx->pc = pc;
                ctx->flags &= ~VM_FLAG_RUNNING;
                ctx->flags |= VM_FLAG_HALTED;
                if (host_regs && reg_count > 0) {
                    memcpy(host_regs, ctx->registers, reg_count * sizeof(VMRegister));
                }
                return VM_OK;
            }

            VMRegister ret_val;
            memset(&ret_val, 0, sizeof(ret_val));
            if (src != 0xFF) {
                ret_val = cur_regs[src];
            }

            ctx->call_depth--;
            const VMFrame* frame = &ctx->frames[ctx->call_depth];

            reg_base  = frame->reg_base;
            reg_count = frame->reg_count;
            cur_regs  = ctx->registers + reg_base;

            cur_regs[frame->dst_reg] = ret_val;
            ctx->result               = ret_val;

            pc = frame->return_pc;
        } break;
        /* ============================================================== */
        /* CMP_U32 / CMP_U64  [dst:u8][lhs:u8][rhs:u8]                    */
        /* ============================================================== */

        case OP_CMP_U32:
        case OP_CMP_U64: {
            CHECK_BOUNDS(3);
            const uint8_t dst = read8LE(bytecode + pc);
            const uint8_t lhs = read8LE(bytecode + pc + 1);
            const uint8_t rhs = read8LE(bytecode + pc + 2);
            CHECK_REG(dst); CHECK_REG(lhs); CHECK_REG(rhs);
            pc += 3;
            if (op == OP_CMP_U32) {
                const uint32_t a = regs[lhs].u32, b = regs[rhs].u32;
                regs[dst].i32 = (a < b) ? -1 : (a > b) ? 1 : 0;
            } else {
                const uint64_t a = regs[lhs].u64, b = regs[rhs].u64;
                regs[dst].i32 = (a < b) ? -1 : (a > b) ? 1 : 0;
            }
        } break;

        /* ============================================================== */
        /* Unsigned conditional branches  [A:u8][B:u8][offset:i16]        */
        /* ============================================================== */

        case OP_IF_ULT:
        case OP_IF_UGE:
        case OP_IF_UGT:
        case OP_IF_ULE: {
            CHECK_BOUNDS(4);
            const uint8_t A      = read8LE  (bytecode + pc);
            const uint8_t B      = read8LE  (bytecode + pc + 1);
            const int16_t offset = read16SLE(bytecode + pc + 2);
            CHECK_REG(A); CHECK_REG(B);
            pc += 4;
            const uint32_t lhs = regs[A].u32;
            const uint32_t rhs = regs[B].u32;
            int taken;
            switch (op) {
            case OP_IF_ULT: taken = (lhs <  rhs); break;
            case OP_IF_UGE: taken = (lhs >= rhs); break;
            case OP_IF_UGT: taken = (lhs >  rhs); break;
            case OP_IF_ULE: taken = (lhs <= rhs); break;
            default: taken = 0; break;
            }
            if (taken)
                BRANCH_TARGET(pc, offset, pc);
        } break;

        /* ============================================================== */
        /* SELECT  [dst:u8][a:u8][b:u8][cond:u8]                          */
        /* dst = (cond.i32 != 0) ? a : b                                  */
        /* ============================================================== */

        case OP_SELECT: {
            CHECK_BOUNDS(4);
            const uint8_t dst  = read8LE(bytecode + pc);
            const uint8_t a    = read8LE(bytecode + pc + 1);
            const uint8_t b    = read8LE(bytecode + pc + 2);
            const uint8_t cond = read8LE(bytecode + pc + 3);
            CHECK_REG(dst); CHECK_REG(a); CHECK_REG(b); CHECK_REG(cond);
            pc += 4;
            regs[dst] = regs[cond].i32 ? regs[a] : regs[b];
        } break;

        /* ============================================================== */
        /* Bit manipulation — unary  [dst:u8][src:u8]                     */
        /* ============================================================== */

        case OP_CLZ_I32:    { CHECK_BOUNDS(2); uint8_t d=read8LE(bytecode+pc), s=read8LE(bytecode+pc+1); CHECK_REG(d); CHECK_REG(s); pc+=2; regs[d].u64 = (uint64_t)VM_CLZ32(regs[s].u32); } break;
        case OP_CLZ_I64:    { CHECK_BOUNDS(2); uint8_t d=read8LE(bytecode+pc), s=read8LE(bytecode+pc+1); CHECK_REG(d); CHECK_REG(s); pc+=2; regs[d].u64 = (uint64_t)VM_CLZ64(regs[s].u64); } break;
        case OP_CTZ_I32:    { CHECK_BOUNDS(2); uint8_t d=read8LE(bytecode+pc), s=read8LE(bytecode+pc+1); CHECK_REG(d); CHECK_REG(s); pc+=2; regs[d].u64 = (uint64_t)VM_CTZ32(regs[s].u32); } break;
        case OP_CTZ_I64:    { CHECK_BOUNDS(2); uint8_t d=read8LE(bytecode+pc), s=read8LE(bytecode+pc+1); CHECK_REG(d); CHECK_REG(s); pc+=2; regs[d].u64 = (uint64_t)VM_CTZ64(regs[s].u64); } break;
        case OP_POPCNT_I32: { CHECK_BOUNDS(2); uint8_t d=read8LE(bytecode+pc), s=read8LE(bytecode+pc+1); CHECK_REG(d); CHECK_REG(s); pc+=2; regs[d].u64 = (uint64_t)VM_POPCNT32(regs[s].u32); } break;
        case OP_POPCNT_I64: { CHECK_BOUNDS(2); uint8_t d=read8LE(bytecode+pc), s=read8LE(bytecode+pc+1); CHECK_REG(d); CHECK_REG(s); pc+=2; regs[d].u64 = (uint64_t)VM_POPCNT64(regs[s].u64); } break;

        /* ============================================================== */
        /* Rotations  [dst:u8][val:u8][amt:u8]                            */
        /* ============================================================== */

        case OP_ROTL_I32:
        case OP_ROTR_I32: {
            CHECK_BOUNDS(3);
            const uint8_t dst = read8LE(bytecode + pc);
            const uint8_t val = read8LE(bytecode + pc + 1);
            const uint8_t amt = read8LE(bytecode + pc + 2);
            CHECK_REG(dst); CHECK_REG(val); CHECK_REG(amt);
            pc += 3;
            const uint32_t v = regs[val].u32;
            const uint32_t s = (uint32_t)regs[amt].i32 & 31u;
            if (op == OP_ROTL_I32)
                regs[dst].u64 = (uint64_t)(s ? (v << s) | (v >> (32u - s)) : v);
            else
                regs[dst].u64 = (uint64_t)(s ? (v >> s) | (v << (32u - s)) : v);
        } break;

        case OP_ROTL_I64:
        case OP_ROTR_I64: {
            CHECK_BOUNDS(3);
            const uint8_t dst = read8LE(bytecode + pc);
            const uint8_t val = read8LE(bytecode + pc + 1);
            const uint8_t amt = read8LE(bytecode + pc + 2);
            CHECK_REG(dst); CHECK_REG(val); CHECK_REG(amt);
            pc += 3;
            const uint64_t v = regs[val].u64;
            const uint64_t s = (uint64_t)regs[amt].i64 & 63u;
            if (op == OP_ROTL_I64)
                regs[dst].u64 = s ? (v << s) | (v >> (64u - s)) : v;
            else
                regs[dst].u64 = s ? (v >> s) | (v << (64u - s)) : v;
        } break;

        /* ============================================================== */
        /* Integer ABS  [dst:u8][src:u8]                                  */
        /* ============================================================== */

        case OP_ABS_I32: {
            CHECK_BOUNDS(2);
            const uint8_t dst = read8LE(bytecode + pc);
            const uint8_t src = read8LE(bytecode + pc + 1);
            CHECK_REG(dst); CHECK_REG(src);
            pc += 2;
            const int32_t v = regs[src].i32;
            regs[dst].i32 = (v < 0) ? -v : v;
        } break;

        case OP_ABS_I64: {
            CHECK_BOUNDS(2);
            const uint8_t dst = read8LE(bytecode + pc);
            const uint8_t src = read8LE(bytecode + pc + 1);
            CHECK_REG(dst); CHECK_REG(src);
            pc += 2;
            const int64_t v = regs[src].i64;
            regs[dst].i64 = (v < 0) ? -v : v;
        } break;

        /* ============================================================== */
        /* Integer MIN / MAX  [dst:u8][a:u8][b:u8]                        */
        /* ============================================================== */

        case OP_MIN_I32:
        case OP_MAX_I32:
        case OP_MIN_U32:
        case OP_MAX_U32:
        case OP_MIN_I64:
        case OP_MAX_I64:
        case OP_MIN_U64:
        case OP_MAX_U64: {
            CHECK_BOUNDS(3);
            const uint8_t dst = read8LE(bytecode + pc);
            const uint8_t a   = read8LE(bytecode + pc + 1);
            const uint8_t b   = read8LE(bytecode + pc + 2);
            CHECK_REG(dst); CHECK_REG(a); CHECK_REG(b);
            pc += 3;
            switch (op) {
            case OP_MIN_I32: regs[dst].i32 = regs[a].i32 < regs[b].i32 ? regs[a].i32 : regs[b].i32; break;
            case OP_MAX_I32: regs[dst].i32 = regs[a].i32 > regs[b].i32 ? regs[a].i32 : regs[b].i32; break;
            case OP_MIN_U32: regs[dst].u32 = regs[a].u32 < regs[b].u32 ? regs[a].u32 : regs[b].u32; break;
            case OP_MAX_U32: regs[dst].u32 = regs[a].u32 > regs[b].u32 ? regs[a].u32 : regs[b].u32; break;
            case OP_MIN_I64: regs[dst].i64 = regs[a].i64 < regs[b].i64 ? regs[a].i64 : regs[b].i64; break;
            case OP_MAX_I64: regs[dst].i64 = regs[a].i64 > regs[b].i64 ? regs[a].i64 : regs[b].i64; break;
            case OP_MIN_U64: regs[dst].u64 = regs[a].u64 < regs[b].u64 ? regs[a].u64 : regs[b].u64; break;
            case OP_MAX_U64: regs[dst].u64 = regs[a].u64 > regs[b].u64 ? regs[a].u64 : regs[b].u64; break;
            default: break;
            }
        } break;

        /* ============================================================== */
        /* MULH  [dst:u8][a:u8][b:u8]                                     */
        /* ============================================================== */

        case OP_MULH_I32:
        case OP_MULH_U32:
        case OP_MULH_I64:
        case OP_MULH_U64: {
            CHECK_BOUNDS(3);
            const uint8_t dst = read8LE(bytecode + pc);
            const uint8_t a   = read8LE(bytecode + pc + 1);
            const uint8_t b   = read8LE(bytecode + pc + 2);
            CHECK_REG(dst); CHECK_REG(a); CHECK_REG(b);
            pc += 3;
            switch (op) {
            case OP_MULH_I32: regs[dst].i32 = (int32_t)(((int64_t) regs[a].i32 * (int64_t) regs[b].i32) >> 32); break;
            case OP_MULH_U32: regs[dst].u32 = (uint32_t)(((uint64_t)regs[a].u32 * (uint64_t)regs[b].u32) >> 32); break;
            case OP_MULH_I64: regs[dst].i64 = vm_mulh_i64(regs[a].i64, regs[b].i64); break;
            case OP_MULH_U64: regs[dst].u64 = vm_mulh_u64(regs[a].u64, regs[b].u64); break;
            default: break;
            }
        } break;

        /* ============================================================== */
        /* BOOL  [dst:u8][src:u8]                                         */
        /* ============================================================== */

        case OP_BOOL_I32: {
            CHECK_BOUNDS(2);
            const uint8_t dst = read8LE(bytecode + pc);
            const uint8_t src = read8LE(bytecode + pc + 1);
            CHECK_REG(dst); CHECK_REG(src);
            pc += 2;
            regs[dst].u64 = regs[src].i32 != 0 ? 1 : 0;
        } break;

        case OP_BOOL_I64: {
            CHECK_BOUNDS(2);
            const uint8_t dst = read8LE(bytecode + pc);
            const uint8_t src = read8LE(bytecode + pc + 1);
            CHECK_REG(dst); CHECK_REG(src);
            pc += 2;
            regs[dst].u64 = regs[src].i64 != 0 ? 1 : 0;
        } break;

        /* ============================================================== */
        /* Float intrinsics — unary  [dst:u8][src:u8]                     */
        /* ============================================================== */

        case OP_ABS_F32:   case OP_ABS_F64:
        case OP_SQRT_F32:  case OP_SQRT_F64:
        case OP_FLOOR_F32: case OP_FLOOR_F64:
        case OP_CEIL_F32:  case OP_CEIL_F64:
        case OP_TRUNC_F32: case OP_TRUNC_F64:
        case OP_ROUND_F32: case OP_ROUND_F64: {
            CHECK_BOUNDS(2);
            const uint8_t dst = read8LE(bytecode + pc);
            const uint8_t src = read8LE(bytecode + pc + 1);
            CHECK_REG(dst); CHECK_REG(src);
            pc += 2;
            switch (op) {
            case OP_ABS_F32:   regs[dst].f32 = fabsf(regs[src].f32);   break;
            case OP_ABS_F64:   regs[dst].f64 = fabs (regs[src].f64);   break;
            case OP_SQRT_F32:  regs[dst].f32 = sqrtf(regs[src].f32);   break;
            case OP_SQRT_F64:  regs[dst].f64 = sqrt (regs[src].f64);   break;
            case OP_FLOOR_F32: regs[dst].f32 = floorf(regs[src].f32);  break;
            case OP_FLOOR_F64: regs[dst].f64 = floor (regs[src].f64);  break;
            case OP_CEIL_F32:  regs[dst].f32 = ceilf(regs[src].f32);   break;
            case OP_CEIL_F64:  regs[dst].f64 = ceil (regs[src].f64);   break;
            case OP_TRUNC_F32: regs[dst].f32 = truncf(regs[src].f32);  break;
            case OP_TRUNC_F64: regs[dst].f64 = trunc (regs[src].f64);  break;
            case OP_ROUND_F32: regs[dst].f32 = roundf(regs[src].f32);  break;
            case OP_ROUND_F64: regs[dst].f64 = round (regs[src].f64);  break;
            default: break;
            }
        } break;

        /* ============================================================== */
        /* Float intrinsics — binary  [dst:u8][a:u8][b:u8]               */
        /* ============================================================== */

        case OP_MIN_F32:
        case OP_MAX_F32:
        case OP_MIN_F64:
        case OP_MAX_F64:
        case OP_COPYSIGN_F32:
        case OP_COPYSIGN_F64: {
            CHECK_BOUNDS(3);
            const uint8_t dst = read8LE(bytecode + pc);
            const uint8_t a   = read8LE(bytecode + pc + 1);
            const uint8_t b   = read8LE(bytecode + pc + 2);
            CHECK_REG(dst); CHECK_REG(a); CHECK_REG(b);
            pc += 3;
            switch (op) {
            /* fminf/fmaxf propagate NaN correctly per C99 Annex F */
            case OP_MIN_F32:      regs[dst].f32 = fminf(regs[a].f32, regs[b].f32);       break;
            case OP_MAX_F32:      regs[dst].f32 = fmaxf(regs[a].f32, regs[b].f32);       break;
            case OP_MIN_F64:      regs[dst].f64 = fmin (regs[a].f64, regs[b].f64);       break;
            case OP_MAX_F64:      regs[dst].f64 = fmax (regs[a].f64, regs[b].f64);       break;
            case OP_COPYSIGN_F32: regs[dst].f32 = copysignf(regs[a].f32, regs[b].f32);   break;
            case OP_COPYSIGN_F64: regs[dst].f64 = copysign (regs[a].f64, regs[b].f64);   break;
            default: break;
            }
        } break;

        /* ============================================================== */
        /* Load with immediate offset  [dst:u8][base:u8][offset:i32]      */
        /* dst = *(T*)((char*)base.ptr + offset)                          */
        /* ============================================================== */

        case OP_LOAD8_OFF:
        case OP_LOAD8S_OFF:
        case OP_LOAD16_OFF:
        case OP_LOAD16S_OFF:
        case OP_LOAD32_OFF:
        case OP_LOAD32S_OFF:
        case OP_LOAD64_OFF:
        case OP_LOAD_PTR_OFF: {
            CHECK_BOUNDS(6);
            const uint8_t  dst    = read8LE  (bytecode + pc);
            const uint8_t  base   = read8LE  (bytecode + pc + 1);
            const int32_t  offset = read32SLE(bytecode + pc + 2);
            CHECK_REG(dst); CHECK_REG(base);
            pc += 6;
            const void* p = (const char*)regs[base].ptr + offset;
            regs[dst].u64 = 0;
            switch (op) {
            case OP_LOAD8_OFF:    { uint8_t  v; memcpy(&v, p, 1); regs[dst].u64 = (uint64_t)v; } break;
            case OP_LOAD8S_OFF:   { int8_t   v; memcpy(&v, p, 1); regs[dst].i64 = (int64_t) v; } break;
            case OP_LOAD16_OFF:   { uint16_t v; memcpy(&v, p, 2); regs[dst].u64 = (uint64_t)v; } break;
            case OP_LOAD16S_OFF:  { int16_t  v; memcpy(&v, p, 2); regs[dst].i64 = (int64_t) v; } break;
            case OP_LOAD32_OFF:   { uint32_t v; memcpy(&v, p, 4); regs[dst].u64 = (uint64_t)v; } break;
            case OP_LOAD32S_OFF:  { int32_t  v; memcpy(&v, p, 4); regs[dst].i64 = (int64_t) v; } break;
            case OP_LOAD64_OFF:   {              memcpy(&regs[dst].u64, p, 8);                  } break;
            case OP_LOAD_PTR_OFF: {              memcpy(&regs[dst].ptr, p, sizeof(void*));      } break;
            default: break;
            }
        } break;

        /* ============================================================== */
        /* Store with immediate offset  [addr:u8][src:u8][offset:i32]     */
        /* *(T*)((char*)addr.ptr + offset) = src                          */
        /* ============================================================== */

        case OP_STORE8_OFF:
        case OP_STORE16_OFF:
        case OP_STORE32_OFF:
        case OP_STORE64_OFF:
        case OP_STORE_PTR_OFF: {
            CHECK_BOUNDS(6);
            const uint8_t  addr   = read8LE  (bytecode + pc);
            const uint8_t  src    = read8LE  (bytecode + pc + 1);
            const int32_t  offset = read32SLE(bytecode + pc + 2);
            CHECK_REG(addr); CHECK_REG(src);
            pc += 6;
            void* p = (char*)regs[addr].ptr + offset;
            switch (op) {
            case OP_STORE8_OFF:    { uint8_t  v = regs[src].u8;  memcpy(p, &v, 1);             } break;
            case OP_STORE16_OFF:   { uint16_t v = regs[src].u16; memcpy(p, &v, 2);             } break;
            case OP_STORE32_OFF:   { uint32_t v = regs[src].u32; memcpy(p, &v, 4);             } break;
            case OP_STORE64_OFF:   { uint64_t v = regs[src].u64; memcpy(p, &v, 8);             } break;
            case OP_STORE_PTR_OFF: { void*    v = regs[src].ptr; memcpy(p, &v, sizeof(void*)); } break;
            default: break;
            }
        } break;

        /* ============================================================== */
        /* LEA_REG  [dst:u8][base:u8][idx:u8]                             */
        /* dst.ptr = (char*)base.ptr + idx.i64                           */
        /* ============================================================== */

        case OP_LEA_REG: {
            CHECK_BOUNDS(3);
            const uint8_t dst = read8LE(bytecode + pc);
            const uint8_t base= read8LE(bytecode + pc + 1);
            const uint8_t idx = read8LE(bytecode + pc + 2);
            CHECK_REG(dst); CHECK_REG(base); CHECK_REG(idx);
            pc += 3;
            regs[dst].ptr = (char*)regs[base].ptr + regs[idx].i64;
        } break;

        /* ============================================================== */
        /* MEMCPY / MEMSET  [dst:u8][src:u8][len:u8]                      */
        /* ============================================================== */

        case OP_MEMCPY: {
            CHECK_BOUNDS(3);
            const uint8_t dst = read8LE(bytecode + pc);
            const uint8_t src = read8LE(bytecode + pc + 1);
            const uint8_t len = read8LE(bytecode + pc + 2);
            CHECK_REG(dst); CHECK_REG(src); CHECK_REG(len);
            pc += 3;
            memmove(regs[dst].ptr, regs[src].ptr, (size_t)regs[len].i64);
        } break;

        case OP_MEMSET: {
            CHECK_BOUNDS(3);
            const uint8_t dst = read8LE(bytecode + pc);
            const uint8_t val = read8LE(bytecode + pc + 1);
            const uint8_t len = read8LE(bytecode + pc + 2);
            CHECK_REG(dst); CHECK_REG(val); CHECK_REG(len);
            pc += 3;
            memset(regs[dst].ptr, regs[val].i32 & 0xFF, (size_t)regs[len].i64);
        } break;

        /* ============================================================== */
        /* SWITCH  [reg:u8][count:u32][default:i32][off_0:i32]...[off_N]  */
        /* ============================================================== */

        case OP_SWITCH: {
            CHECK_BOUNDS(9); /* reg + count + default = 1+4+4 */
            const uint8_t  reg     = read8LE  (bytecode + pc);
            const uint32_t count   = read32LE (bytecode + pc + 1);
            const int32_t  def_off = read32SLE(bytecode + pc + 5);
            CHECK_BOUNDS(9u + (uint64_t)count * 4u);
            CHECK_REG(reg);
            pc += 9u;
            const int32_t idx = regs[reg].i32;
            int32_t chosen;
            if (idx >= 0 && (uint32_t)idx < count) {
                chosen = read32SLE(bytecode + pc + (uint32_t)idx * 4u);
            } else {
                chosen = def_off;
            }
            pc += count * 4u;
            BRANCH_TARGET(pc, chosen, pc);
        } break;

        default:
            VM_EXIT_ERR(VM_ERR_INVALID_OPCODE);

        } /* switch (op) */

        ctx->pc = pc;
        if (single_step) {
            ctx->flags &= ~(VM_FLAG_RUNNING | VM_FLAG_SINGLE_STEP);
            ctx->flags |= VM_FLAG_PAUSED | VM_FLAG_RESUME;
            if (host_regs && reg_count > 0 && ctx->call_depth == 0) {
                memcpy(host_regs, ctx->registers, reg_count * sizeof(VMRegister));
            }
            return VM_OK;
        }
    } /* while (pc < bytecode_size) */

    ctx->pc = pc;
    ctx->flags &= ~VM_FLAG_RUNNING;
    ctx->flags |= VM_FLAG_HALTED;
    if (host_regs && reg_count > 0 && ctx->call_depth == 0) {
        memcpy(host_regs, ctx->registers, reg_count * sizeof(VMRegister));
    }
    #undef regs
    return VM_OK;
}

#if defined(VM_DEBUG)
/* =========================================================================
 * Debugger & Profiler Implementations (compiled only when -DVM_DEBUG is set)
 * ====================================================================== */

int vm_has_breakpoint(const VMContext* ctx, uint32_t pc)
{
    if (!ctx || !ctx->breakpoints || ctx->breakpoint_count == 0)
        return 0;
    for (uint32_t i = 0; i < ctx->breakpoint_count; i++) {
        if (ctx->breakpoints[i] == pc)
            return 1;
    }
    return 0;
}

void vm_profiler_reset(VMContext* ctx)
{
    if (!ctx) return;
    memset(ctx->opcode_counts, 0, sizeof(ctx->opcode_counts));
    ctx->total_instructions = 0;
}

typedef struct {
    uint16_t opcode;
    uint64_t count;
} VMOpCount;

static int cmp_op_counts(const void* a, const void* b)
{
    const VMOpCount* oa = (const VMOpCount*)a;
    const VMOpCount* ob = (const VMOpCount*)b;
    if (oa->count > ob->count) return -1;
    if (oa->count < ob->count) return 1;
    return 0;
}

void vm_profiler_dump(const VMContext* ctx, FILE* stream)
{
    if (!ctx || !stream) return;

    fprintf(stream, "\n============================================================\n");
    fprintf(stream, "                 CVM Execution Profiler Report              \n");
    fprintf(stream, "============================================================\n");
    fprintf(stream, "Total Instructions Executed: %llu\n\n", (unsigned long long)ctx->total_instructions);

    if (ctx->total_instructions == 0) {
        fprintf(stream, "No instructions executed.\n");
        fprintf(stream, "============================================================\n");
        return;
    }

    VMOpCount items[VM_OPCODE_COUNT];
    uint32_t item_count = 0;

    for (uint32_t op = 0; op < VM_OPCODE_COUNT; op++) {
        if (ctx->opcode_counts[op] > 0) {
            items[item_count].opcode = (uint16_t)op;
            items[item_count].count  = ctx->opcode_counts[op];
            item_count++;
        }
    }

    qsort(items, item_count, sizeof(VMOpCount), cmp_op_counts);

    fprintf(stream, "%-6s  %-20s  %-12s  %-10s\n", "Opcode", "Name", "Count", "Percentage");
    fprintf(stream, "------------------------------------------------------------\n");

    for (uint32_t i = 0; i < item_count; i++) {
        double pct = (double)items[i].count * 100.0 / (double)ctx->total_instructions;
        fprintf(stream, "0x%04X  %-20s  %-12llu  %6.2f%%\n",
                items[i].opcode,
                vm_opname(items[i].opcode),
                (unsigned long long)items[i].count,
                pct);
    }
    fprintf(stream, "============================================================\n\n");
}
#endif
