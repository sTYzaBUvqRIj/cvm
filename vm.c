/*
 * vm.c  —  C/C++ Register VM  —  Implementation
 *
 * Compile with -DVM_DEBUG to enable the ctx->debug runtime tracing flag.
 */

#include "vm.h"

#include <math.h>       /* isnan()                          */
#include <stdio.h>      /* fprintf(), stderr  (VM_DEBUG)    */
#include <string.h>     /* memset(), memcpy()               */

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

#define VM_EXIT_ERR(err_code) \
    do { \
        ctx->pc = pc; \
        ctx->flags &= ~VM_FLAG_RUNNING; \
        ctx->flags |= VM_FLAG_HALTED; \
        return (err_code); \
    } while (0)

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
    default:              return "??";
    }
}

#endif /* VM_DEBUG */

/* =========================================================================
 * API
 * ====================================================================== */

void vm_init(VMContext* ctx)
{
    memset(ctx, 0, sizeof(VMContext));
}

VMError vm_register_function(VMContext* ctx, uint32_t id, VMNativeFn fn)
{
    if (id >= VM_MAX_NATIVE_FUNCS)
        return VM_ERR_BAD_FUNCTION;
    ctx->native_funcs[id] = fn;
    ctx->native_count++;
    return VM_OK;
}

/* =========================================================================
 * vm_execute — main dispatch loop
 * ====================================================================== */

VMError vm_execute(
    VMContext*     ctx,
    VMRegister*    regs,
    uint32_t       reg_count,
    uint32_t       pc,
    const uint8_t* bytecode,
    uint32_t       bytecode_size)
{
    if ((ctx->flags & VM_FLAG_PAUSED) && !(ctx->flags & VM_FLAG_SINGLE_STEP)) {
        return VM_OK;
    }

    /* If resuming from PAUSED state and pc is 0, continue from ctx->pc; otherwise use pc */
    if ((ctx->flags & VM_FLAG_PAUSED) && pc == 0) {
        pc = ctx->pc;
    } else {
        ctx->pc = pc;
    }

    const int single_step = (ctx->flags & VM_FLAG_SINGLE_STEP) != 0;

    ctx->flags |= VM_FLAG_RUNNING;
    ctx->flags &= ~(VM_FLAG_PAUSED | VM_FLAG_HALTED);

    while (pc < bytecode_size) {
        ctx->pc = pc;

        /* ----------------------------------------------------------------
         * Read the 16-bit opcode.
         * ------------------------------------------------------------ */
#if defined(VM_DEBUG)
        const uint32_t instr_pc = pc;
#endif
        CHECK_BOUNDS(2);
        const uint16_t op = read16LE(bytecode + pc);
        pc += 2;

#if defined(VM_DEBUG)
        if (ctx->debug)
            fprintf(stderr, "PC=0x%04X  OP=%s\n", instr_pc, vm_opname(op));
#endif

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
            if (argc > VM_MAX_CALL_ARGC)    VM_EXIT_ERR(VM_ERR_BAD_ARGC);
            if (id >= VM_MAX_NATIVE_FUNCS)  VM_EXIT_ERR(VM_ERR_BAD_FUNCTION);
            if (!ctx->native_funcs[id])     VM_EXIT_ERR(VM_ERR_BAD_FUNCTION);

            VMRegister args[VM_MAX_CALL_ARGC];
            {
                uint32_t i;
                for (i = 0; i < (uint32_t)argc; i++) {
                    const uint8_t ai = read8LE(bytecode + pc + 6 + i);
                    CHECK_REG(ai);
                    args[i] = regs[ai];
                }
            }
            pc += 6u + (uint32_t)argc;

            VMRegister call_result;
            memset(&call_result, 0, sizeof(call_result));
            {
                const VMError err = ctx->native_funcs[id](ctx, (uint32_t)argc, args, &call_result);
                if (err != VM_OK) VM_EXIT_ERR(err);
            }
            regs[dst]   = call_result;
            ctx->result = call_result;
        } break;

        /* ============================================================== */
        /* CALL_VOID  [id:u32][argc:u8][r0:u8]...[rN:u8]                  */
        /* ============================================================== */

        case OP_CALL_VOID: {
            CHECK_BOUNDS(5);
            const uint32_t id   = read32LE(bytecode + pc);
            const uint8_t  argc = read8LE (bytecode + pc + 4);
            CHECK_BOUNDS(5u + (uint32_t)argc);
            if (argc > VM_MAX_CALL_ARGC)    VM_EXIT_ERR(VM_ERR_BAD_ARGC);
            if (id >= VM_MAX_NATIVE_FUNCS)  VM_EXIT_ERR(VM_ERR_BAD_FUNCTION);
            if (!ctx->native_funcs[id])     VM_EXIT_ERR(VM_ERR_BAD_FUNCTION);

            VMRegister args[VM_MAX_CALL_ARGC];
            {
                uint32_t i;
                for (i = 0; i < (uint32_t)argc; i++) {
                    const uint8_t ai = read8LE(bytecode + pc + 5 + i);
                    CHECK_REG(ai);
                    args[i] = regs[ai];
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
            return VM_OK;

        case OP_RETURN: {
            CHECK_BOUNDS(1);
            const uint8_t src = read8LE(bytecode + pc);
            CHECK_REG(src);
            ctx->result = regs[src];
            ctx->pc = pc;
            ctx->flags &= ~VM_FLAG_RUNNING;
            ctx->flags |= VM_FLAG_HALTED;
            return VM_OK;
        }

        /* ============================================================== */
        /* Unknown opcode                                                  */
        /* ============================================================== */

        default:
            VM_EXIT_ERR(VM_ERR_INVALID_OPCODE);

        } /* switch (op) */

        ctx->pc = pc;
        if (single_step) {
            ctx->flags &= ~(VM_FLAG_RUNNING | VM_FLAG_SINGLE_STEP);
            ctx->flags |= VM_FLAG_PAUSED;
            return VM_OK;
        }
    } /* while (pc < bytecode_size) */

    ctx->pc = pc;
    ctx->flags &= ~VM_FLAG_RUNNING;
    ctx->flags |= VM_FLAG_HALTED;
    return VM_OK;
}
