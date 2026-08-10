/*
 * vm.h  —  C/C++ Register VM  —  Public Interface
 *
 * A small, portable register-based VM designed to execute bytecode generated
 * from C/C++-level semantics.  No external dependencies.
 *
 * Bytecode format
 * ───────────────
 *   Every instruction begins with a 16-bit little-endian opcode, followed by
 *   zero or more operand bytes.  Multi-byte operands are little-endian.
 *
 *   Notation used in opcode comments:
 *     u8  / i8   – unsigned / signed 1-byte operand
 *     u16 / i16  – unsigned / signed 2-byte operand
 *     u32 / i32  – unsigned / signed 4-byte operand
 *     u64 / i64  – unsigned / signed 8-byte operand
 *     dst/lhs/rhs/src/addr/base  – register index (u8 unless noted)
 *
 * Branch offsets
 * ──────────────
 *   All branch offsets are signed and relative to the first byte of the
 *   instruction immediately following the branch operands (i.e. the "next"
 *   instruction).  A zero offset is a no-op; a negative offset loops back.
 *
 * Compatibility
 * ─────────────
 *   Compiles as C99 or newer.  Safe to include from C++ with the extern "C"
 *   guard below.  Compile vm.c with -DVM_DEBUG to enable runtime tracing
 *   (controlled per-context via VMContext::debug).
 */

#ifndef VM_H
#define VM_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

/** Maximum supported opcode count for profiler table. */
#define VM_OPCODE_COUNT 256

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * Error codes
 * ====================================================================== */

typedef enum {
    VM_OK                   = 0,    /* success / clean exit               */
    VM_ERR_INVALID_OPCODE   = 1,    /* unknown opcode encountered         */
    VM_ERR_OUT_OF_BOUNDS    = 2,    /* pc or branch target out of range   */
    VM_ERR_DIV_ZERO         = 3,    /* integer division by zero           */
    VM_ERR_INVALID_REGISTER = 4,    /* register index >= reg_count        */
    VM_ERR_BAD_FUNCTION     = 5,    /* function id out of range / null    */
    VM_ERR_BAD_ARGC         = 6,    /* argc > VM_MAX_CALL_ARGC            */
    VM_ERR_STACK_OVERFLOW   = 7     /* recursion depth > max call depth   */
} VMError;

/* =========================================================================
 * Opcode table  (uint16_t values, sequential from 0)
 * ====================================================================== */

