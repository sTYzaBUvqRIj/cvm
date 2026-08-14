/*
 * vm_builder.h  —  Bytecode construction helpers and test framework
 *                  for the C/C++ Register VM example suite.
 *
 * Include this file (alongside ../vm.h) in each example program.
 * It is NOT part of the VM implementation and must not modify vm.h / vm.c.
 *
 * -------------------------------------------------------------------------
 * Bytecode layout reminder (little-endian):
 *
 *   Every instruction:  [opcode:u16][operands...]
 *
 *   Branch offset semantics:
 *     offset is signed, relative to the byte immediately AFTER the last
 *     operand byte of the branch instruction ("next instruction").
 *     target = next_pc + offset
 *     offset = 0  → no-op (fall through)
 *     offset < 0  → loop back
 *     offset > 0  → skip forward
 *
 * -------------------------------------------------------------------------
 * Common loop pattern:
 *
 *   uint32_t loop_top = bc->size;
 *   // ... loop body ...
 *   // backward branch: jump to loop_top
 *   uint32_t p = emit_if_nez(bc, counter_reg);       // or emit_if_gtz, etc.
 *   bc_patch_back(bc, p, loop_top);                  // fills offset
 *
 * Forward skip pattern:
 *
 *   uint32_t skip = emit_if_eqz(bc, cond_reg);       // branch-if-zero
 *   // ... then body ...
 *   bc_patch_here(bc, skip);                          // skip lands here
 *
 * -------------------------------------------------------------------------
 */

#ifndef VM_BUILDER_H
#define VM_BUILDER_H

/* Suppress warnings about static helpers not used in every TU. */
#ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(disable: 4505)
#elif defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wunused-function"
#endif

#include "vm.h"
#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* =========================================================================
 * Bytecode buffer
 * ====================================================================== */

#define BC_MAX_SIZE 16384

typedef struct {
    uint8_t  data[BC_MAX_SIZE];
    uint32_t size;
} Bytecode;

static void bc_init(Bytecode* bc)
{
    bc->size = 0;
}

/* =========================================================================
 * Low-level little-endian writers
 * ====================================================================== */

static void bc_u8(Bytecode* bc, uint8_t v)
{
    assert(bc->size < BC_MAX_SIZE &&
           "Bytecode buffer overflow: increase BC_MAX_SIZE or split bytecode");
    bc->data[bc->size++] = v;
}

static void bc_i8(Bytecode* bc, int8_t v)
{
    bc->data[bc->size++] = (uint8_t)v;
}

static void bc_u16(Bytecode* bc, uint16_t v)
{
    bc->data[bc->size++] = (uint8_t)(v);
    bc->data[bc->size++] = (uint8_t)(v >> 8);
}

static void bc_i16(Bytecode* bc, int16_t v)
{
    bc_u16(bc, (uint16_t)v);
}

static void bc_u32(Bytecode* bc, uint32_t v)
{
    bc->data[bc->size++] = (uint8_t)(v);
    bc->data[bc->size++] = (uint8_t)(v >>  8);
    bc->data[bc->size++] = (uint8_t)(v >> 16);
    bc->data[bc->size++] = (uint8_t)(v >> 24);
}

static void bc_i32(Bytecode* bc, int32_t v)
{
    bc_u32(bc, (uint32_t)v);
}

static void bc_u64(Bytecode* bc, uint64_t v)
{
    bc_u32(bc, (uint32_t)v);
    bc_u32(bc, (uint32_t)(v >> 32));
}

static void bc_i64(Bytecode* bc, int64_t v)
{
    bc_u64(bc, (uint64_t)v);
}

static void bc_f32(Bytecode* bc, float f)
{
    uint32_t bits;
    memcpy(&bits, &f, sizeof(float));
    bc_u32(bc, bits);
}

static void bc_f64(Bytecode* bc, double d)
{
    uint64_t bits;
    memcpy(&bits, &d, sizeof(double));
    bc_u64(bc, bits);
}

static void bc_op(Bytecode* bc, VMOpcode op)
{
    bc_u16(bc, (uint16_t)op);
}

/* =========================================================================
 * Opcode emitters  (one function per instruction)
 *
 * Parameters mirror the bytecode encoding:
 *   dst / lhs / rhs / src / addr / base / val / amt  → register index (u8)
 *   All branch-emitting functions return the position of the displacement
 *   field in the buffer for later patching.
 * ====================================================================== */

/* --- Misc ---------------------------------------------------------------- */
static void emit_nop(Bytecode* bc)
{
    bc_op(bc, OP_NOP);
}

/* --- Data movement -------------------------------------------------------- */
/* MOVE [dst:u8][src:u16] */
static void emit_move(Bytecode* bc, uint8_t dst, uint16_t src)
{
    bc_op(bc, OP_MOVE); bc_u8(bc, dst); bc_u16(bc, src);
}

