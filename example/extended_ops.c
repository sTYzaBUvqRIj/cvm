/*
 * Copyright (C) 2026 CVM Contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * extended_ops.c -- tests for extended opcodes (WebAssembly-level completeness)
 */

#include "../vm.h"
#include "../vm_builder.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <limits.h>

/* =========================================================================
 * Test harness
 * ====================================================================== */

/* g_pass / g_fail / check_* / print_summary are provided by vm_builder.h */

static VMError run8(VMContext* ctx, Bytecode* bc)
{
    return bc_run(ctx, bc, 8);
}


/* =========================================================================
 * 1. Unsigned comparisons
 * ====================================================================== */

static void test_cmp_unsigned(void)
{
    printf("\n--- CMP_U32 / CMP_U64 ---\n");
    VMContext ctx; Bytecode bc;

    /* 0xFFFFFFFF > 1 as unsigned (= -1 as signed, so signed CMP would give -1 not +1) */
    vm_init(&ctx); bc_init(&bc);
    emit_const_i32(&bc, 0, (int32_t)0xFFFFFFFF);
    emit_const_i32(&bc, 1, 1);
    emit_cmp_u32(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    run8(&ctx, &bc);
    check_i32("CMP_U32(0xFFFFFFFF,1)=+1", ctx.result.i32, 1);
    vm_destroy(&ctx);

    vm_init(&ctx); bc_init(&bc);
    emit_const_i32(&bc, 0, 1);
    emit_const_i32(&bc, 1, (int32_t)0xFFFFFFFF);
    emit_cmp_u32(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    run8(&ctx, &bc);
    check_i32("CMP_U32(1,0xFFFFFFFF)=-1", ctx.result.i32, -1);
    vm_destroy(&ctx);

    vm_init(&ctx); bc_init(&bc);
    emit_const_i32(&bc, 0, 100);
    emit_const_i32(&bc, 1, 100);
    emit_cmp_u32(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    run8(&ctx, &bc);
    check_i32("CMP_U32(100,100)=0", ctx.result.i32, 0);
    vm_destroy(&ctx);

    /* CMP_U64 */
    vm_init(&ctx); bc_init(&bc);
    emit_const_i64(&bc, 0, (int64_t)UINT64_C(0xFFFFFFFFFFFFFFFF));
    emit_const_i64(&bc, 1, 1LL);
    emit_cmp_u64(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    run8(&ctx, &bc);
    check_i32("CMP_U64(MAX64,1)=+1", ctx.result.i32, 1);
    vm_destroy(&ctx);
}

/* =========================================================================
 * 2. Unsigned conditional branches
 * ====================================================================== */

static void test_unsigned_branches(void)
{
    printf("\n--- IF_ULT / IF_UGE / IF_UGT / IF_ULE ---\n");
    VMContext ctx; Bytecode bc;
    uint32_t skip, end;

#define BRANCH_TEST(lbl, emitter, A, B, expect) do {        \
    vm_init(&ctx); bc_init(&bc);                   \
    emit_const_i32(&bc, 0, (int32_t)(A));                   \
    emit_const_i32(&bc, 1, (int32_t)(B));                   \
    skip = emitter(&bc, 0, 1);                               \
    emit_const_i32(&bc, 2, 0);                              \
    end = emit_goto_16_fwd(&bc);                             \
    bc_patch_here(&bc, skip);                                \
    emit_const_i32(&bc, 2, 1);                              \
    bc_patch_here(&bc, end);                                 \
    emit_return(&bc, 2);                                     \
    run8(&ctx, &bc);                                         \
    check_i32(lbl, ctx.result.i32, (expect));                \
    vm_destroy(&ctx);                                        \
} while(0)

    BRANCH_TEST("IF_ULT taken (1u < 0xFFFF...)", emit_if_ult, 1u, 0xFFFFFFFFu, 1);
    BRANCH_TEST("IF_ULT NOT taken (0xFF... < 1)", emit_if_ult, 0xFFFFFFFFu, 1u, 0);
    BRANCH_TEST("IF_UGE taken (0xFF... >= 1)",    emit_if_uge, 0xFFFFFFFFu, 1u, 1);
    BRANCH_TEST("IF_UGE NOT taken (1 >= 0xFF...)",emit_if_uge, 1u, 0xFFFFFFFFu, 0);
    BRANCH_TEST("IF_UGT taken (0xFF... > 1)",     emit_if_ugt, 0xFFFFFFFFu, 1u, 1);
    BRANCH_TEST("IF_UGT NOT taken (1 > 1)",       emit_if_ugt, 1u, 1u, 0);
    BRANCH_TEST("IF_ULE taken (1 <= 1)",           emit_if_ule, 1u, 1u, 1);
    BRANCH_TEST("IF_ULE NOT taken (5 <= 4)",       emit_if_ule, 5u, 4u, 0);
#undef BRANCH_TEST
}

/* =========================================================================
 * 3. SELECT
 * ====================================================================== */

static void test_select(void)
{
    printf("\n--- SELECT ---\n");
    VMContext ctx; Bytecode bc;

#define SEL_TEST(lbl, cond_v, expect) do {       \
    vm_init(&ctx); bc_init(&bc);        \
    emit_const_i32(&bc, 1, 100);                 \
    emit_const_i32(&bc, 2, 200);                 \
    emit_const_i32(&bc, 3, (cond_v));            \
    emit_select(&bc, 0, 1, 2, 3);               \
    emit_return(&bc, 0);                          \
    run8(&ctx, &bc);                              \
    check_i32(lbl, ctx.result.i32, (expect));    \
    vm_destroy(&ctx);                             \
} while(0)

    SEL_TEST("SELECT cond=1 -> 100",   1,   100);
    SEL_TEST("SELECT cond=0 -> 200",   0,   200);
    SEL_TEST("SELECT cond=-5 -> 100", -5,   100);
#undef SEL_TEST
}

/* =========================================================================
 * 4. CLZ / CTZ / POPCNT
 * ====================================================================== */

static void test_clz_ctz_popcnt(void)
{
    printf("\n--- CLZ / CTZ / POPCNT ---\n");
    VMContext ctx; Bytecode bc;

#define U32_UNARY(lbl, emitter, inp, exp) do {   \
    vm_init(&ctx); bc_init(&bc);        \
    emit_const_i32(&bc, 1, (int32_t)(inp));      \
    emitter(&bc, 0, 1);                           \
    emit_return(&bc, 0);                          \
    run8(&ctx, &bc);                              \
    check_u32(lbl, ctx.result.u32, (uint32_t)(exp)); \
    vm_destroy(&ctx);                             \
} while(0)

    U32_UNARY("CLZ_I32(0)=32",         emit_clz_i32,   0u,          32);
    U32_UNARY("CLZ_I32(1)=31",         emit_clz_i32,   1u,          31);
    U32_UNARY("CLZ_I32(0x80000000)=0", emit_clz_i32,   0x80000000u, 0);
    U32_UNARY("CLZ_I32(0xFFFFFFFF)=0", emit_clz_i32,   0xFFFFFFFFu, 0);

    U32_UNARY("CTZ_I32(0)=32",         emit_ctz_i32,   0u,          32);
    U32_UNARY("CTZ_I32(1)=0",          emit_ctz_i32,   1u,          0);
    U32_UNARY("CTZ_I32(8)=3",          emit_ctz_i32,   8u,          3);
    U32_UNARY("CTZ_I32(0x80000000)=31",emit_ctz_i32,   0x80000000u, 31);

    U32_UNARY("POPCNT_I32(0)=0",       emit_popcnt_i32, 0u,          0);
    U32_UNARY("POPCNT_I32(0xFF)=8",    emit_popcnt_i32, 0xFFu,       8);
    U32_UNARY("POPCNT(0xAAAAAAAA)=16", emit_popcnt_i32, 0xAAAAAAAAu, 16);
    U32_UNARY("POPCNT(0xFFFFFFFF)=32", emit_popcnt_i32, 0xFFFFFFFFu, 32);
#undef U32_UNARY

    /* CLZ_I64 */
    vm_init(&ctx); bc_init(&bc);
    emit_const_i64(&bc, 1, 1LL);
    emit_clz_i64(&bc, 0, 1);
    emit_return(&bc, 0);
    run8(&ctx, &bc);
    check_u32("CLZ_I64(1)=63", ctx.result.u32, 63);
    vm_destroy(&ctx);

    vm_init(&ctx); bc_init(&bc);
    emit_const_i64(&bc, 1, 0LL);
    emit_clz_i64(&bc, 0, 1);
    emit_return(&bc, 0);
    run8(&ctx, &bc);
    check_u32("CLZ_I64(0)=64", ctx.result.u32, 64);
    vm_destroy(&ctx);

    vm_init(&ctx); bc_init(&bc);
    emit_const_i64(&bc, 1, (int64_t)UINT64_C(0xFFFFFFFFFFFFFFFF));
    emit_popcnt_i64(&bc, 0, 1);
    emit_return(&bc, 0);
    run8(&ctx, &bc);
    check_u32("POPCNT_I64(allones)=64", ctx.result.u32, 64);
    vm_destroy(&ctx);
}

/* =========================================================================
 * 5. ROTL / ROTR
 * ====================================================================== */

static void test_rotl_rotr(void)
{
    printf("\n--- ROTL / ROTR ---\n");
    VMContext ctx; Bytecode bc;

#define ROT32(lbl, emitter, v, a, exp) do {      \
    vm_init(&ctx); bc_init(&bc);        \
    emit_const_i32(&bc, 1, (int32_t)(v));        \
    emit_const_i32(&bc, 2, (int32_t)(a));        \
    emitter(&bc, 0, 1, 2);                        \
    emit_return(&bc, 0);                          \
    run8(&ctx, &bc);                              \
    check_u32(lbl, ctx.result.u32, (uint32_t)(exp)); \
    vm_destroy(&ctx);                             \
} while(0)

    ROT32("ROTL_I32(1,1)=2",           emit_rotl_i32, 1u,          1,  2u);
    ROT32("ROTL_I32(1,8)=256",         emit_rotl_i32, 1u,          8,  256u);
    ROT32("ROTL_I32(0x80000000,1)=1",  emit_rotl_i32, 0x80000000u, 1,  1u);
    ROT32("ROTR_I32(2,1)=1",           emit_rotr_i32, 2u,          1,  1u);
    ROT32("ROTR_I32(1,1)=0x80000000",  emit_rotr_i32, 1u,          1,  0x80000000u);
#undef ROT32

    /* ROTL_I64 */
    vm_init(&ctx); bc_init(&bc);
    emit_const_i64(&bc, 1, 1LL);
    emit_const_i32(&bc, 2, 63);
    emit_rotl_i64(&bc, 0, 1, 2);
    emit_return(&bc, 0);
    run8(&ctx, &bc);
    check_u64("ROTL_I64(1,63)=0x8000...", ctx.result.u64, UINT64_C(0x8000000000000000));
    vm_destroy(&ctx);
}