typedef enum {

    /* ------------------------------------------------------------------ */
    /*  Miscellaneous                                                      */
    /* ------------------------------------------------------------------ */

    OP_NOP          = 0x0000,   /* (no operands)                          */

    /* ------------------------------------------------------------------ */
    /*  Data movement                                                      */
    /* ------------------------------------------------------------------ */

    OP_MOVE,                    /* [dst:u8][src:u16]                      */

    /* ------------------------------------------------------------------ */
    /*  Integer constants                                                  */
    /*                                                                     */
    /*  All integer constants are sign-extended into the full 64-bit       */
    /*  register so they are immediately usable as any integer type.       */
    /* ------------------------------------------------------------------ */

    OP_CONST_I8,                /* [dst:u8][val:i8]                       */
    OP_CONST_I16,               /* [dst:u8][val:i16]                      */
    OP_CONST_I32,               /* [dst:u8][val:i32]                      */
    OP_CONST_I64,               /* [dst:u8][val:i64]                      */

    /* ------------------------------------------------------------------ */
    /*  Float constants  (IEEE-754 bit pattern in little-endian)           */
    /* ------------------------------------------------------------------ */

    OP_CONST_F32,               /* [dst:u8][bits:u32]                     */
    OP_CONST_F64,               /* [dst:u8][bits:u64]                     */

    /* ------------------------------------------------------------------ */
    /*  Integer arithmetic – i32                                          */
    /*                                                                     */
    /*  DIV_I32 / REM_I32 trap (return VM_ERR_DIV_ZERO) on division by   */
    /*  zero.  INT32_MIN / -1 is handled without undefined behaviour.     */
    /* ------------------------------------------------------------------ */

    OP_ADD_I32,                 /* [dst:u8][lhs:u8][rhs:u8]               */
    OP_SUB_I32,
    OP_MUL_I32,
    OP_DIV_I32,
    OP_REM_I32,

    /* ------------------------------------------------------------------ */
    /*  Integer arithmetic – i64                                          */
    /* ------------------------------------------------------------------ */

    OP_ADD_I64,
    OP_SUB_I64,
    OP_MUL_I64,
    OP_DIV_I64,
    OP_REM_I64,

    /* ------------------------------------------------------------------ */
    /*  Unsigned integer arithmetic                                        */
    /*                                                                     */
    /*  DIV_U32 / REM_U32 / DIV_U64 / REM_U64 operate on the u32/u64     */
    /*  view of the register.  Trap (VM_ERR_DIV_ZERO) on division by      */
    /*  zero.  No INT_MIN/-1 special case — unsigned div never overflows.  */
    /* ------------------------------------------------------------------ */

    OP_DIV_U32,                 /* [dst:u8][lhs:u8][rhs:u8]               */
    OP_REM_U32,
    OP_DIV_U64,
    OP_REM_U64,

    /* ------------------------------------------------------------------ */
    /*  Float arithmetic – f32  (IEEE-754 NaN/Inf propagate naturally)    */
    /* ------------------------------------------------------------------ */

    OP_ADD_F32,
    OP_SUB_F32,
    OP_MUL_F32,
    OP_DIV_F32,

    /* ------------------------------------------------------------------ */
    /*  Float arithmetic – f64                                            */
    /* ------------------------------------------------------------------ */

    OP_ADD_F64,
    OP_SUB_F64,
    OP_MUL_F64,
    OP_DIV_F64,

    /* ------------------------------------------------------------------ */
    /*  Unary operations                                                   */
    /* ------------------------------------------------------------------ */

    OP_NEG_I32,                 /* [dst:u8][src:u8]  arithmetic negate    */
    OP_NEG_I64,
    OP_NEG_F32,
    OP_NEG_F64,
    OP_NOT_I32,                 /* bitwise complement                     */
    OP_NOT_I64,

    /* ------------------------------------------------------------------ */
    /*  Bitwise – i32                                                      */
    /* ------------------------------------------------------------------ */

    OP_AND_I32,                 /* [dst:u8][lhs:u8][rhs:u8]               */
    OP_OR_I32,
    OP_XOR_I32,

    /* ------------------------------------------------------------------ */
    /*  Bitwise – i64                                                      */
    /* ------------------------------------------------------------------ */

    OP_AND_I64,
    OP_OR_I64,
    OP_XOR_I64,

    /* ------------------------------------------------------------------ */
    /*  Shifts – i32  (shift amount masked to low 5 bits)                 */
    /* ------------------------------------------------------------------ */

    OP_SHL_I32,                 /* [dst:u8][val:u8][amt:u8]               */
    OP_SHR_I32,                 /* arithmetic (signed) right shift        */
    OP_USHR_I32,                /* logical (unsigned) right shift         */

    /* ------------------------------------------------------------------ */
    /*  Shifts – i64  (shift amount masked to low 6 bits)                 */
    /* ------------------------------------------------------------------ */

    OP_SHL_I64,
    OP_SHR_I64,
    OP_USHR_I64,

    /* ------------------------------------------------------------------ */
    /*  Comparisons: dst.i32 = -1 | 0 | +1                               */
    /*                                                                     */
    /*  CMP_F32 / CMP_F64 return -1 when either operand is NaN            */
    /*  (unordered-less, matches Java fcmpl semantics).                   */
    /*                                                                     */
    /*  CMP_F32_GT returns +2 when either operand is NaN                  */
    /*  (unordered-greater, matches Java fcmpg semantics).  This allows   */
    /*  correct compilation of !(a < b) vs (a >= b) when NaN is possible. */
    /* ------------------------------------------------------------------ */

    OP_CMP_I32,                 /* [dst:u8][lhs:u8][rhs:u8]               */
    OP_CMP_I64,
    OP_CMP_F32,
    OP_CMP_F64,
    OP_CMP_F32_GT,              /* like CMP_F32 but NaN → +2              */

    /* ------------------------------------------------------------------ */
    /*  Type conversions                                                   */
    /* ------------------------------------------------------------------ */

    OP_I32_TO_I8,               /* [dst:u8][src:u8]  truncate then sign-  */
    OP_I32_TO_I16,              /*   extend result into full register      */
    OP_I32_TO_I64,              /* sign-extend                            */
    OP_I32_TO_F32,
    OP_I32_TO_F64,
    OP_I64_TO_I32,              /* truncate low 32 bits                   */
    OP_I64_TO_F32,
    OP_I64_TO_F64,
    OP_F32_TO_I32,              /* truncate toward zero                   */
    OP_F32_TO_I64,
    OP_F32_TO_F64,
    OP_F64_TO_I32,
    OP_F64_TO_I64,
    OP_F64_TO_F32,

    /* ------------------------------------------------------------------ */
    /*  Memory loads: dst = *(T*)(addr_reg.ptr)                           */
    /*                                                                     */
    /*  Unsigned variants zero-extend the loaded value into the full      */
    /*  register (dst.u64).  Signed variants sign-extend (dst.i64).       */
    /* ------------------------------------------------------------------ */

    OP_LOAD8,                   /* [dst:u8][addr:u8]  zero-extend → u64  */
    OP_LOAD8S,                  /* [dst:u8][addr:u8]  sign-extend → i64  */
    OP_LOAD16,
    OP_LOAD16S,
    OP_LOAD32,
    OP_LOAD32S,
    OP_LOAD64,                  /* load full 64-bit value                 */
    OP_LOAD_PTR,                /* load a pointer-sized value             */

    /* ------------------------------------------------------------------ */
    /*  Memory stores: *(T*)(addr_reg.ptr) = src (truncated to width)     */
    /* ------------------------------------------------------------------ */

    OP_STORE8,                  /* [addr:u8][src:u8]                      */
    OP_STORE16,
    OP_STORE32,
    OP_STORE64,
    OP_STORE_PTR,

    /* ------------------------------------------------------------------ */
    /*  Pointer arithmetic                                                 */
    /* ------------------------------------------------------------------ */

    OP_LEA,                     /* [dst:u8][base:u8][offset:i32]          */
                                /* dst.ptr = (char*)base.ptr + offset     */

    /* ------------------------------------------------------------------ */
    /*  Unconditional branches                                             */
    /*  (offset = signed displacement from next instruction)              */
    /* ------------------------------------------------------------------ */

    OP_GOTO,                    /* [offset:i8]                            */
    OP_GOTO_16,                 /* [offset:i16]                           */
    OP_GOTO_32,                 /* [offset:i32]                           */

    /* ------------------------------------------------------------------ */
    /*  Conditional branches — compare two i32 registers                  */
    /* ------------------------------------------------------------------ */

    OP_IF_EQ,                   /* [A:u8][B:u8][offset:i16]               */
    OP_IF_NE,
    OP_IF_LT,
    OP_IF_GE,
    OP_IF_GT,
    OP_IF_LE,

    /* ------------------------------------------------------------------ */
    /*  Conditional branches — compare i32 register vs zero               */
    /* ------------------------------------------------------------------ */

    OP_IF_EQZ,                  /* [A:u8][offset:i16]                     */
    OP_IF_NEZ,
    OP_IF_LTZ,
    OP_IF_GEZ,
    OP_IF_GTZ,
    OP_IF_LEZ,

    /* ------------------------------------------------------------------ */
    /*  Native function calls                                              */
    /*                                                                     */
    /*  CALL      — call and store return value in dst.                   */
    /*  CALL_VOID — call and discard return value (ctx->result updated).  */
    /*                                                                     */
    /*  Variable-length encoding:                                          */
    /*    CALL      [dst:u8][id:u32][argc:u8][r0:u8]...[rN:u8]           */
    /*    CALL_VOID [id:u32][argc:u8][r0:u8]...[rN:u8]                   */
    /* ------------------------------------------------------------------ */

    OP_CALL,
    OP_CALL_VOID,

    /* ------------------------------------------------------------------ */
    /*  Return                                                             */
    /* ------------------------------------------------------------------ */

    OP_RETURN_VOID,             /* (no operands)                          */
    OP_RETURN,                  /* [src:u8]  — sets ctx->result           */

    /* ------------------------------------------------------------------ */
    /*  Subroutines & Bytecode Calls                                       */
    /* ------------------------------------------------------------------ */

    OP_CALL_BC,                 /* [dst:u8][target:u32][argc:u8][arg0..N] */
    OP_RET                      /* [src:u8] (src=0xFF for void)           */

} VMOpcode;

