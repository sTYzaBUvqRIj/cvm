/*
 * Copyright (C) 2026 CVM Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * subroutines.c — tests for OP_CALL_BC, OP_RET, stack frames, and recursion
 */

#include "../vm.h"
#include "../vm_builder.h"

#include <stdio.h>
#include <string.h>

/*
 * Test 1: Simple subroutine call (add_five)
 *
 * Main (pc = 0):
 *   0: GOTO_16 main_entry
 *
 * add_five (pc = 4):
 *   4: CONST_I32 r1, 5
 *  11: ADD_I32   r2, r0, r1
 *  16: RET       r2
 *
 * main_entry (pc = 18):
 *  18: CONST_I32 r0, 37
 *  25: CALL_BC   r3, 4, 1, r0   (calls add_five(37) -> r3 = 42)
 *  33: RETURN    r3
 */
static void test_subroutine_call(void)
{
    printf("\n--- test_subroutine_call ---\n");

    VMContext ctx;
    vm_init(&ctx);

    Bytecode bc;
    bc_init(&bc);

    uint32_t p_main = emit_goto_16_fwd(&bc);

    /* Function add_five at pc = 4 */
    uint32_t add_five_pc = bc.size;
    emit_const_i32(&bc, 1, 5);
    emit_add_i32  (&bc, 2, 0, 1);
    emit_ret      (&bc, 2);

    /* Main entry */
    bc_patch_here(&bc, p_main);
    emit_const_i32(&bc, 0, 37);
    emit_call_bc_1(&bc, 3, add_five_pc, 0);
    emit_return   (&bc, 3);

    VMRegister regs[4];
    memset(regs, 0, sizeof(regs));

    VMError err = bc_run_regs(&ctx, &bc, regs, 4);
    check_err("subroutine call err == VM_OK", err, VM_OK);
    check_i32("add_five(37) == 42", ctx.result.i32, 42);
}

/*
 * Test 2: Recursive Factorial in Bytecode
 *
 * fact(n):
 *   if n <= 1 return 1
 *   else return n * fact(n - 1)
 *
 * Bytecode layout:
 *   0: GOTO_16 main_entry
 *
 * fact (pc = 4):  // r0 = n
 *   4: CONST_I32 r1, 1
 *  11: IF_LE     r0, r1, base_case  (if n <= 1 goto base_case)
 *  17: SUB_I32   r2, r0, r1         (r2 = n - 1)
 *  22: CALL_BC   r3, 4, 1, r2       (r3 = fact(n - 1))
 *  30: MUL_I32   r4, r0, r3         (r4 = n * fact(n - 1))
 *  35: RET       r4
 * base_case:
 *  37: RET       r1                 (return 1)
 *
 * main_entry (pc = 39):
 *  39: CONST_I32 r0, 5
 *  46: CALL_BC   r5, 4, 1, r0       (r5 = fact(5))
 *  54: RETURN    r5
 */
static void test_recursive_factorial(void)
{
    printf("\n--- test_recursive_factorial ---\n");

    VMContext ctx;
    vm_init(&ctx);

    Bytecode bc;
    bc_init(&bc);

    uint32_t p_main = emit_goto_16_fwd(&bc);

    /* fact(n) at pc = 4 */
    uint32_t fact_pc = bc.size;
    emit_const_i32(&bc, 1, 1);
    uint32_t p_base = emit_if_le(&bc, 0, 1);
    emit_sub_i32(&bc, 2, 0, 1);             /* r2 = n - 1 */
    emit_call_bc_1(&bc, 3, fact_pc, 2);     /* r3 = fact(n-1) */
    emit_mul_i32(&bc, 4, 0, 3);             /* r4 = n * fact(n-1) */
    emit_ret(&bc, 4);

    bc_patch_here(&bc, p_base);
    emit_ret(&bc, 1);                       /* return 1 */

    /* Main entry */
    bc_patch_here(&bc, p_main);
    emit_const_i32(&bc, 0, 5);
    emit_call_bc_1(&bc, 5, fact_pc, 0);     /* r5 = fact(5) */
    emit_return(&bc, 5);

    VMRegister regs[6];
    memset(regs, 0, sizeof(regs));

    VMError err = bc_run_regs(&ctx, &bc, regs, 6);
    check_err("recursive factorial err == VM_OK", err, VM_OK);
    check_i32("fact(5) == 120", ctx.result.i32, 120);
}

/*
 * Test 3: Recursive Fibonacci in Bytecode
 * fib(n):
 *   if n <= 1 return n
 *   else return fib(n-1) + fib(n-2)
 */
static void test_recursive_fibonacci(void)
{
    printf("\n--- test_recursive_fibonacci ---\n");

    VMContext ctx;
    vm_init(&ctx);

    Bytecode bc;
    bc_init(&bc);

    uint32_t p_main = emit_goto_16_fwd(&bc);

    /* fib(n) at pc = 4 */
    uint32_t fib_pc = bc.size;
    emit_const_i32(&bc, 1, 1);
    emit_const_i32(&bc, 2, 2);
    uint32_t p_base = emit_if_le(&bc, 0, 1);

    emit_sub_i32(&bc, 3, 0, 1);             /* r3 = n - 1 */
    emit_call_bc_1(&bc, 4, fib_pc, 3);      /* r4 = fib(n-1) */

    emit_sub_i32(&bc, 5, 0, 2);             /* r5 = n - 2 */
    emit_call_bc_1(&bc, 6, fib_pc, 5);      /* r6 = fib(n-2) */

    emit_add_i32(&bc, 7, 4, 6);             /* r7 = fib(n-1) + fib(n-2) */
    emit_ret(&bc, 7);

    bc_patch_here(&bc, p_base);
    emit_ret(&bc, 0);                       /* return n */

    /* Main entry */
    bc_patch_here(&bc, p_main);
    emit_const_i32(&bc, 0, 10);
    emit_call_bc_1(&bc, 1, fib_pc, 0);      /* r1 = fib(10) */
    emit_return(&bc, 1);

    VMRegister regs[8];
    memset(regs, 0, sizeof(regs));

    VMError err = bc_run_regs(&ctx, &bc, regs, 8);
    check_err("recursive fibonacci err == VM_OK", err, VM_OK);
    check_i32("fib(10) == 55", ctx.result.i32, 55);
}

/*
 * Test 4: Call Stack Overflow (> 128 recursive calls)
 */
static void test_call_stack_overflow(void)
{
    printf("\n--- test_call_stack_overflow ---\n");

    VMContext ctx;
    vm_init(&ctx);

    Bytecode bc;
    bc_init(&bc);

    /* infinite_recurse at pc = 0: CALL_BC r0, 0, 0 */
    emit_call_bc_0(&bc, 0, 0);

    VMRegister regs[4];
    memset(regs, 0, sizeof(regs));

    VMError err = bc_run_regs(&ctx, &bc, regs, 4);
    check_err("infinite recursion => VM_ERR_STACK_OVERFLOW", err, VM_ERR_STACK_OVERFLOW);
}

int main(void)
{
    printf("=== Subroutine & Call Stack Tests ===\n");
    test_subroutine_call();
    test_recursive_factorial();
    test_recursive_fibonacci();
    test_call_stack_overflow();
    print_summary();
    return g_fail > 0 ? 1 : 0;
}