/* =========================================================================
 * 6. Integer ABS / MIN / MAX
 * ====================================================================== */

static void test_abs_min_max_int(void)
{
    printf("\n--- Integer ABS / MIN / MAX ---\n");
    VMContext ctx; Bytecode bc;

    /* ABS */
    vm_init(&ctx); bc_init(&bc);
    emit_const_i32(&bc, 1, -42);
    emit_abs_i32(&bc, 0, 1);
    emit_return(&bc, 0);
    run8(&ctx, &bc);
    check_i32("ABS_I32(-42)=42", ctx.result.i32, 42);
    vm_destroy(&ctx);

    vm_init(&ctx); bc_init(&bc);
    emit_const_i64(&bc, 1, -999999999LL);
    emit_abs_i64(&bc, 0, 1);
    emit_return(&bc, 0);
    run8(&ctx, &bc);
    check_i64("ABS_I64(-999999999)", ctx.result.i64, 999999999LL);
    vm_destroy(&ctx);

#define MINMAX_I32(lbl, emitter, a, b, exp) do { \
    vm_init(&ctx); bc_init(&bc);        \
    emit_const_i32(&bc, 1, (int32_t)(a));        \
    emit_const_i32(&bc, 2, (int32_t)(b));        \
    emitter(&bc, 0, 1, 2);                        \
    emit_return(&bc, 0);                          \
    run8(&ctx, &bc);                              \
    check_i32(lbl, ctx.result.i32, (int32_t)(exp)); \
    vm_destroy(&ctx);                             \
} while(0)

    MINMAX_I32("MIN_I32(-5,3)=-5",   emit_min_i32, -5, 3, -5);
    MINMAX_I32("MAX_I32(-5,3)=3",    emit_max_i32, -5, 3, 3);