/* =========================================================================
 * Register
 *
 * Each register is a 64-bit union capable of holding any primitive C type.
 * The caller is responsible for reading the correct member after an
 * operation (i.e. registers are untyped at the VM level).
 * ====================================================================== */

typedef union {
    int8_t   i8;
    uint8_t  u8;
    int16_t  i16;
    uint16_t u16;
    int32_t  i32;
    uint32_t u32;
    int64_t  i64;
    uint64_t u64;
    float    f32;
    double   f64;
    void*    ptr;
} VMRegister;

/* =========================================================================
 * Native function interface
 * ====================================================================== */

struct VMContext; /* forward declaration for VMNativeFn */

/*
 * Signature for native functions callable from bytecode via CALL / CALL_VOID.
 *
 *   ctx        – executing VM context.
 *   argc       – number of argument registers passed.
 *   args       – array of [argc] register values.
 *   out_result – write the return value here (never NULL).
 *
 * Return VM_OK on success.  Any other VMError aborts execution and is
 * propagated as the return value of vm_execute().
 */
typedef VMError (*VMNativeFn)(
    struct VMContext* ctx,
    uint32_t          argc,
    VMRegister*       args,
    VMRegister*       out_result
);

/* =========================================================================
 * VM Context
 * ====================================================================== */

