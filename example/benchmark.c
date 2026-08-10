/*
 * Copyright (C) 2026 CVM Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * benchmark.c — Performance benchmark measuring VM opcode execution throughput
 */

#include "../vm.h"
#include "../vm_builder.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

static void test_collatz_benchmark(void)
{
    printf("\n--- test_collatz_benchmark (1,000,000 iterations) ---\n");

    VMContext ctx;
    vm_init(&ctx);

    Bytecode bc;
    bc_init(&bc);

    /*
     * Compute Collatz sequence length for n = 27 (takes 111 steps)
     * Repeat outer loop 10,000 times -> ~1.1 million VM instructions executed.
     *
     * Outer loop: r0 = outer_count = 10000, r1 = outer_i = 0
     * Inner loop: r2 = n = 27, r3 = steps = 0
     *
     * Inner loop:
     *   if n == 1 goto InnerEnd
     *   steps++
     *   if (n & 1 == 0) n = n / 2  (via USHR_I32 r2, r2, 1)
     *   else n = 3*n + 1
     *   goto InnerLoop
     */

    emit_const_i32(&bc, 0, 10000);  /* outer_count = 10000 */
    emit_const_i32(&bc, 1, 0);      /* outer_i = 0 */
    emit_const_i32(&bc, 7, 1);      /* const_1 = 1 */
    emit_const_i32(&bc, 8, 3);      /* const_3 = 3 */

    uint32_t outer_start = bc.size;

    emit_const_i32(&bc, 2, 27);     /* n = 27 */
    emit_const_i32(&bc, 3, 0);      /* steps = 0 */

    uint32_t inner_start = bc.size;

    /* if n == 1 goto inner_end */
    uint32_t p_done = emit_if_eq(&bc, 2, 7);

    /* steps++ */
    emit_add_i32(&bc, 3, 3, 7);

    /* r4 = n & 1 */
    emit_and_i32(&bc, 4, 2, 7);
    uint32_t p_odd = emit_if_nez(&bc, 4);

    /* even path: n = n >> 1 */
    emit_ushr_i32(&bc, 2, 2, 7);
    uint32_t p_even_done = emit_goto_16_fwd(&bc);

    /* odd path: n = 3*n + 1 */
    bc_patch_here(&bc, p_odd);
    emit_mul_i32(&bc, 2, 2, 8);
    emit_add_i32(&bc, 2, 2, 7);

    bc_patch_here(&bc, p_even_done);
    bc_patch_back(&bc, emit_goto_16_fwd(&bc), inner_start);

    bc_patch_here(&bc, p_done);

    /* outer_i++ */
    emit_add_i32(&bc, 1, 1, 7);
    uint32_t p_outer = emit_if_lt(&bc, 1, 0);
    bc_patch_back(&bc, p_outer, outer_start);

    emit_return(&bc, 3);

    VMRegister regs[10];
    memset(regs, 0, sizeof(regs));

    clock_t t0 = clock();
    VMError err = bc_run_regs(&ctx, &bc, regs, 10);
    clock_t t1 = clock();

    double elapsed_sec = (double)(t1 - t0) / CLOCKS_PER_SEC;

    check_err("benchmark err == VM_OK", err, VM_OK);
    check_i32("collatz steps for n=27 == 111", ctx.result.i32, 111);
    printf("  Execution time: %.4f seconds (10,000 Collatz runs = ~1,110,000 bytecode instrs)\n", elapsed_sec);
}

int main(void)
{
    printf("=== Performance Benchmark ===\n");
    test_collatz_benchmark();
    print_summary();
    return g_fail > 0 ? 1 : 0;
}