#undef MINMAX_I32

    /* MIN/MAX_U32 (signed values but unsigned comparison) */
    vm_init(&ctx); bc_init(&bc);
    emit_const_i32(&bc, 1, (int32_t)0xFFFFFFFF);
    emit_const_i32(&bc, 2, 1);
    emit_min_u32(&bc, 0, 1, 2);
    emit_return(&bc, 0);
    run8(&ctx, &bc);
    check_u32("MIN_U32(0xFFFF...,1)=1", ctx.result.u32, 1u);
    vm_destroy(&ctx);

    vm_init(&ctx); bc_init(&bc);
    emit_const_i32(&bc, 1, (int32_t)0xFFFFFFFF);
    emit_const_i32(&bc, 2, 1);
    emit_max_u32(&bc, 0, 1, 2);
    emit_return(&bc, 0);
    run8(&ctx, &bc);
    check_u32("MAX_U32(0xFFFF...,1)=0xFFFF...", ctx.result.u32, 0xFFFFFFFFu);
    vm_destroy(&ctx);

    /* MIN/MAX_I64 */
    vm_init(&ctx); bc_init(&bc);
    emit_const_i64(&bc, 1, (int64_t)INT64_MIN);
    emit_const_i64(&bc, 2, 0LL);
    emit_min_i64(&bc, 0, 1, 2);
    emit_return(&bc, 0);
    run8(&ctx, &bc);
    check_i64("MIN_I64(INT64_MIN,0)=INT64_MIN", ctx.result.i64, (int64_t)INT64_MIN);
    vm_destroy(&ctx);

    vm_init(&ctx); bc_init(&bc);
    emit_const_i64(&bc, 1, (int64_t)INT64_MAX);
    emit_const_i64(&bc, 2, 0LL);
    emit_max_i64(&bc, 0, 1, 2);
    emit_return(&bc, 0);
    run8(&ctx, &bc);
    check_i64("MAX_I64(INT64_MAX,0)=INT64_MAX", ctx.result.i64, (int64_t)INT64_MAX);
    vm_destroy(&ctx);
}