/* --- Integer constants --------------------------------------------------- */
static void emit_const_i8 (Bytecode* bc, uint8_t dst, int8_t  val) { bc_op(bc, OP_CONST_I8);  bc_u8(bc, dst); bc_i8 (bc, val); }
static void emit_const_i16(Bytecode* bc, uint8_t dst, int16_t val) { bc_op(bc, OP_CONST_I16); bc_u8(bc, dst); bc_i16(bc, val); }
static void emit_const_i32(Bytecode* bc, uint8_t dst, int32_t val) { bc_op(bc, OP_CONST_I32); bc_u8(bc, dst); bc_i32(bc, val); }
static void emit_const_i64(Bytecode* bc, uint8_t dst, int64_t val) { bc_op(bc, OP_CONST_I64); bc_u8(bc, dst); bc_i64(bc, val); }

/* --- Float constants (pass host float/double; bits are encoded) ----------- */
static void emit_const_f32(Bytecode* bc, uint8_t dst, float  val) { bc_op(bc, OP_CONST_F32); bc_u8(bc, dst); bc_f32(bc, val); }
static void emit_const_f64(Bytecode* bc, uint8_t dst, double val) { bc_op(bc, OP_CONST_F64); bc_u8(bc, dst); bc_f64(bc, val); }

/* --- Integer arithmetic – i32 -------------------------------------------- */
static void emit_add_i32(Bytecode* bc, uint8_t d, uint8_t l, uint8_t r) { bc_op(bc, OP_ADD_I32); bc_u8(bc,d); bc_u8(bc,l); bc_u8(bc,r); }
static void emit_sub_i32(Bytecode* bc, uint8_t d, uint8_t l, uint8_t r) { bc_op(bc, OP_SUB_I32); bc_u8(bc,d); bc_u8(bc,l); bc_u8(bc,r); }
static void emit_mul_i32(Bytecode* bc, uint8_t d, uint8_t l, uint8_t r) { bc_op(bc, OP_MUL_I32); bc_u8(bc,d); bc_u8(bc,l); bc_u8(bc,r); }
static void emit_div_i32(Bytecode* bc, uint8_t d, uint8_t l, uint8_t r) { bc_op(bc, OP_DIV_I32); bc_u8(bc,d); bc_u8(bc,l); bc_u8(bc,r); }
static void emit_rem_i32(Bytecode* bc, uint8_t d, uint8_t l, uint8_t r) { bc_op(bc, OP_REM_I32); bc_u8(bc,d); bc_u8(bc,l); bc_u8(bc,r); }

/* --- Integer arithmetic – i64 -------------------------------------------- */
static void emit_add_i64(Bytecode* bc, uint8_t d, uint8_t l, uint8_t r) { bc_op(bc, OP_ADD_I64); bc_u8(bc,d); bc_u8(bc,l); bc_u8(bc,r); }
static void emit_sub_i64(Bytecode* bc, uint8_t d, uint8_t l, uint8_t r) { bc_op(bc, OP_SUB_I64); bc_u8(bc,d); bc_u8(bc,l); bc_u8(bc,r); }
static void emit_mul_i64(Bytecode* bc, uint8_t d, uint8_t l, uint8_t r) { bc_op(bc, OP_MUL_I64); bc_u8(bc,d); bc_u8(bc,l); bc_u8(bc,r); }
static void emit_div_i64(Bytecode* bc, uint8_t d, uint8_t l, uint8_t r) { bc_op(bc, OP_DIV_I64); bc_u8(bc,d); bc_u8(bc,l); bc_u8(bc,r); }
static void emit_rem_i64(Bytecode* bc, uint8_t d, uint8_t l, uint8_t r) { bc_op(bc, OP_REM_I64); bc_u8(bc,d); bc_u8(bc,l); bc_u8(bc,r); }

/* --- Unsigned integer arithmetic ----------------------------------------- */
static void emit_div_u32(Bytecode* bc, uint8_t d, uint8_t l, uint8_t r) { bc_op(bc, OP_DIV_U32); bc_u8(bc,d); bc_u8(bc,l); bc_u8(bc,r); }
static void emit_rem_u32(Bytecode* bc, uint8_t d, uint8_t l, uint8_t r) { bc_op(bc, OP_REM_U32); bc_u8(bc,d); bc_u8(bc,l); bc_u8(bc,r); }
static void emit_div_u64(Bytecode* bc, uint8_t d, uint8_t l, uint8_t r) { bc_op(bc, OP_DIV_U64); bc_u8(bc,d); bc_u8(bc,l); bc_u8(bc,r); }
static void emit_rem_u64(Bytecode* bc, uint8_t d, uint8_t l, uint8_t r) { bc_op(bc, OP_REM_U64); bc_u8(bc,d); bc_u8(bc,l); bc_u8(bc,r); }