/** Maximum number of native functions that can be registered. */
#define VM_MAX_NATIVE_FUNCS  256

/** Maximum number of arguments per CALL / CALL_VOID instruction. */
#define VM_MAX_CALL_ARGC      64

/** Maximum call stack depth for bytecode subroutines (OP_CALL_BC). */
#define VM_MAX_CALL_DEPTH    128

/** Maximum registers saved per call frame. */
#define VM_MAX_FRAME_REGS     64

/* =========================================================================
 * Execution flags
 * ====================================================================== */

#define VM_FLAG_RUNNING      (1u << 0)  /* VM is actively executing           */
#define VM_FLAG_PAUSED       (1u << 1)  /* execution is paused                */
#define VM_FLAG_SINGLE_STEP  (1u << 2)  /* execute exactly one instruction   */
#define VM_FLAG_HALTED       (1u << 3)  /* execution has halted / finished    */

/* =========================================================================
 * Debugger & Profiler interface (enabled when compiled with -DVM_DEBUG)
 * ====================================================================== */

#if defined(VM_DEBUG)
typedef enum VMDebugEvent {
    VM_DEBUG_EVENT_STEP,       /* executed single instruction                 */
    VM_DEBUG_EVENT_BREAKPOINT, /* hit breakpoint address                       */
    VM_DEBUG_EVENT_ERROR       /* trapped execution error                      */
} VMDebugEvent;

struct VMContext;

/** Callback function pointer for debugger events. */
typedef void (*VMDebugHook)(struct VMContext* ctx, VMDebugEvent event, uint32_t pc, uint16_t opcode);
#endif

/* =========================================================================
 * Call Frame (Bytecode Subroutines)
 * ====================================================================== */