/* =========================================================================
 * 7. MULH
 * ====================================================================== */

static void test_mulh(void)
{
    printf("\n--- MULH ---\n");
    VMContext ctx; Bytecode bc;

    /* MULH_I32: (0x10000 * 0x10000) = 0x100000000 -> high32 = 1 */
    vm_init(&ctx); bc_init(&bc);
    emit_const_i32(&bc, 1, 0x10000);
    emit_const_i32(&bc, 2, 0x10000);
    emit_mulh_i32(&bc, 0, 1, 2);
    emit_return(&bc, 0);
    run8(&ctx, &bc);
    check_i32("MULH_I32(0x10000,0x10000)=1", ctx.result.i32, 1);
    vm_destroy(&ctx);

    /* MULH_U32: 0xFFFFFFFF * 0xFFFFFFFF high = 0xFFFFFFFE */
    vm_init(&ctx); bc_init(&bc);
    emit_const_i32(&bc, 1, (int32_t)0xFFFFFFFF);
    emit_const_i32(&bc, 2, (int32_t)0xFFFFFFFF);
    emit_mulh_u32(&bc, 0, 1, 2);
    emit_return(&bc, 0);
    run8(&ctx, &bc);
    check_u32("MULH_U32(0xFFFF...,0xFFFF...)=0xFFFFFFFE", ctx.result.u32, 0xFFFFFFFEu);
    vm_destroy(&ctx);

    /* MULH_I64: 2^32 * 2^32 high = 1 */
    vm_init(&ctx); bc_init(&bc);
    emit_const_i64(&bc, 1, (int64_t)UINT64_C(0x100000000));
    emit_const_i64(&bc, 2, (int64_t)UINT64_C(0x100000000));
    emit_mulh_i64(&bc, 0, 1, 2);
    emit_return(&bc, 0);
    run8(&ctx, &bc);
    check_i64("MULH_I64(2^32,2^32)=1", ctx.result.i64, 1LL);
    vm_destroy(&ctx);

    /* MULH_U64: same */
    vm_init(&ctx); bc_init(&bc);
    emit_const_i64(&bc, 1, (int64_t)UINT64_C(0x100000000));
    emit_const_i64(&bc, 2, (int64_t)UINT64_C(0x100000000));
    emit_mulh_u64(&bc, 0, 1, 2);
    emit_return(&bc, 0);
    run8(&ctx, &bc);
    check_u64("MULH_U64(2^32,2^32)=1", ctx.result.u64, 1ULL);
    vm_destroy(&ctx);
}

