/*
 * Copyright (C) 2026 CVM Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * single_step.c — tests for VMContext pc, flags, and single-step execution
 */

#include "../vm.h"
#include "../vm_builder.h"

#include <stdio.h>
#include <string.h>

static void test_single_step_execution(void)
{
    printf("\n--- test_single_step_execution ---\n");

    VMContext ctx;
    vm_init(&ctx);

    Bytecode bc;
    bc_init(&bc);

    /* 
     * Program:
     *   0: CONST_I32 r0, 10
     *   7: CONST_I32 r1, 20
     *  14: ADD_I32   r2, r0, r1
     *  19: RETURN    r2
     */
    emit_const_i32(&bc, 0, 10);
    emit_const_i32(&bc, 1, 20);
    emit_add_i32  (&bc, 2, 0, 1);
    emit_return   (&bc, 2);

    VMRegister regs[4];
    memset(regs, 0, sizeof(regs));

    /* Enable single stepping */
    ctx.flags |= VM_FLAG_SINGLE_STEP;

    /* Step 1: CONST_I32 r0, 10 */
    VMError err = vm_execute(&ctx, regs, 4, 0, bc.data, bc.size);
    check_err("step 1 err == VM_OK", err, VM_OK);
    check_i32("step 1 r0", regs[0].i32, 10);
    check_u32("step 1 pc == 7", ctx.pc, 7);
    check_u32("step 1 paused flag", (ctx.flags & VM_FLAG_PAUSED) != 0, 1);

    /* Step 2: CONST_I32 r1, 20 */
    ctx.flags |= VM_FLAG_SINGLE_STEP;
    err = vm_execute(&ctx, regs, 4, ctx.pc, bc.data, bc.size);
    check_err("step 2 err == VM_OK", err, VM_OK);
    check_i32("step 2 r1", regs[1].i32, 20);
    check_u32("step 2 pc == 14", ctx.pc, 14);
    check_u32("step 2 paused flag", (ctx.flags & VM_FLAG_PAUSED) != 0, 1);

    /* Step 3: ADD_I32 r2, r0, r1 */
    ctx.flags |= VM_FLAG_SINGLE_STEP;
    err = vm_execute(&ctx, regs, 4, ctx.pc, bc.data, bc.size);
    check_err("step 3 err == VM_OK", err, VM_OK);
    check_i32("step 3 r2", regs[2].i32, 30);
    check_u32("step 3 pc == 19", ctx.pc, 19);

    /* Step 4: RETURN r2 (halts) */
    ctx.flags |= VM_FLAG_SINGLE_STEP;
    err = vm_execute(&ctx, regs, 4, ctx.pc, bc.data, bc.size);
    check_err("step 4 err == VM_OK", err, VM_OK);
    check_i32("step 4 result", ctx.result.i32, 30);
    check_u32("step 4 halted flag", (ctx.flags & VM_FLAG_HALTED) != 0, 1);
}

int main(void)
{
    printf("=== Single Step & Execution Control Tests ===\n");
    test_single_step_execution();
    print_summary();
    return g_fail > 0 ? 1 : 0;
}