/* --- Float arithmetic – f32 ---------------------------------------------- */
static void emit_add_f32(Bytecode* bc, uint8_t d, uint8_t l, uint8_t r) { bc_op(bc, OP_ADD_F32); bc_u8(bc,d); bc_u8(bc,l); bc_u8(bc,r); }
static void emit_sub_f32(Bytecode* bc, uint8_t d, uint8_t l, uint8_t r) { bc_op(bc, OP_SUB_F32); bc_u8(bc,d); bc_u8(bc,l); bc_u8(bc,r); }
static void emit_mul_f32(Bytecode* bc, uint8_t d, uint8_t l, uint8_t r) { bc_op(bc, OP_MUL_F32); bc_u8(bc,d); bc_u8(bc,l); bc_u8(bc,r); }
static void emit_div_f32(Bytecode* bc, uint8_t d, uint8_t l, uint8_t r) { bc_op(bc, OP_DIV_F32); bc_u8(bc,d); bc_u8(bc,l); bc_u8(bc,r); }

/* --- Float arithmetic – f64 ---------------------------------------------- */
static void emit_add_f64(Bytecode* bc, uint8_t d, uint8_t l, uint8_t r) { bc_op(bc, OP_ADD_F64); bc_u8(bc,d); bc_u8(bc,l); bc_u8(bc,r); }
static void emit_sub_f64(Bytecode* bc, uint8_t d, uint8_t l, uint8_t r) { bc_op(bc, OP_SUB_F64); bc_u8(bc,d); bc_u8(bc,l); bc_u8(bc,r); }
static void emit_mul_f64(Bytecode* bc, uint8_t d, uint8_t l, uint8_t r) { bc_op(bc, OP_MUL_F64); bc_u8(bc,d); bc_u8(bc,l); bc_u8(bc,r); }
static void emit_div_f64(Bytecode* bc, uint8_t d, uint8_t l, uint8_t r) { bc_op(bc, OP_DIV_F64); bc_u8(bc,d); bc_u8(bc,l); bc_u8(bc,r); }

/* --- Unary --------------------------------------------------------------- */
static void emit_neg_i32(Bytecode* bc, uint8_t d, uint8_t s) { bc_op(bc, OP_NEG_I32); bc_u8(bc,d); bc_u8(bc,s); }
static void emit_neg_i64(Bytecode* bc, uint8_t d, uint8_t s) { bc_op(bc, OP_NEG_I64); bc_u8(bc,d); bc_u8(bc,s); }
static void emit_neg_f32(Bytecode* bc, uint8_t d, uint8_t s) { bc_op(bc, OP_NEG_F32); bc_u8(bc,d); bc_u8(bc,s); }
static void emit_neg_f64(Bytecode* bc, uint8_t d, uint8_t s) { bc_op(bc, OP_NEG_F64); bc_u8(bc,d); bc_u8(bc,s); }
static void emit_not_i32(Bytecode* bc, uint8_t d, uint8_t s) { bc_op(bc, OP_NOT_I32); bc_u8(bc,d); bc_u8(bc,s); }
static void emit_not_i64(Bytecode* bc, uint8_t d, uint8_t s) { bc_op(bc, OP_NOT_I64); bc_u8(bc,d); bc_u8(bc,s); }

/* --- Bitwise – i32 ------------------------------------------------------- */
static void emit_and_i32(Bytecode* bc, uint8_t d, uint8_t l, uint8_t r) { bc_op(bc, OP_AND_I32); bc_u8(bc,d); bc_u8(bc,l); bc_u8(bc,r); }
static void emit_or_i32 (Bytecode* bc, uint8_t d, uint8_t l, uint8_t r) { bc_op(bc, OP_OR_I32);  bc_u8(bc,d); bc_u8(bc,l); bc_u8(bc,r); }
static void emit_xor_i32(Bytecode* bc, uint8_t d, uint8_t l, uint8_t r) { bc_op(bc, OP_XOR_I32); bc_u8(bc,d); bc_u8(bc,l); bc_u8(bc,r); }

/* --- Bitwise – i64 ------------------------------------------------------- */
static void emit_and_i64(Bytecode* bc, uint8_t d, uint8_t l, uint8_t r) { bc_op(bc, OP_AND_I64); bc_u8(bc,d); bc_u8(bc,l); bc_u8(bc,r); }
static void emit_or_i64 (Bytecode* bc, uint8_t d, uint8_t l, uint8_t r) { bc_op(bc, OP_OR_I64);  bc_u8(bc,d); bc_u8(bc,l); bc_u8(bc,r); }
static void emit_xor_i64(Bytecode* bc, uint8_t d, uint8_t l, uint8_t r) { bc_op(bc, OP_XOR_I64); bc_u8(bc,d); bc_u8(bc,l); bc_u8(bc,r); }

/* --- Shifts – i32  (amt register, masked to low 5 bits by VM) ------------ */
static void emit_shl_i32 (Bytecode* bc, uint8_t d, uint8_t v, uint8_t a) { bc_op(bc, OP_SHL_I32);  bc_u8(bc,d); bc_u8(bc,v); bc_u8(bc,a); }
static void emit_shr_i32 (Bytecode* bc, uint8_t d, uint8_t v, uint8_t a) { bc_op(bc, OP_SHR_I32);  bc_u8(bc,d); bc_u8(bc,v); bc_u8(bc,a); }
static void emit_ushr_i32(Bytecode* bc, uint8_t d, uint8_t v, uint8_t a) { bc_op(bc, OP_USHR_I32); bc_u8(bc,d); bc_u8(bc,v); bc_u8(bc,a); }