/* =========================================================================
 * 8. BOOL
 * ====================================================================== */

static void test_bool(void)
{
    printf("\n--- BOOL ---\n");
    VMContext ctx; Bytecode bc;

#define BOOL32(lbl, v, exp) do {              \
    vm_init(&ctx); bc_init(&bc);    \
    emit_const_i32(&bc, 1, (v));             \
    emit_bool_i32(&bc, 0, 1);               \
    emit_return(&bc, 0);                      \
    run8(&ctx, &bc);                          \
    check_i32(lbl, ctx.result.i32, (exp));   \
    vm_destroy(&ctx);                         \
} while(0)

    BOOL32("BOOL_I32(0)=0",   0,   0);
    BOOL32("BOOL_I32(1)=1",   1,   1);
    BOOL32("BOOL_I32(42)=1",  42,  1);
    BOOL32("BOOL_I32(-1)=1", -1,   1);
#undef BOOL32

    /* BOOL_I64 */
    vm_init(&ctx); bc_init(&bc);
    emit_const_i64(&bc, 1, 0LL);
    emit_bool_i64(&bc, 0, 1);
    emit_return(&bc, 0);
    run8(&ctx, &bc);
    check_i32("BOOL_I64(0)=0", ctx.result.i32, 0);
    vm_destroy(&ctx);

    vm_init(&ctx); bc_init(&bc);
    emit_const_i64(&bc, 1, 9999999999LL);
    emit_bool_i64(&bc, 0, 1);
    emit_return(&bc, 0);
    run8(&ctx, &bc);
    check_i32("BOOL_I64(9999999999)=1", ctx.result.i32, 1);
    vm_destroy(&ctx);
}

/* =========================================================================
 * 9. Float unary intrinsics
 * ====================================================================== */

static void test_float_unary(void)
{
    printf("\n--- Float unary intrinsics ---\n");
    VMContext ctx; Bytecode bc;

#define F32U(lbl, emitter, v, exp) do {       \
    vm_init(&ctx); bc_init(&bc);    \
    emit_const_f32(&bc, 1, (float)(v));      \
    emitter(&bc, 0, 1);                        \
    emit_return(&bc, 0);                       \
    run8(&ctx, &bc);                           \
    check_f32(lbl, ctx.result.f32, (float)(exp), 1e-5f); \
    vm_destroy(&ctx);                          \
} while(0)

