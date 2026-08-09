/*
 * Copyright (C) 2026 CVM Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * unsigned_ops.c — tests for DIV_U32, REM_U32, DIV_U64, REM_U64, CMP_F32_GT
 */

#include "../vm.h"
#include "../vm_builder.h"  /* also provides check_i32, check_u32, check_u64,
                               check_err, g_pass, g_fail, bc_run, bc_free */

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>

/* =========================================================================
 * DIV_U32  —  unsigned 32-bit division
 * ====================================================================== */
static void test_div_u32(VMContext* ctx)
{
    printf("\n--- DIV_U32 ---\n");

    /* 0xFFFFFFFF / 2 = 2147483647  (would be -1/2=0 under signed) */
    {
        Bytecode bc; bc_init(&bc);
        /* emit_const_i64 stores all 64 bits; .u32 reads low 32 = 0xFFFFFFFF */
        emit_const_i64(&bc, 0, (int64_t)(uint64_t)0xFFFFFFFFu);
        emit_const_i32(&bc, 1, 2);
        emit_div_u32(&bc, 2, 0, 1);
        emit_return(&bc, 2);
        bc_run(ctx, &bc, 4);
        check_u32("0xFFFFFFFF / 2 = 2147483647", ctx->result.u32, 2147483647u);

    }
    /* 10 / 3 = 3 */
    {
        Bytecode bc; bc_init(&bc);
        emit_const_i32(&bc, 0, 10);
        emit_const_i32(&bc, 1, 3);
        emit_div_u32(&bc, 2, 0, 1);
        emit_return(&bc, 2);
        bc_run(ctx, &bc, 4);
        check_u32("10u / 3u = 3", ctx->result.u32, 3u);

    }
    /* 0 / 5 = 0 */
    {
        Bytecode bc; bc_init(&bc);
        emit_const_i32(&bc, 0, 0);
        emit_const_i32(&bc, 1, 5);
        emit_div_u32(&bc, 2, 0, 1);
        emit_return(&bc, 2);
        bc_run(ctx, &bc, 4);
        check_u32("0u / 5u = 0", ctx->result.u32, 0u);

    }
    /* div-by-zero traps */
    {
        Bytecode bc; bc_init(&bc);
        emit_const_i32(&bc, 0, 42);
        emit_const_i32(&bc, 1, 0);
        emit_div_u32(&bc, 2, 0, 1);
        emit_return(&bc, 2);
        VMError err = bc_run(ctx, &bc, 4);
        check_err("div-by-zero => VM_ERR_DIV_ZERO", err, VM_ERR_DIV_ZERO);

    }
}

/* =========================================================================
 * REM_U32  —  unsigned 32-bit remainder
 * ====================================================================== */
static void test_rem_u32(VMContext* ctx)
{
    printf("\n--- REM_U32 ---\n");

    /* 0xFFFFFFFF % 3 — signed: (-1)%3=-1; unsigned: 4294967295%3=0 */
    {
        Bytecode bc; bc_init(&bc);
        emit_const_i64(&bc, 0, (int64_t)(uint64_t)0xFFFFFFFFu);
        emit_const_i32(&bc, 1, 3);
        emit_rem_u32(&bc, 2, 0, 1);
        emit_return(&bc, 2);
        bc_run(ctx, &bc, 4);
        /* 4294967295 = 3 * 1431655765 + 0 */
        check_u32("0xFFFFFFFF % 3 = 0", ctx->result.u32, 0u);

    }
    /* 17 % 5 = 2 */
    {
        Bytecode bc; bc_init(&bc);
        emit_const_i32(&bc, 0, 17);
        emit_const_i32(&bc, 1, 5);
        emit_rem_u32(&bc, 2, 0, 1);
        emit_return(&bc, 2);
        bc_run(ctx, &bc, 4);
        check_u32("17u % 5u = 2", ctx->result.u32, 2u);

    }
    /* rem-by-zero traps */
    {
        Bytecode bc; bc_init(&bc);
        emit_const_i32(&bc, 0, 99);
        emit_const_i32(&bc, 1, 0);
        emit_rem_u32(&bc, 2, 0, 1);
        emit_return(&bc, 2);
        VMError err = bc_run(ctx, &bc, 4);
        check_err("rem-by-zero => VM_ERR_DIV_ZERO", err, VM_ERR_DIV_ZERO);

    }
}