/* --- Shifts – i64  (amt register, masked to low 6 bits by VM) ------------ */
static void emit_shl_i64 (Bytecode* bc, uint8_t d, uint8_t v, uint8_t a) { bc_op(bc, OP_SHL_I64);  bc_u8(bc,d); bc_u8(bc,v); bc_u8(bc,a); }
static void emit_shr_i64 (Bytecode* bc, uint8_t d, uint8_t v, uint8_t a) { bc_op(bc, OP_SHR_I64);  bc_u8(bc,d); bc_u8(bc,v); bc_u8(bc,a); }
static void emit_ushr_i64(Bytecode* bc, uint8_t d, uint8_t v, uint8_t a) { bc_op(bc, OP_USHR_I64); bc_u8(bc,d); bc_u8(bc,v); bc_u8(bc,a); }

/* --- Comparisons: dst.i32 = -1 | 0 | +1 ---------------------------------- */
static void emit_cmp_i32(Bytecode* bc, uint8_t d, uint8_t l, uint8_t r) { bc_op(bc, OP_CMP_I32); bc_u8(bc,d); bc_u8(bc,l); bc_u8(bc,r); }
static void emit_cmp_i64(Bytecode* bc, uint8_t d, uint8_t l, uint8_t r) { bc_op(bc, OP_CMP_I64); bc_u8(bc,d); bc_u8(bc,l); bc_u8(bc,r); }
static void emit_cmp_f32(Bytecode* bc, uint8_t d, uint8_t l, uint8_t r) { bc_op(bc, OP_CMP_F32); bc_u8(bc,d); bc_u8(bc,l); bc_u8(bc,r); }
static void emit_cmp_f64(Bytecode* bc, uint8_t d, uint8_t l, uint8_t r) { bc_op(bc, OP_CMP_F64); bc_u8(bc,d); bc_u8(bc,l); bc_u8(bc,r); }
static void emit_cmp_f32_gt(Bytecode* bc, uint8_t d, uint8_t l, uint8_t r) { bc_op(bc, OP_CMP_F32_GT); bc_u8(bc,d); bc_u8(bc,l); bc_u8(bc,r); }

/* --- Type conversions ----------------------------------------------------- */
static void emit_i32_to_i8 (Bytecode* bc, uint8_t d, uint8_t s) { bc_op(bc, OP_I32_TO_I8);  bc_u8(bc,d); bc_u8(bc,s); }
static void emit_i32_to_i16(Bytecode* bc, uint8_t d, uint8_t s) { bc_op(bc, OP_I32_TO_I16); bc_u8(bc,d); bc_u8(bc,s); }
static void emit_i32_to_i64(Bytecode* bc, uint8_t d, uint8_t s) { bc_op(bc, OP_I32_TO_I64); bc_u8(bc,d); bc_u8(bc,s); }
static void emit_i32_to_f32(Bytecode* bc, uint8_t d, uint8_t s) { bc_op(bc, OP_I32_TO_F32); bc_u8(bc,d); bc_u8(bc,s); }
static void emit_i32_to_f64(Bytecode* bc, uint8_t d, uint8_t s) { bc_op(bc, OP_I32_TO_F64); bc_u8(bc,d); bc_u8(bc,s); }
static void emit_i64_to_i32(Bytecode* bc, uint8_t d, uint8_t s) { bc_op(bc, OP_I64_TO_I32); bc_u8(bc,d); bc_u8(bc,s); }
static void emit_i64_to_f32(Bytecode* bc, uint8_t d, uint8_t s) { bc_op(bc, OP_I64_TO_F32); bc_u8(bc,d); bc_u8(bc,s); }
static void emit_i64_to_f64(Bytecode* bc, uint8_t d, uint8_t s) { bc_op(bc, OP_I64_TO_F64); bc_u8(bc,d); bc_u8(bc,s); }
static void emit_f32_to_i32(Bytecode* bc, uint8_t d, uint8_t s) { bc_op(bc, OP_F32_TO_I32); bc_u8(bc,d); bc_u8(bc,s); }
static void emit_f32_to_i64(Bytecode* bc, uint8_t d, uint8_t s) { bc_op(bc, OP_F32_TO_I64); bc_u8(bc,d); bc_u8(bc,s); }
static void emit_f32_to_f64(Bytecode* bc, uint8_t d, uint8_t s) { bc_op(bc, OP_F32_TO_F64); bc_u8(bc,d); bc_u8(bc,s); }
static void emit_f64_to_i32(Bytecode* bc, uint8_t d, uint8_t s) { bc_op(bc, OP_F64_TO_I32); bc_u8(bc,d); bc_u8(bc,s); }
static void emit_f64_to_i64(Bytecode* bc, uint8_t d, uint8_t s) { bc_op(bc, OP_F64_TO_I64); bc_u8(bc,d); bc_u8(bc,s); }
static void emit_f64_to_f32(Bytecode* bc, uint8_t d, uint8_t s) { bc_op(bc, OP_F64_TO_F32); bc_u8(bc,d); bc_u8(bc,s); }