typedef struct VMFrame {
    uint32_t    return_pc;                        /* PC to resume in caller    */
    uint8_t     dst_reg;                          /* caller's destination reg  */
    uint32_t    reg_count;                        /* caller's register count   */
    VMRegister  saved_regs[VM_MAX_FRAME_REGS];    /* saved caller registers    */
} VMFrame;

typedef struct VMContext {
    VMRegister  result;                           /* last RETURN / CALL value  */
    uint32_t    pc;                               /* current program counter   */
    uint32_t    flags;                            /* execution state flags     */
    uint32_t    native_count;                     /* number of registered fns  */
    VMNativeFn  native_funcs[VM_MAX_NATIVE_FUNCS];
    void*       user_data;                        /* host-defined payload      */

#if defined(VM_DEBUG)
    int         debug;                            /* non-zero: trace to stderr */

    /* Debugger interface */
    VMDebugHook     debug_hook;                   /* event callback hook       */
    const uint32_t* breakpoints;                  /* array of breakpoint PCs   */
    uint32_t        breakpoint_count;             /* number of breakpoints     */

    /* Profiler interface */
    uint64_t    opcode_counts[VM_OPCODE_COUNT];   /* per-opcode execution count*/
    uint64_t    total_instructions;               /* total instructions run    */
    int         profiler_enabled;                 /* non-zero: enable profiler */
#endif

    /* Call Stack (OP_CALL_BC / OP_RET) */
    VMFrame     call_stack[VM_MAX_CALL_DEPTH];    /* subroutine stack frames   */
    uint32_t    call_depth;                       /* current call stack depth  */
} VMContext;

/* =========================================================================
 * Public API
 * ====================================================================== */

/*
 * vm_init()
 *
 * Zero-initialize a VMContext.  Must be called before any other vm_* call.
 */
void vm_init(VMContext* ctx);

/*
 * vm_register_function()
 *
 * Register a native function at the given id (0 ≤ id < VM_MAX_NATIVE_FUNCS).
 * Registering the same id twice overwrites the previous entry.
 *
 * Returns VM_ERR_BAD_FUNCTION if id is out of range.
 */
VMError vm_register_function(VMContext* ctx, uint32_t id, VMNativeFn fn);

/*
 * vm_execute()
 *
 * Execute bytecode until a RETURN / RETURN_VOID instruction is reached, the
 * program counter reaches the end of the buffer, or an error occurs.
 *
 *   ctx           – runtime context (must be initialised with vm_init)
 *   regs          – caller-provided register file
 *   reg_count     – number of valid registers in regs[]
 *   pc            – initial program counter (byte offset into bytecode)
 *   bytecode      – read-only bytecode buffer
 *   bytecode_size – size of bytecode in bytes
 *
 * On OP_RETURN,   ctx->result holds the returned value.
 * On OP_CALL,     ctx->result holds the native function's return value.
 *
 * Returns VM_OK on a clean exit, or a non-zero VMError on failure.
 *
 * Compile vm.c with -DVM_DEBUG to enable instruction tracing.  Tracing is
 * then gated at runtime by ctx->debug (non-zero = enabled).
 */
VMError vm_execute(
    VMContext*     ctx,
    VMRegister*    regs,
    uint32_t       reg_count,
    uint32_t       pc,
    const uint8_t* bytecode,
    uint32_t       bytecode_size
);

#if defined(VM_DEBUG)
/* =========================================================================
 * Debugger & Profiler API (compiled only when -DVM_DEBUG is set)
 * ====================================================================== */

/** Returns 1 if pc matches a breakpoint in ctx->breakpoints, 0 otherwise. */
int vm_has_breakpoint(const VMContext* ctx, uint32_t pc);

/** Reset all profiler opcode counters and total_instructions to zero. */
void vm_profiler_reset(VMContext* ctx);

/** Dump formatted profiler report (top executed opcodes, totals) to stream. */
void vm_profiler_dump(const VMContext* ctx, FILE* stream);
#endif

#ifdef __cplusplus
}
#endif

#endif /* VM_H */