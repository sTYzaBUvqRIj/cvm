#include "../vm_builder.h"
#include <stdio.h>
#include <stdint.h>

void test_factorial_10() {
    VMContext ctx;
    vm_init(&ctx);
    Bytecode bc;
    bc_init(&bc);
    // 10!
    emit_const_i32(&bc, 0, 10); // n
    emit_const_i32(&bc, 1, 1); // result
    emit_const_i32(&bc, 2, 1); // i
    emit_const_i32(&bc, 3, 1); // step
    
    uint32_t loop_top = bc.size;
    uint32_t p = emit_if_gt(&bc, 2, 0); // if i > n exit
    emit_mul_i32(&bc, 1, 1, 2); // result *= i
    emit_add_i32(&bc, 2, 2, 3); // i++
    emit_goto_back(&bc, loop_top);
    bc_patch_here(&bc, p);
    
    emit_return(&bc, 1);
    VMError err = bc_run(&ctx, &bc, 4);
    check_err("test_factorial_10", err, VM_OK);
    check_i32("10!", ctx.result.i32, 3628800);
}

void test_factorial_15_i64() {
    VMContext ctx;
    vm_init(&ctx);
    Bytecode bc;
    bc_init(&bc);
    // 15!
    emit_const_i64(&bc, 0, 15); // n
    emit_const_i64(&bc, 1, 1); // result
    emit_const_i64(&bc, 2, 1); // i
    emit_const_i64(&bc, 3, 1); // step
    
    uint32_t loop_top = bc.size;
    uint32_t p = emit_if_gt(&bc, 2, 0); // if i > n exit
    emit_mul_i64(&bc, 1, 1, 2); // result *= i
    emit_add_i64(&bc, 2, 2, 3); // i++
    emit_goto_back(&bc, loop_top);
    bc_patch_here(&bc, p);
    
    emit_return(&bc, 1);
    VMError err = bc_run(&ctx, &bc, 4);
    check_err("test_factorial_15_i64", err, VM_OK);
    check_i64("15!", ctx.result.i64, 1307674368000LL);
}

int32_t fact_c(int32_t n) {
    if (n <= 1) return 1;
    return n * fact_c(n-1);
}

VMError native_fact(VMContext* ctx, uint32_t argc, VMRegister* args, VMRegister* out_result) {
    if (argc != 1) return VM_ERR_BAD_ARGC;
    out_result->i32 = fact_c(args[0].i32);
    return VM_OK;
}

void test_factorial_via_native() {
    VMContext ctx;
    vm_init(&ctx);
    vm_register_function(&ctx, 1, native_fact);
    
    Bytecode bc;
    bc_init(&bc);
    emit_const_i32(&bc, 0, 12);
    emit_call1(&bc, 1, 1, 0);
    emit_return(&bc, 1);
    
    VMError err = bc_run(&ctx, &bc, 2);
    check_err("test_factorial_via_native", err, VM_OK);
    check_i32("12! native", ctx.result.i32, 479001600);
}

void test_factorial_0() {
    VMContext ctx;
    vm_init(&ctx);
    Bytecode bc;
    bc_init(&bc);
    // 0!
    emit_const_i32(&bc, 0, 0); // n
    emit_const_i32(&bc, 1, 1); // result
    emit_const_i32(&bc, 2, 1); // i
    emit_const_i32(&bc, 3, 1); // step
    
    uint32_t loop_top = bc.size;
    uint32_t p = emit_if_gt(&bc, 2, 0); // if i > n exit
    emit_mul_i32(&bc, 1, 1, 2); // result *= i
    emit_add_i32(&bc, 2, 2, 3); // i++
    emit_goto_back(&bc, loop_top);
    bc_patch_here(&bc, p);
    
    emit_return(&bc, 1);
    VMError err = bc_run(&ctx, &bc, 4);
    check_err("test_factorial_0", err, VM_OK);
    check_i32("0!", ctx.result.i32, 1);
}

void test_factorial_1() {
    VMContext ctx;
    vm_init(&ctx);
    Bytecode bc;
    bc_init(&bc);
    // 1!
    emit_const_i32(&bc, 0, 1); // n
    emit_const_i32(&bc, 1, 1); // result
    emit_const_i32(&bc, 2, 1); // i
    emit_const_i32(&bc, 3, 1); // step
    
    uint32_t loop_top = bc.size;
    uint32_t p = emit_if_gt(&bc, 2, 0); // if i > n exit
    emit_mul_i32(&bc, 1, 1, 2); // result *= i
    emit_add_i32(&bc, 2, 2, 3); // i++
    emit_goto_back(&bc, loop_top);
    bc_patch_here(&bc, p);
    
    emit_return(&bc, 1);
    VMError err = bc_run(&ctx, &bc, 4);
    check_err("test_factorial_1", err, VM_OK);
    check_i32("1!", ctx.result.i32, 1);
}

void test_double_factorial() {
    VMContext ctx;
    vm_init(&ctx);
    Bytecode bc;
    bc_init(&bc);
    // 9!! = 9*7*5*3*1
    emit_const_i32(&bc, 0, 9); // n (and iterating down)
    emit_const_i32(&bc, 1, 1); // result
    emit_const_i32(&bc, 2, 2); // step
    emit_const_i32(&bc, 3, 1); // limit
    
    uint32_t loop_top = bc.size;
    uint32_t p = emit_if_lt(&bc, 0, 3); // if n < 1 exit
    emit_mul_i32(&bc, 1, 1, 0); // result *= n
    emit_sub_i32(&bc, 0, 0, 2); // n -= 2
    emit_goto_back(&bc, loop_top);
    bc_patch_here(&bc, p);
    
    emit_return(&bc, 1);
    VMError err = bc_run(&ctx, &bc, 4);
    check_err("test_double_factorial", err, VM_OK);
    check_i32("9!!", ctx.result.i32, 945);
}

void test_factorial_sum() {
    VMContext ctx;
    vm_init(&ctx);
    Bytecode bc;
    bc_init(&bc);
    // sum of k! for k=0..6
    emit_const_i32(&bc, 0, 0); // k
    emit_const_i32(&bc, 1, 6); // limit
    emit_const_i32(&bc, 2, 0); // total_sum
    emit_const_i32(&bc, 3, 1); // step 1
    
    uint32_t outer_loop = bc.size;
    uint32_t p_outer = emit_if_gt(&bc, 0, 1);
    
    // compute k!
    emit_const_i32(&bc, 4, 1); // result
    emit_const_i32(&bc, 5, 1); // i
    uint32_t inner_loop = bc.size;
    uint32_t p_inner = emit_if_gt(&bc, 5, 0); // if i > k exit
    emit_mul_i32(&bc, 4, 4, 5); // result *= i
    emit_add_i32(&bc, 5, 5, 3); // i++
    emit_goto_back(&bc, inner_loop);
    bc_patch_here(&bc, p_inner);
    
    emit_add_i32(&bc, 2, 2, 4); // total_sum += k!
    emit_add_i32(&bc, 0, 0, 3); // k++
    emit_goto_back(&bc, outer_loop);
    
    bc_patch_here(&bc, p_outer);
    emit_return(&bc, 2);
    
    VMError err = bc_run(&ctx, &bc, 6);
    check_err("test_factorial_sum", err, VM_OK);
    check_i32("sum k! 0..6", ctx.result.i32, 874);
}

int main() {
    TEST_SECTION("Factorial");
    test_factorial_10();
    test_factorial_15_i64();
    test_factorial_via_native();
    test_factorial_0();
    test_factorial_1();
    test_double_factorial();
    test_factorial_sum();
    print_summary();
    return 0;
}