/* --- Memory loads --------------------------------------------------------- */
static void emit_load8    (Bytecode* bc, uint8_t d, uint8_t a) { bc_op(bc, OP_LOAD8);     bc_u8(bc,d); bc_u8(bc,a); }
static void emit_load8s   (Bytecode* bc, uint8_t d, uint8_t a) { bc_op(bc, OP_LOAD8S);    bc_u8(bc,d); bc_u8(bc,a); }
static void emit_load16   (Bytecode* bc, uint8_t d, uint8_t a) { bc_op(bc, OP_LOAD16);    bc_u8(bc,d); bc_u8(bc,a); }
static void emit_load16s  (Bytecode* bc, uint8_t d, uint8_t a) { bc_op(bc, OP_LOAD16S);   bc_u8(bc,d); bc_u8(bc,a); }
static void emit_load32   (Bytecode* bc, uint8_t d, uint8_t a) { bc_op(bc, OP_LOAD32);    bc_u8(bc,d); bc_u8(bc,a); }
static void emit_load32s  (Bytecode* bc, uint8_t d, uint8_t a) { bc_op(bc, OP_LOAD32S);   bc_u8(bc,d); bc_u8(bc,a); }
static void emit_load64   (Bytecode* bc, uint8_t d, uint8_t a) { bc_op(bc, OP_LOAD64);    bc_u8(bc,d); bc_u8(bc,a); }
static void emit_load_ptr (Bytecode* bc, uint8_t d, uint8_t a) { bc_op(bc, OP_LOAD_PTR);  bc_u8(bc,d); bc_u8(bc,a); }

/* --- Memory stores -------------------------------------------------------- */
static void emit_store8   (Bytecode* bc, uint8_t a, uint8_t s) { bc_op(bc, OP_STORE8);    bc_u8(bc,a); bc_u8(bc,s); }
static void emit_store16  (Bytecode* bc, uint8_t a, uint8_t s) { bc_op(bc, OP_STORE16);   bc_u8(bc,a); bc_u8(bc,s); }
static void emit_store32  (Bytecode* bc, uint8_t a, uint8_t s) { bc_op(bc, OP_STORE32);   bc_u8(bc,a); bc_u8(bc,s); }
static void emit_store64  (Bytecode* bc, uint8_t a, uint8_t s) { bc_op(bc, OP_STORE64);   bc_u8(bc,a); bc_u8(bc,s); }
static void emit_store_ptr(Bytecode* bc, uint8_t a, uint8_t s) { bc_op(bc, OP_STORE_PTR); bc_u8(bc,a); bc_u8(bc,s); }

/* --- LEA [dst:u8][base:u8][offset:i32] ----------------------------------- */
static void emit_lea(Bytecode* bc, uint8_t dst, uint8_t base, int32_t offset)
{
    bc_op(bc, OP_LEA); bc_u8(bc, dst); bc_u8(bc, base); bc_i32(bc, offset);
}

/* --- Unconditional branches ----------------------------------------------- */
/* offset is relative to the next instruction (byte after operand) */
static void emit_goto   (Bytecode* bc, int8_t  off) { bc_op(bc, OP_GOTO);    bc_i8 (bc, off); }
static void emit_goto_16(Bytecode* bc, int16_t off) { bc_op(bc, OP_GOTO_16); bc_i16(bc, off); }
static void emit_goto_32(Bytecode* bc, int32_t off) { bc_op(bc, OP_GOTO_32); bc_i32(bc, off); }

/*
 * emit_goto_16_fwd — emit GOTO_16 with a placeholder displacement.
 * Returns the position of the displacement bytes for later patching.
 */
static uint32_t emit_goto_16_fwd(Bytecode* bc)
{
    bc_op(bc, OP_GOTO_16);
    uint32_t p = bc->size;
    bc_i16(bc, 0);
    return p;
}

/*
 * emit_goto_back — emit GOTO_16 jumping backward to an already-emitted
 * position (target_pos = bc->size at the start of the loop body).
 *
 * Uses GOTO_16 unconditionally for predictable instruction size.
 */
static void emit_goto_back(Bytecode* bc, uint32_t target_pos)
{
    /* After fully emitting GOTO_16 [i16]: total 4 bytes.
     * next_pc = bc->size + 4
     * offset  = target_pos - next_pc */
    int32_t off = (int32_t)target_pos - ((int32_t)bc->size + 2 + 2);
    bc_op(bc, OP_GOTO_16); bc_i16(bc, (int16_t)off);
}

