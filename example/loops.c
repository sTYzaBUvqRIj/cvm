#include "../vm_builder.h"
#include <stdio.h>
#include <stdint.h>

void test_count_loop() {
    VMContext ctx;
    vm_init(&ctx);
    Bytecode bc;
    bc_init(&bc);
    // for(i=0; i<10; i++) count
    emit_const_i32(&bc, 0, 0); // i
    emit_const_i32(&bc, 1, 10); // limit
    emit_const_i32(&bc, 2, 1); // step
    
    uint32_t loop_top = bc.size;
    uint32_t p = emit_if_ge(&bc, 0, 1); // if i >= 10 exit
    emit_add_i32(&bc, 0, 0, 2); // i++
    emit_goto_back(&bc, loop_top);
    bc_patch_here(&bc, p);
    
    emit_return(&bc, 0);
    VMError err = bc_run(&ctx, &bc, 3);
    check_err("test_count_loop", err, VM_OK);
    check_i32("count loop", ctx.result.i32, 10);
}

void test_sum_1_to_n() {
    VMContext ctx;
    vm_init(&ctx);
    Bytecode bc;
    bc_init(&bc);
    // sum 1 to 100
    emit_const_i32(&bc, 0, 1); // i
    emit_const_i32(&bc, 1, 100); // limit
    emit_const_i32(&bc, 2, 0); // sum
    emit_const_i32(&bc, 3, 1); // step
    
    uint32_t loop_top = bc.size;
    uint32_t p = emit_if_gt(&bc, 0, 1);
    emit_add_i32(&bc, 2, 2, 0); // sum += i
    emit_add_i32(&bc, 0, 0, 3); // i++
    emit_goto_back(&bc, loop_top);
    bc_patch_here(&bc, p);
    
    emit_return(&bc, 2);
    VMError err = bc_run(&ctx, &bc, 4);
    check_err("test_sum_1_to_n", err, VM_OK);
    check_i32("sum 1..100", ctx.result.i32, 5050);
}

void test_power() {
    VMContext ctx;
    vm_init(&ctx);
    Bytecode bc;
    bc_init(&bc);
    // 2^10
    emit_const_i32(&bc, 0, 0); // i
    emit_const_i32(&bc, 1, 10); // limit
    emit_const_i32(&bc, 2, 1); // result
    emit_const_i32(&bc, 3, 1); // step
    emit_const_i32(&bc, 4, 2); // base
    
    uint32_t loop_top = bc.size;
    uint32_t p = emit_if_ge(&bc, 0, 1);
    emit_mul_i32(&bc, 2, 2, 4); // result *= 2
    emit_add_i32(&bc, 0, 0, 3); // i++
    emit_goto_back(&bc, loop_top);
    bc_patch_here(&bc, p);
    
    emit_return(&bc, 2);
    VMError err = bc_run(&ctx, &bc, 5);
    check_err("test_power", err, VM_OK);
    check_i32("power 2^10", ctx.result.i32, 1024);
}

void test_multiply_by_repeated_add() {
    VMContext ctx;
    vm_init(&ctx);
    Bytecode bc;
    bc_init(&bc);
    // 7 * 8
    emit_const_i32(&bc, 0, 0); // i
    emit_const_i32(&bc, 1, 8); // limit
    emit_const_i32(&bc, 2, 0); // sum
    emit_const_i32(&bc, 3, 1); // step
    emit_const_i32(&bc, 4, 7); // addend
    
    uint32_t loop_top = bc.size;
    uint32_t p = emit_if_ge(&bc, 0, 1);
    emit_add_i32(&bc, 2, 2, 4);
    emit_add_i32(&bc, 0, 0, 3);
    emit_goto_back(&bc, loop_top);
    bc_patch_here(&bc, p);
    
    emit_return(&bc, 2);
    VMError err = bc_run(&ctx, &bc, 5);
    check_err("test_multiply_by_repeated_add", err, VM_OK);
    check_i32("7*8", ctx.result.i32, 56);
}

void test_countdown() {
    VMContext ctx;
    vm_init(&ctx);
    Bytecode bc;
    bc_init(&bc);
    // start 10 down to 0
    emit_const_i32(&bc, 0, 10); // i
    emit_const_i32(&bc, 1, 0); // limit
    emit_const_i32(&bc, 2, 1); // step
    emit_const_i32(&bc, 3, 0); // count iterations
    
    uint32_t loop_top = bc.size;
    uint32_t p = emit_if_le(&bc, 0, 1);
    emit_sub_i32(&bc, 0, 0, 2); // i--
    emit_add_i32(&bc, 3, 3, 2); // count++
    emit_goto_back(&bc, loop_top);
    bc_patch_here(&bc, p);
    
    emit_return(&bc, 3);
    VMError err = bc_run(&ctx, &bc, 4);
    check_err("test_countdown", err, VM_OK);
    check_i32("countdown iterations", ctx.result.i32, 10);
}

void test_while_condition() {
    VMContext ctx;
    vm_init(&ctx);
    Bytecode bc;
    bc_init(&bc);
    // while (x < 100) x = x*2 + 1
    emit_const_i32(&bc, 0, 1); // x
    emit_const_i32(&bc, 1, 100); // limit
    emit_const_i32(&bc, 2, 2); // mult
    emit_const_i32(&bc, 3, 1); // add
    
    uint32_t loop_top = bc.size;
    uint32_t p = emit_if_ge(&bc, 0, 1);
    emit_mul_i32(&bc, 0, 0, 2);
    emit_add_i32(&bc, 0, 0, 3);
    emit_goto_back(&bc, loop_top);
    bc_patch_here(&bc, p);
    
    emit_return(&bc, 0);
    VMError err = bc_run(&ctx, &bc, 4);
    check_err("test_while_condition", err, VM_OK);
    check_i32("while condition", ctx.result.i32, 127);
}

