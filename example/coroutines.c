/*
 * Copyright (C) 2026 CVM Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * coroutines.c — Generator/Coroutine pattern using VM single-stepping and pausing
 */

#include "../vm.h"
#include "../vm_builder.h"

#include <stdio.h>
#include <string.h>

/* =========================================================================
 * Generator Example: Fibonnaci Generator in Bytecode
 * Yields terms 1, 1, 2, 3, 5, 8, 13, 21, 34, 55...
 * ====================================================================== */
static void test_fibonacci_generator(void)
{
    printf("\n--- test_fibonacci_generator ---\n");

    VMContext ctx;
    vm_init(&ctx);

    Bytecode bc;
    bc_init(&bc);

    /*
     * r0 = prev = 0
     * r1 = curr = 1
     * r2 = temp
     * r3 = count = 10
     * r4 = i = 0
     * r5 = step = 1
     *
     * Loop:
     *   temp = curr
     *   curr = prev + curr
     *   prev = temp
     *   i++
     *   if i < count goto Loop
     */
    emit_const_i32(&bc, 0, 0);   /* r0 = 0 */
    emit_const_i32(&bc, 1, 1);   /* r1 = 1 */
    emit_const_i32(&bc, 3, 10);  /* r3 = 10 */
    emit_const_i32(&bc, 4, 0);   /* r4 = 0 */
    emit_const_i32(&bc, 5, 1);   /* r5 = 1 */

    uint32_t loop_start = bc.size;
    emit_move(&bc, 2, 1);        /* temp = curr */
    emit_add_i32(&bc, 1, 0, 1);  /* curr = prev + curr */
    emit_move(&bc, 0, 2);        /* prev = temp */
    emit_add_i32(&bc, 4, 4, 5);  /* i++ */
    uint32_t patch = emit_if_lt(&bc, 4, 3);
    bc_patch_back(&bc, patch, loop_start);
    emit_return(&bc, 0);

    VMRegister regs[6];
    memset(regs, 0, sizeof(regs));

    int expected_fibs[] = {1, 1, 2, 3, 5, 8, 13, 21, 34, 55};
    int yielded_count = 0;

    /* Run generator by stepping through loop iterations */
    ctx.flags |= VM_FLAG_SINGLE_STEP;
    uint32_t last_i = 0;

    while (!(ctx.flags & VM_FLAG_HALTED)) {
        VMError err = vm_execute(&ctx, regs, 6, ctx.pc, bc.data, bc.size);
        check_err("generator step err == VM_OK", err, VM_OK);

        /* Detect when loop counter increments (a new fib value was computed) */
        if ((uint32_t)regs[4].i32 > last_i && yielded_count < 10) {
            check_i32("yielded fibonacci term", regs[0].i32, expected_fibs[yielded_count]);
            last_i = (uint32_t)regs[4].i32;
            yielded_count++;
        }

        /* Continue single stepping */
        ctx.flags |= VM_FLAG_SINGLE_STEP;
    }

    check_i32("total yielded count", yielded_count, 10);
    check_i32("final return value", ctx.result.i32, 55);
}

int main(void)
{
    printf("=== Coroutines & Generators ===\n");
    test_fibonacci_generator();
    print_summary();
    return g_fail > 0 ? 1 : 0;
}