/* --- Conditional branches: compare two i32 registers --------------------- */
/* Each returns the displacement patch position.                             */
static uint32_t emit_if_eq(Bytecode* bc, uint8_t a, uint8_t b) { bc_op(bc,OP_IF_EQ); bc_u8(bc,a); bc_u8(bc,b); uint32_t p=bc->size; bc_i16(bc,0); return p; }
static uint32_t emit_if_ne(Bytecode* bc, uint8_t a, uint8_t b) { bc_op(bc,OP_IF_NE); bc_u8(bc,a); bc_u8(bc,b); uint32_t p=bc->size; bc_i16(bc,0); return p; }
static uint32_t emit_if_lt(Bytecode* bc, uint8_t a, uint8_t b) { bc_op(bc,OP_IF_LT); bc_u8(bc,a); bc_u8(bc,b); uint32_t p=bc->size; bc_i16(bc,0); return p; }
static uint32_t emit_if_ge(Bytecode* bc, uint8_t a, uint8_t b) { bc_op(bc,OP_IF_GE); bc_u8(bc,a); bc_u8(bc,b); uint32_t p=bc->size; bc_i16(bc,0); return p; }
static uint32_t emit_if_gt(Bytecode* bc, uint8_t a, uint8_t b) { bc_op(bc,OP_IF_GT); bc_u8(bc,a); bc_u8(bc,b); uint32_t p=bc->size; bc_i16(bc,0); return p; }
static uint32_t emit_if_le(Bytecode* bc, uint8_t a, uint8_t b) { bc_op(bc,OP_IF_LE); bc_u8(bc,a); bc_u8(bc,b); uint32_t p=bc->size; bc_i16(bc,0); return p; }

/* --- Conditional branches: compare i32 register vs zero ------------------ */
static uint32_t emit_if_eqz(Bytecode* bc, uint8_t a) { bc_op(bc,OP_IF_EQZ); bc_u8(bc,a); uint32_t p=bc->size; bc_i16(bc,0); return p; }
static uint32_t emit_if_nez(Bytecode* bc, uint8_t a) { bc_op(bc,OP_IF_NEZ); bc_u8(bc,a); uint32_t p=bc->size; bc_i16(bc,0); return p; }
static uint32_t emit_if_ltz(Bytecode* bc, uint8_t a) { bc_op(bc,OP_IF_LTZ); bc_u8(bc,a); uint32_t p=bc->size; bc_i16(bc,0); return p; }
static uint32_t emit_if_gez(Bytecode* bc, uint8_t a) { bc_op(bc,OP_IF_GEZ); bc_u8(bc,a); uint32_t p=bc->size; bc_i16(bc,0); return p; }
static uint32_t emit_if_gtz(Bytecode* bc, uint8_t a) { bc_op(bc,OP_IF_GTZ); bc_u8(bc,a); uint32_t p=bc->size; bc_i16(bc,0); return p; }
static uint32_t emit_if_lez(Bytecode* bc, uint8_t a) { bc_op(bc,OP_IF_LEZ); bc_u8(bc,a); uint32_t p=bc->size; bc_i16(bc,0); return p; }

/* =========================================================================
 * Branch patching utilities
 * ====================================================================== */

/*
 * bc_patch_i16 — write a signed 16-bit displacement into a previously
 * reserved slot at patch_pos.
 */
static void bc_patch_i16(Bytecode* bc, uint32_t patch_pos, int16_t offset)
{
    bc->data[patch_pos]     = (uint8_t)(offset);
    bc->data[patch_pos + 1] = (uint8_t)((uint16_t)offset >> 8);
}

/*
 * bc_patch_here — patch a forward branch so it jumps to the CURRENT buffer
 * position (i.e. the instruction emitted next).
 *
 * patch_pos is the value returned by emit_if_*() or emit_goto_16_fwd().
 * The displacement field occupies [patch_pos .. patch_pos+1].
 * next_pc (from the branch's perspective) = patch_pos + 2.
 * target  = bc->size  (the instruction we're about to emit)
 * offset  = target - next_pc
 */
static void bc_patch_here(Bytecode* bc, uint32_t patch_pos)
{
    int32_t off = (int32_t)bc->size - (int32_t)(patch_pos + 2);
    bc_patch_i16(bc, patch_pos, (int16_t)off);
}

/*
 * bc_patch_back — patch a branch so it jumps backward to target_pos.
 * target_pos is the bc->size captured at the top of the loop body.
 */
static void bc_patch_back(Bytecode* bc, uint32_t patch_pos, uint32_t target_pos)
{
    int32_t off = (int32_t)target_pos - (int32_t)(patch_pos + 2);
    bc_patch_i16(bc, patch_pos, (int16_t)off);
}

/* =========================================================================
 * Native function call emitters
 *
 * CALL      [dst:u8][id:u32][argc:u8][r0..rN:u8]
 * CALL_VOID [id:u32][argc:u8][r0..rN:u8]
 * ====================================================================== */