void test_gcd() {
    VMContext ctx;
    vm_init(&ctx);
    Bytecode bc;
    bc_init(&bc);
    // gcd(48, 18) = 6
    emit_const_i32(&bc, 0, 48); // a
    emit_const_i32(&bc, 1, 18); // b
    emit_const_i32(&bc, 2, 0); // t
    
    uint32_t loop_top = bc.size;
    uint32_t p = emit_if_eqz(&bc, 1); // if b==0 exit
    emit_rem_i32(&bc, 2, 0, 1); // t = a%b
    emit_move(&bc, 0, 1); // a = b
    emit_move(&bc, 1, 2); // b = t
    emit_goto_back(&bc, loop_top);
    bc_patch_here(&bc, p);
    
    emit_return(&bc, 0);
    VMError err = bc_run(&ctx, &bc, 3);
    check_err("test_gcd", err, VM_OK);
    check_i32("gcd(48,18)", ctx.result.i32, 6);
}

VMError native_sum_array(VMContext* ctx, uint32_t argc, VMRegister* args, VMRegister* out_result) {
    if (argc != 2) return VM_ERR_BAD_ARGC;
    int32_t* arr = (int32_t*)args[0].ptr;
    int32_t count = args[1].i32;
    int32_t sum = 0;
    for (int i=0; i<count; i++) sum += arr[i];
    out_result->i32 = sum;
    return VM_OK;
}

void test_sum_array_via_native() {
    VMContext ctx;
    vm_init(&ctx);
    vm_register_function(&ctx, 1, native_sum_array);
    
    int32_t arr[5] = {1, 2, 3, 4, 5};
    VMRegister regs[3] = {0};
    regs[0].ptr = arr;
    regs[1].i32 = 5;
    
    Bytecode bc;
    bc_init(&bc);
    emit_call2(&bc, 2, 1, 0, 1);
    emit_return(&bc, 2);
    
    VMError err = bc_run_regs(&ctx, &bc, regs, 3);
    check_err("test_sum_array_via_native", err, VM_OK);
    check_i32("sum array native", ctx.result.i32, 15);
}

void test_nested_loop() {
    VMContext ctx;
    vm_init(&ctx);
    Bytecode bc;
    bc_init(&bc);
    // count pairs i+j < 5 for i=0..4, j=0..4
    emit_const_i32(&bc, 0, 0); // i
    emit_const_i32(&bc, 1, 0); // j
    emit_const_i32(&bc, 2, 0); // count
    emit_const_i32(&bc, 3, 5); // limit outer/inner
    emit_const_i32(&bc, 4, 1); // step
    emit_const_i32(&bc, 5, 0); // temp i+j
    
    uint32_t outer = bc.size;
    uint32_t p_outer = emit_if_ge(&bc, 0, 3);
    emit_const_i32(&bc, 1, 0); // j = 0
    
    uint32_t inner = bc.size;
    uint32_t p_inner = emit_if_ge(&bc, 1, 3);
    
    emit_add_i32(&bc, 5, 0, 1); // temp = i+j
    uint32_t skip = emit_if_ge(&bc, 5, 3); // if i+j >= 5 skip count++
    emit_add_i32(&bc, 2, 2, 4); // count++
    bc_patch_here(&bc, skip);
    
    emit_add_i32(&bc, 1, 1, 4); // j++
    emit_goto_back(&bc, inner);
    
    bc_patch_here(&bc, p_inner);
    emit_add_i32(&bc, 0, 0, 4); // i++
    emit_goto_back(&bc, outer);
    
    bc_patch_here(&bc, p_outer);
    emit_return(&bc, 2);
    
    VMError err = bc_run(&ctx, &bc, 6);
    check_err("test_nested_loop", err, VM_OK);
    // 0+0, 0+1, 0+2, 0+3, 0+4
    // 1+0, 1+1, 1+2, 1+3
    // 2+0, 2+1, 2+2
    // 3+0, 3+1
    // 4+0
    // total = 5 + 4 + 3 + 2 + 1 = 15
    check_i32("nested loop count", ctx.result.i32, 15);
}

void test_do_while() {
    VMContext ctx;
    vm_init(&ctx);
    Bytecode bc;
    bc_init(&bc);
    // do { sum+=i; i++ } while (i<=5)
    emit_const_i32(&bc, 0, 1); // i
    emit_const_i32(&bc, 1, 0); // sum
    emit_const_i32(&bc, 2, 1); // step
    emit_const_i32(&bc, 3, 5); // limit
    
    uint32_t loop_top = bc.size;
    emit_add_i32(&bc, 1, 1, 0); // sum += i
    emit_add_i32(&bc, 0, 0, 2); // i++
    uint32_t p = emit_if_le(&bc, 0, 3);
    bc_patch_back(&bc, p, loop_top);
    
    emit_return(&bc, 1);
    VMError err = bc_run(&ctx, &bc, 4);
    check_err("test_do_while", err, VM_OK);
    check_i32("do while sum", ctx.result.i32, 15);
}

int main() {
    TEST_SECTION("Loops");
    test_count_loop();
    test_sum_1_to_n();
    test_power();
    test_multiply_by_repeated_add();
    test_countdown();
    test_while_condition();
    test_gcd();
    test_sum_array_via_native();
    test_nested_loop();
    test_do_while();
    print_summary();
    return 0;
}