#define F64U(lbl, emitter, v, exp) do {       \
    vm_init(&ctx); bc_init(&bc);    \
    emit_const_f64(&bc, 1, (double)(v));     \
    emitter(&bc, 0, 1);                        \
    emit_return(&bc, 0);                       \
    run8(&ctx, &bc);                           \
    check_f64(lbl, ctx.result.f64, (double)(exp), 1e-9); \
    vm_destroy(&ctx);                          \
} while(0)

    F32U("ABS_F32(-3.5)=3.5",    emit_abs_f32,  -3.5f, 3.5f);
    F64U("ABS_F64(-2.0)=2.0",    emit_abs_f64,  -2.0,  2.0);
    F32U("SQRT_F32(9.0)=3.0",    emit_sqrt_f32,  9.0f, 3.0f);
    F64U("SQRT_F64(4.0)=2.0",    emit_sqrt_f64,  4.0,  2.0);
    F32U("FLOOR_F32(2.7)=2.0",   emit_floor_f32, 2.7f, 2.0f);
    F32U("FLOOR_F32(-2.3)=-3.0", emit_floor_f32,-2.3f,-3.0f);
    F64U("FLOOR_F64(2.9)=2.0",   emit_floor_f64, 2.9,  2.0);
    F32U("CEIL_F32(2.1)=3.0",    emit_ceil_f32,  2.1f, 3.0f);
    F32U("CEIL_F32(-2.9)=-2.0",  emit_ceil_f32, -2.9f,-2.0f);
    F64U("CEIL_F64(-1.1)=-1.0",  emit_ceil_f64, -1.1, -1.0);
    F32U("TRUNC_F32(1.9)=1.0",   emit_trunc_f32, 1.9f, 1.0f);
    F32U("TRUNC_F32(-1.9)=-1.0", emit_trunc_f32,-1.9f,-1.0f);
    F64U("TRUNC_F64(3.7)=3.0",   emit_trunc_f64, 3.7,  3.0);
    F32U("ROUND_F32(0.5)=1.0",   emit_round_f32, 0.5f, 1.0f);
    F32U("ROUND_F32(2.4)=2.0",   emit_round_f32, 2.4f, 2.0f);
    F64U("ROUND_F64(0.5)=1.0",   emit_round_f64, 0.5,  1.0);
#undef F32U
#undef F64U
}

/* =========================================================================
 * 10. Float binary intrinsics
 * ====================================================================== */

static void test_float_binary(void)
{
    printf("\n--- Float binary intrinsics ---\n");
    VMContext ctx; Bytecode bc;

#define F32B(lbl, emitter, a, b, exp) do {   \
    vm_init(&ctx); bc_init(&bc);   \
    emit_const_f32(&bc, 1, (float)(a));     \
    emit_const_f32(&bc, 2, (float)(b));     \
    emitter(&bc, 0, 1, 2);                   \
    emit_return(&bc, 0);                      \
    run8(&ctx, &bc);                          \
    check_f32(lbl, ctx.result.f32, (float)(exp), 1e-5f); \
    vm_destroy(&ctx);                         \
} while(0)

#define F64B(lbl, emitter, a, b, exp) do {   \
    vm_init(&ctx); bc_init(&bc);   \
    emit_const_f64(&bc, 1, (double)(a));    \
    emit_const_f64(&bc, 2, (double)(b));    \
    emitter(&bc, 0, 1, 2);                   \
    emit_return(&bc, 0);                      \
    run8(&ctx, &bc);                          \
    check_f64(lbl, ctx.result.f64, (double)(exp), 1e-9); \
    vm_destroy(&ctx);                         \
} while(0)

    F32B("MIN_F32(2.0,3.0)=2.0",        emit_min_f32, 2.0f, 3.0f,  2.0f);
    F32B("MAX_F32(2.0,3.0)=3.0",        emit_max_f32, 2.0f, 3.0f,  3.0f);
    F64B("MIN_F64(-1.0,1.0)=-1.0",      emit_min_f64, -1.0, 1.0,  -1.0);
    F64B("MAX_F64(-1.0,1.0)=1.0",       emit_max_f64, -1.0, 1.0,   1.0);
    F32B("COPYSIGN_F32(-3,1.0)=3.0",    emit_copysign_f32, -3.0f, 1.0f,  3.0f);
    F32B("COPYSIGN_F32(3,-1.0)=-3.0",   emit_copysign_f32,  3.0f,-1.0f, -3.0f);
    F64B("COPYSIGN_F64(5.0,-1.0)=-5.0", emit_copysign_f64,  5.0, -1.0,  -5.0);
#undef F32B
#undef F64B
}

/* =========================================================================
 * 11. Load / Store with immediate offset
 * ====================================================================== */