/* Fixed-arity convenience versions (0..4 args) — no varargs needed. */

static void emit_call0(Bytecode* bc, uint8_t dst, uint32_t id)
{ bc_op(bc,OP_CALL); bc_u8(bc,dst); bc_u32(bc,id); bc_u8(bc,0); }

static void emit_call1(Bytecode* bc, uint8_t dst, uint32_t id, uint8_t r0)
{ bc_op(bc,OP_CALL); bc_u8(bc,dst); bc_u32(bc,id); bc_u8(bc,1); bc_u8(bc,r0); }

static void emit_call2(Bytecode* bc, uint8_t dst, uint32_t id, uint8_t r0, uint8_t r1)
{ bc_op(bc,OP_CALL); bc_u8(bc,dst); bc_u32(bc,id); bc_u8(bc,2); bc_u8(bc,r0); bc_u8(bc,r1); }

static void emit_call3(Bytecode* bc, uint8_t dst, uint32_t id, uint8_t r0, uint8_t r1, uint8_t r2)
{ bc_op(bc,OP_CALL); bc_u8(bc,dst); bc_u32(bc,id); bc_u8(bc,3); bc_u8(bc,r0); bc_u8(bc,r1); bc_u8(bc,r2); }

static void emit_call4(Bytecode* bc, uint8_t dst, uint32_t id, uint8_t r0, uint8_t r1, uint8_t r2, uint8_t r3)
{ bc_op(bc,OP_CALL); bc_u8(bc,dst); bc_u32(bc,id); bc_u8(bc,4); bc_u8(bc,r0); bc_u8(bc,r1); bc_u8(bc,r2); bc_u8(bc,r3); }

static void emit_call_void0(Bytecode* bc, uint32_t id)
{ bc_op(bc,OP_CALL_VOID); bc_u32(bc,id); bc_u8(bc,0); }

static void emit_call_void1(Bytecode* bc, uint32_t id, uint8_t r0)
{ bc_op(bc,OP_CALL_VOID); bc_u32(bc,id); bc_u8(bc,1); bc_u8(bc,r0); }

static void emit_call_void2(Bytecode* bc, uint32_t id, uint8_t r0, uint8_t r1)
{ bc_op(bc,OP_CALL_VOID); bc_u32(bc,id); bc_u8(bc,2); bc_u8(bc,r0); bc_u8(bc,r1); }

static void emit_call_void3(Bytecode* bc, uint32_t id, uint8_t r0, uint8_t r1, uint8_t r2)
{ bc_op(bc,OP_CALL_VOID); bc_u32(bc,id); bc_u8(bc,3); bc_u8(bc,r0); bc_u8(bc,r1); bc_u8(bc,r2); }

/* Array-based version for callers with > 4 args */
static void emit_call_n(Bytecode* bc, uint8_t dst, uint32_t id,
                        uint8_t argc, const uint8_t* args)
{
    uint8_t i;
    bc_op(bc,OP_CALL); bc_u8(bc,dst); bc_u32(bc,id); bc_u8(bc,argc);
    for (i = 0; i < argc; i++) bc_u8(bc, args[i]);
}

static void emit_call_void_n(Bytecode* bc, uint32_t id,
                             uint8_t argc, const uint8_t* args)
{
    uint8_t i;
    bc_op(bc,OP_CALL_VOID); bc_u32(bc,id); bc_u8(bc,argc);
    for (i = 0; i < argc; i++) bc_u8(bc, args[i]);
}

/* --- Subroutines & Bytecode Calls ---------------------------------------- */
static void emit_call_bc_n(Bytecode* bc, uint8_t dst, uint32_t target_pc,
                            uint8_t argc, const uint8_t* args)
{
    uint8_t i;
    bc_op(bc, OP_CALL_BC); bc_u8(bc, dst); bc_u32(bc, target_pc); bc_u8(bc, argc);
    for (i = 0; i < argc; i++) bc_u8(bc, args[i]);
}

static void emit_call_bc_0(Bytecode* bc, uint8_t dst, uint32_t target_pc)
{
    emit_call_bc_n(bc, dst, target_pc, 0, NULL);
}

static void emit_call_bc_1(Bytecode* bc, uint8_t dst, uint32_t target_pc, uint8_t a0)
{
    const uint8_t args[1] = { a0 };
    emit_call_bc_n(bc, dst, target_pc, 1, args);
}

static void emit_call_bc_2(Bytecode* bc, uint8_t dst, uint32_t target_pc, uint8_t a0, uint8_t a1)
{
    const uint8_t args[2] = { a0, a1 };
    emit_call_bc_n(bc, dst, target_pc, 2, args);
}

static void emit_ret(Bytecode* bc, uint8_t src) { bc_op(bc, OP_RET); bc_u8(bc, src); }
static void emit_ret_void(Bytecode* bc) { bc_op(bc, OP_RET); bc_u8(bc, 0xFF); }