/* =========================================================================
 * DIV_U64  —  unsigned 64-bit division
 * ====================================================================== */
static void test_div_u64(VMContext* ctx)
{
    printf("\n--- DIV_U64 ---\n");

    /* UINT64_MAX / 2 (signed -1/2=0; unsigned 9223372036854775807) */
    {
        Bytecode bc; bc_init(&bc);
        emit_const_i64(&bc, 0, (int64_t)UINT64_MAX); /* all bits set */
        emit_const_i32(&bc, 1, 2);
        emit_div_u64(&bc, 2, 0, 1);
        emit_return(&bc, 2);
        bc_run(ctx, &bc, 4);
        check_u64("UINT64_MAX / 2", ctx->result.u64, UINT64_MAX / 2u);

    }
    /* 1e18 / 1e9 = 1e9 */
    {
        Bytecode bc; bc_init(&bc);
        emit_const_i64(&bc, 0, (int64_t)1000000000000000000ULL);
        emit_const_i64(&bc, 1, (int64_t)1000000000ULL);
        emit_div_u64(&bc, 2, 0, 1);
        emit_return(&bc, 2);
        bc_run(ctx, &bc, 4);
        check_u64("1e18 / 1e9 = 1e9", ctx->result.u64, 1000000000ULL);

    }
    /* div-by-zero traps */
    {
        Bytecode bc; bc_init(&bc);
        emit_const_i64(&bc, 0, 42);
        emit_const_i64(&bc, 1, 0);
        emit_div_u64(&bc, 2, 0, 1);
        emit_return(&bc, 2);
        VMError err = bc_run(ctx, &bc, 4);
        check_err("u64 div-by-zero => VM_ERR_DIV_ZERO", err, VM_ERR_DIV_ZERO);

    }
}

/* =========================================================================
 * REM_U64  —  unsigned 64-bit remainder
 * ====================================================================== */
static void test_rem_u64(VMContext* ctx)
{
    printf("\n--- REM_U64 ---\n");

    /* UINT64_MAX % 7 */
    {
        uint64_t expected = UINT64_MAX % 7u;
        Bytecode bc; bc_init(&bc);
        emit_const_i64(&bc, 0, (int64_t)UINT64_MAX);
        emit_const_i32(&bc, 1, 7);
        emit_rem_u64(&bc, 2, 0, 1);
        emit_return(&bc, 2);
        bc_run(ctx, &bc, 4);
        check_u64("UINT64_MAX % 7", ctx->result.u64, expected);

    }
    /* 1000000007 % 1000000 = 7 */
    {
        Bytecode bc; bc_init(&bc);
        emit_const_i64(&bc, 0, 1000000007LL);
        emit_const_i64(&bc, 1, 1000000LL);
        emit_rem_u64(&bc, 2, 0, 1);
        emit_return(&bc, 2);
        bc_run(ctx, &bc, 4);
        check_u64("1000000007u64 % 1000000u64 = 7", ctx->result.u64, 7ULL);

    }
    /* rem-by-zero traps */
    {
        Bytecode bc; bc_init(&bc);
        emit_const_i64(&bc, 0, 99);
        emit_const_i64(&bc, 1, 0);
        emit_rem_u64(&bc, 2, 0, 1);
        emit_return(&bc, 2);
        VMError err = bc_run(ctx, &bc, 4);
        check_err("u64 rem-by-zero => VM_ERR_DIV_ZERO", err, VM_ERR_DIV_ZERO);

    }
}