static void test_load_store_offset(void)
{
    printf("\n--- Load/Store with immediate offset ---\n");
    struct { int32_t x; int32_t y; } pt = { 10, 20 };
    VMContext ctx; Bytecode bc; VMRegister regs[8];

    /* LOAD32S_OFF: read x and y from struct */
    vm_init(&ctx); bc_init(&bc);
    memset(regs, 0, sizeof(regs));
    regs[7].ptr = &pt;
    emit_load32s_off(&bc, 0, 7, 0);   /* r0 = pt.x */
    emit_load32s_off(&bc, 1, 7, 4);   /* r1 = pt.y */
    emit_add_i32(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run_regs(&ctx, &bc, regs, 8);
    check_i32("LOAD32S_OFF: pt.x+pt.y=30", ctx.result.i32, 30);
    vm_destroy(&ctx);

    /* STORE32_OFF: update pt.y */
    vm_init(&ctx); bc_init(&bc);
    memset(regs, 0, sizeof(regs));
    pt.y = 20;
    regs[7].ptr = &pt;
    emit_const_i32(&bc, 0, 99);
    emit_store32_off(&bc, 7, 0, 4);   /* pt.y = 99 */
    emit_return_void(&bc);
    bc_run_regs(&ctx, &bc, regs, 8);
    check_i32("STORE32_OFF: pt.y=99", (int32_t)pt.y, 99);
    vm_destroy(&ctx);

    /* LOAD8S_OFF: sign-extended byte */
    {
        uint8_t arr[4] = { 0x10, 0x80, 0x7F, 0x01 };
        vm_init(&ctx); bc_init(&bc);
        memset(regs, 0, sizeof(regs));
        regs[7].ptr = arr;
        emit_load8s_off(&bc, 0, 7, 1); /* arr[1]=0x80 -> -128 signed */
        emit_return(&bc, 0);
        bc_run_regs(&ctx, &bc, regs, 8);
        check_i32("LOAD8S_OFF(0x80)=-128", ctx.result.i32, -128);
        vm_destroy(&ctx);
    }
}

/* =========================================================================
 * 12. LEA_REG
 * ====================================================================== */

static void test_lea_reg(void)
{
    printf("\n--- LEA_REG ---\n");
    int32_t arr[5] = { 10, 20, 30, 40, 50 };
    VMContext ctx; Bytecode bc; VMRegister regs[8];

    /* Access arr[2] via variable index */
    vm_init(&ctx); bc_init(&bc);
    memset(regs, 0, sizeof(regs));
    regs[5].ptr = arr;
    regs[6].i64 = 2 * 4; /* byte offset */
    emit_lea_reg(&bc, 0, 5, 6);   /* r0 = &arr[2] */
    emit_load32s(&bc, 1, 0);       /* r1 = arr[2] = 30 */
    emit_return(&bc, 1);
    bc_run_regs(&ctx, &bc, regs, 8);
    check_i32("LEA_REG: arr[2]=30", ctx.result.i32, 30);
    vm_destroy(&ctx);

    /* Store via LEA_REG */
    vm_init(&ctx); bc_init(&bc);
    memset(regs, 0, sizeof(regs));
    arr[4] = 50;
    regs[5].ptr = arr;
    regs[6].i64 = 4 * 4;
    emit_const_i32(&bc, 0, 777);
    emit_lea_reg(&bc, 1, 5, 6);
    emit_store32(&bc, 1, 0);
    emit_return_void(&bc);
    bc_run_regs(&ctx, &bc, regs, 8);
    check_i32("LEA_REG store: arr[4]=777", arr[4], 777);
    vm_destroy(&ctx);
}

/* =========================================================================
 * 13. MEMCPY / MEMSET
 * ====================================================================== */

static void test_memcpy_memset(void)
{
    printf("\n--- MEMCPY / MEMSET ---\n");
    uint8_t src[16], dst[16];
    VMContext ctx; Bytecode bc; VMRegister regs[8];

    /* MEMSET */
    memset(dst, 0, sizeof(dst));
    vm_init(&ctx); bc_init(&bc);
    memset(regs, 0, sizeof(regs));
    regs[0].ptr = dst;
    regs[1].i32 = 0xAB;
    regs[2].i64 = 8;
    emit_memset(&bc, 0, 1, 2);
    emit_return_void(&bc);
    bc_run_regs(&ctx, &bc, regs, 8);
    check_i32("MEMSET dst[0]=0xAB",  (int32_t)dst[0], 0xAB);
    check_i32("MEMSET dst[7]=0xAB",  (int32_t)dst[7], 0xAB);
    check_i32("MEMSET dst[8]=0x00",  (int32_t)dst[8], 0x00);
    vm_destroy(&ctx);

    /* MEMCPY */
    for (int i = 0; i < 8; i++) src[i] = (uint8_t)(i * 11);
    memset(dst, 0xFF, sizeof(dst));
    vm_init(&ctx); bc_init(&bc);
    memset(regs, 0, sizeof(regs));
    regs[0].ptr = dst;
    regs[1].ptr = src;
    regs[2].i64 = 8;
    emit_memcpy(&bc, 0, 1, 2);
    emit_return_void(&bc);
    bc_run_regs(&ctx, &bc, regs, 8);
    check_i32("MEMCPY dst[0]=0",   (int32_t)dst[0], 0);
    check_i32("MEMCPY dst[3]=33",  (int32_t)dst[3], 33);
    check_i32("MEMCPY dst[8]=0xFF",(int32_t)dst[8], 0xFF);
    vm_destroy(&ctx);
}

/* =========================================================================
 * 14. SWITCH
 * ====================================================================== */

static void run_switch_test(const char* label, int32_t key, int32_t expected)
{
    /* SWITCH with 3 cases: 0->10, 1->20, 2->30, default->99 */
    VMContext ctx; Bytecode bc;
    vm_init(&ctx);
    bc_init(&bc);

    emit_const_i32(&bc, 0, key);

    /* Header: 2(op)+1(reg)+4(count)+4*4(slots)=23 bytes */
    uint32_t sw_slots     = emit_switch_header(&bc, 0, 3);
    uint32_t switch_end   = bc.size;      /* pc right after entire header */

    uint32_t pc_case0 = bc.size;
    emit_const_i32(&bc, 1, 10);
    uint32_t j0 = emit_goto_16_fwd(&bc);

    uint32_t pc_case1 = bc.size;
    emit_const_i32(&bc, 1, 20);
    uint32_t j1 = emit_goto_16_fwd(&bc);

    uint32_t pc_case2 = bc.size;
    emit_const_i32(&bc, 1, 30);
    uint32_t j2 = emit_goto_16_fwd(&bc);

    uint32_t pc_def = bc.size;
    emit_const_i32(&bc, 1, 99);

    /* Patch jumps to end */
    bc_patch_here(&bc, j0);
    bc_patch_here(&bc, j1);
    bc_patch_here(&bc, j2);

    emit_return(&bc, 1);

    /* Patch SWITCH table */
    bc_patch_switch(&bc, sw_slots, 0, pc_def,   switch_end); /* default */
    bc_patch_switch(&bc, sw_slots, 1, pc_case0, switch_end); /* case 0  */
    bc_patch_switch(&bc, sw_slots, 2, pc_case1, switch_end); /* case 1  */
    bc_patch_switch(&bc, sw_slots, 3, pc_case2, switch_end); /* case 2  */

    bc_run(&ctx, &bc, 4);
    check_i32(label, ctx.result.i32, expected);
    vm_destroy(&ctx);
}

static void test_switch(void)
{
    printf("\n--- SWITCH ---\n");
    run_switch_test("SWITCH key=0->10",    0,   10);
    run_switch_test("SWITCH key=1->20",    1,   20);
    run_switch_test("SWITCH key=2->30",    2,   30);
    run_switch_test("SWITCH key=3->99(def)", 3, 99);
    run_switch_test("SWITCH key=-1->99(def)",-1, 99);
}

/* =========================================================================
 * main
 * ====================================================================== */

int main(void)
{
    printf("=== CVM Extended Opcodes Test Suite ===\n");

    test_cmp_unsigned();
    test_unsigned_branches();
    test_select();
    test_clz_ctz_popcnt();
    test_rotl_rotr();
    test_abs_min_max_int();
    test_mulh();
    test_bool();
    test_float_unary();
    test_float_binary();
    test_load_store_offset();
    test_lea_reg();
    test_memcpy_memset();
    test_switch();

    print_summary();
    return g_fail > 0 ? 1 : 0;
}