/* --- Return -------------------------------------------------------------- */
static void emit_return_void(Bytecode* bc) { bc_op(bc, OP_RETURN_VOID); }
static void emit_return(Bytecode* bc, uint8_t src) { bc_op(bc, OP_RETURN); bc_u8(bc, src); }

/* =========================================================================
 * Execution helpers
 * ====================================================================== */

/*
 * bc_run — execute bytecode with a zeroed register file of reg_count regs.
 * Returns VMError; ctx->result holds the value from OP_RETURN (if any).
 */
static VMError bc_run(VMContext* ctx, Bytecode* bc, uint32_t reg_count)
{
    VMRegister regs[256];
    if (reg_count == 0 || reg_count > 256) reg_count = 256;
    memset(regs, 0, sizeof(VMRegister) * reg_count);
    return vm_execute(ctx, regs, reg_count, 0, bc->data, bc->size);
}

/*
 * bc_run_regs — execute with a caller-provided register file.
 * Useful when input registers must be pre-populated (e.g. pointer args).
 */
static VMError bc_run_regs(VMContext* ctx, Bytecode* bc,
                           VMRegister* regs, uint32_t reg_count)
{
    return vm_execute(ctx, regs, reg_count, 0, bc->data, bc->size);
}

/* =========================================================================
 * Lightweight test framework
 *
 * g_pass and g_fail are static — unique per translation unit (one program).
 * ====================================================================== */

static int g_pass = 0;
static int g_fail = 0;

#define TEST_SECTION(name) \
    printf("\n--- %s ---\n", name)

static void check_i32(const char* label, int32_t got, int32_t expected)
{
    if (got == expected) {
        printf("  [PASS] %s\n", label);
        g_pass++;
    } else {
        printf("  [FAIL] %s  expected %d  got %d\n", label, expected, got);
        g_fail++;
    }
}

static void check_u32(const char* label, uint32_t got, uint32_t expected)
{
    if (got == expected) {
        printf("  [PASS] %s\n", label);
        g_pass++;
    } else {
        printf("  [FAIL] %s  expected 0x%X  got 0x%X\n", label, expected, got);
        g_fail++;
    }
}

static void check_i64(const char* label, int64_t got, int64_t expected)
{
    if (got == expected) {
        printf("  [PASS] %s\n", label);
        g_pass++;
    } else {
        printf("  [FAIL] %s  expected %lld  got %lld\n", label,
               (long long)expected, (long long)got);
        g_fail++;
    }
}

static void check_u64(const char* label, uint64_t got, uint64_t expected)
{
    if (got == expected) {
        printf("  [PASS] %s\n", label);
        g_pass++;
    } else {
        printf("  [FAIL] %s  expected 0x%llX  got 0x%llX\n", label,
               (unsigned long long)expected, (unsigned long long)got);
        g_fail++;
    }
}

static void check_f32(const char* label, float got, float expected, float eps)
{
    float diff = got - expected;
    if (diff < 0.0f) diff = -diff;
    if (diff <= eps) {
        printf("  [PASS] %s\n", label);
        g_pass++;
    } else {
        printf("  [FAIL] %s  expected %.6f  got %.6f\n",
               label, (double)expected, (double)got);
        g_fail++;
    }
}

static void check_f64(const char* label, double got, double expected, double eps)
{
    double diff = got - expected;
    if (diff < 0.0) diff = -diff;
    if (diff <= eps) {
        printf("  [PASS] %s\n", label);
        g_pass++;
    } else {
        printf("  [FAIL] %s  expected %.10f  got %.10f\n",
               label, expected, got);
        g_fail++;
    }
}

static void check_f32_nan(const char* label, float got)
{
    if (isnan(got)) { printf("  [PASS] %s\n", label); g_pass++; }
    else            { printf("  [FAIL] %s  expected NaN  got %f\n", label, (double)got); g_fail++; }
}

static void check_f64_nan(const char* label, double got)
{
    if (isnan(got)) { printf("  [PASS] %s\n", label); g_pass++; }
    else            { printf("  [FAIL] %s  expected NaN  got %f\n", label, got); g_fail++; }
}

static void check_err(const char* label, VMError got, VMError expected)
{
    if (got == expected) {
        printf("  [PASS] %s\n", label);
        g_pass++;
    } else {
        printf("  [FAIL] %s  expected err=%d  got err=%d\n",
               label, (int)expected, (int)got);
        g_fail++;
    }
}

static void check_bool(const char* label, int condition)
{
    if (condition) { printf("  [PASS] %s\n", label); g_pass++; }
    else           { printf("  [FAIL] %s\n", label); g_fail++; }
}

static void print_summary(void)
{
    int total = g_pass + g_fail;
    printf("\n========================================\n");
    printf("Results: %d / %d passed", g_pass, total);
    if (g_fail == 0)
        printf("  (all tests passed)\n");
    else
        printf("  (%d FAILED)\n", g_fail);
    printf("========================================\n");
}

#ifdef _MSC_VER
#  pragma warning(pop)
#elif defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif

#endif /* VM_BUILDER_H */