/* =========================================================================
 * CMP_F32_GT  —  ordered-greater / NaN => +2
 * ====================================================================== */
static void test_cmp_f32_gt(VMContext* ctx)
{
    printf("\n--- CMP_F32_GT ---\n");

    /* 5.0f > 3.0f => +1 */
    {
        Bytecode bc; bc_init(&bc);
        emit_const_f32(&bc, 0, 5.0f);
        emit_const_f32(&bc, 1, 3.0f);
        emit_cmp_f32_gt(&bc, 2, 0, 1);
        emit_return(&bc, 2);
        bc_run(ctx, &bc, 4);
        check_i32("5.0f > 3.0f => +1", ctx->result.i32, 1);

    }
    /* 2.0f < 7.0f => -1 */
    {
        Bytecode bc; bc_init(&bc);
        emit_const_f32(&bc, 0, 2.0f);
        emit_const_f32(&bc, 1, 7.0f);
        emit_cmp_f32_gt(&bc, 2, 0, 1);
        emit_return(&bc, 2);
        bc_run(ctx, &bc, 4);
        check_i32("2.0f < 7.0f => -1", ctx->result.i32, -1);

    }
    /* 4.0f == 4.0f => 0 */
    {
        Bytecode bc; bc_init(&bc);
        emit_const_f32(&bc, 0, 4.0f);
        emit_const_f32(&bc, 1, 4.0f);
        emit_cmp_f32_gt(&bc, 2, 0, 1);
        emit_return(&bc, 2);
        bc_run(ctx, &bc, 4);
        check_i32("4.0f == 4.0f => 0", ctx->result.i32, 0);

    }
    /* NaN vs 1.0f => +2  (unlike CMP_F32 which returns -1) */
    {
        float nan_val = 0.0f / 0.0f;
        Bytecode bc; bc_init(&bc);
        emit_const_f32(&bc, 0, nan_val);
        emit_const_f32(&bc, 1, 1.0f);
        emit_cmp_f32_gt(&bc, 2, 0, 1);
        emit_return(&bc, 2);
        bc_run(ctx, &bc, 4);
        check_i32("NaN vs 1.0f => +2 (unordered-greater)", ctx->result.i32, 2);

    }
    /* 1.0f vs NaN => +2 */
    {
        float nan_val = 0.0f / 0.0f;
        Bytecode bc; bc_init(&bc);
        emit_const_f32(&bc, 0, 1.0f);
        emit_const_f32(&bc, 1, nan_val);
        emit_cmp_f32_gt(&bc, 2, 0, 1);
        emit_return(&bc, 2);
        bc_run(ctx, &bc, 4);
        check_i32("1.0f vs NaN => +2 (unordered-greater)", ctx->result.i32, 2);

    }
    /* Contrast: CMP_F32 NaN => -1 (unordered-less) */
    {
        float nan_val = 0.0f / 0.0f;
        Bytecode bc; bc_init(&bc);
        emit_const_f32(&bc, 0, nan_val);
        emit_const_f32(&bc, 1, 1.0f);
        emit_cmp_f32(&bc, 2, 0, 1);
        emit_return(&bc, 2);
        bc_run(ctx, &bc, 4);
        check_i32("CMP_F32: NaN vs 1.0f => -1 (unordered-less)", ctx->result.i32, -1);

    }
}

/* =========================================================================
 * main
 * ====================================================================== */
int main(void)
{
    printf("=== unsigned_ops: DIV_U32 REM_U32 DIV_U64 REM_U64 CMP_F32_GT ===\n");

    VMContext ctx;
    vm_init(&ctx);

    test_div_u32(&ctx);
    test_rem_u32(&ctx);
    test_div_u64(&ctx);
    test_rem_u64(&ctx);
    test_cmp_f32_gt(&ctx);

    print_summary();
    return g_fail > 0 ? 1 : 0;
}
