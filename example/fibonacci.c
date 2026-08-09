#include "../vm_builder.h"
#include <stdio.h>
#include <stdint.h>

void test_fib_iterative() {
    VMContext ctx;
    vm_init(&ctx);
    Bytecode bc;
    bc_init(&bc);
    emit_const_i32(&bc, 0, 10); // n
    emit_const_i32(&bc, 1, 0); // prev
    emit_const_i32(&bc, 2, 1); // curr
    emit_const_i32(&bc, 3, 1); // i
    emit_const_i32(&bc, 4, 0); // temp
    emit_const_i32(&bc, 5, 1); // step
    
    uint32_t loop_top = bc.size;
    uint32_t p = emit_if_ge(&bc, 3, 0); // if i >= n exit
    emit_move(&bc, 4, 2); // temp = curr
    emit_add_i32(&bc, 2, 1, 2); // curr = prev + curr
    emit_move(&bc, 1, 4); // prev = temp
    emit_add_i32(&bc, 3, 3, 5); // i++
    emit_goto_back(&bc, loop_top);
    bc_patch_here(&bc, p);
    
    emit_return(&bc, 2);
    VMError err = bc_run(&ctx, &bc, 6);
    check_err("test_fib_iterative", err, VM_OK);
    check_i32("fib(10)", ctx.result.i32, 55);
}

void test_fib_iterative_20() {
    VMContext ctx;
    vm_init(&ctx);
    Bytecode bc;
    bc_init(&bc);
    emit_const_i32(&bc, 0, 20); // n
    emit_const_i32(&bc, 1, 0); // prev
    emit_const_i32(&bc, 2, 1); // curr
    emit_const_i32(&bc, 3, 1); // i
    emit_const_i32(&bc, 4, 0); // temp
    emit_const_i32(&bc, 5, 1); // step
    
    uint32_t loop_top = bc.size;
    uint32_t p = emit_if_ge(&bc, 3, 0); // if i >= n exit
    emit_move(&bc, 4, 2); // temp = curr
    emit_add_i32(&bc, 2, 1, 2); // curr = prev + curr
    emit_move(&bc, 1, 4); // prev = temp
    emit_add_i32(&bc, 3, 3, 5); // i++
    emit_goto_back(&bc, loop_top);
    bc_patch_here(&bc, p);
    
    emit_return(&bc, 2);
    VMError err = bc_run(&ctx, &bc, 6);
    check_err("test_fib_iterative_20", err, VM_OK);
    check_i32("fib(20)", ctx.result.i32, 6765);
}

int32_t fib_c(int32_t n) {
    if (n <= 0) return 0;
    if (n == 1) return 1;
    return fib_c(n-1) + fib_c(n-2);
}

VMError native_fib(VMContext* ctx, uint32_t argc, VMRegister* args, VMRegister* out_result) {
    if (argc != 1) return VM_ERR_BAD_ARGC;
    out_result->i32 = fib_c(args[0].i32);
    return VM_OK;
}

void test_fib_via_native() {
    VMContext ctx;
    vm_init(&ctx);
    vm_register_function(&ctx, 1, native_fib);
    
    Bytecode bc;
    bc_init(&bc);
    emit_const_i32(&bc, 0, 15);
    emit_call1(&bc, 1, 1, 0);
    emit_return(&bc, 1);
    
    VMError err = bc_run(&ctx, &bc, 2);
    check_err("test_fib_via_native", err, VM_OK);
    check_i32("fib(15) native", ctx.result.i32, 610);
}

void test_fib_first_10() {
    VMContext ctx;
    vm_init(&ctx);
    
    int32_t arr[10] = {0};
    VMRegister regs[7] = {0};
    regs[0].ptr = arr;
    
    Bytecode bc;
    bc_init(&bc);
    emit_const_i32(&bc, 1, 10); // limit
    emit_const_i32(&bc, 2, 0); // prev
    emit_const_i32(&bc, 3, 1); // curr
    emit_const_i32(&bc, 4, 0); // i
    emit_const_i32(&bc, 5, 1); // step
    emit_const_i32(&bc, 6, 0); // temp
    
    uint32_t loop_top = bc.size;
    uint32_t p = emit_if_ge(&bc, 4, 1);
    
    // store curr in arr[i]
    emit_store32(&bc, 0, 2);
    emit_add_i32(&bc, 4, 4, 5); // i++
    
    // next fib
    emit_move(&bc, 6, 3); // temp = curr
    emit_add_i32(&bc, 3, 2, 3); // curr = prev + curr
    emit_move(&bc, 2, 6); // prev = temp
    
    // advance ptr
    emit_const_i32(&bc, 6, 4);
    emit_lea(&bc, 0, 0, 4);
    
    emit_goto_back(&bc, loop_top);
    bc_patch_here(&bc, p);
    
    emit_return_void(&bc);
    
    VMError err = bc_run_regs(&ctx, &bc, regs, 7);
    check_err("test_fib_first_10", err, VM_OK);
    check_i32("fib[9]", arr[9], 34);
}

void test_fib_i64() {
    VMContext ctx;
    vm_init(&ctx);
    Bytecode bc;
    bc_init(&bc);
    emit_const_i64(&bc, 0, 50); // n
    emit_const_i64(&bc, 1, 0); // prev
    emit_const_i64(&bc, 2, 1); // curr
    emit_const_i64(&bc, 3, 1); // i
    emit_const_i64(&bc, 4, 0); // temp
    emit_const_i64(&bc, 5, 1); // step
    
    uint32_t loop_top = bc.size;
    uint32_t p = emit_if_ge(&bc, 3, 0); // if i >= n exit
    emit_move(&bc, 4, 2); // temp = curr
    emit_add_i64(&bc, 2, 1, 2); // curr = prev + curr
    emit_move(&bc, 1, 4); // prev = temp
    emit_add_i64(&bc, 3, 3, 5); // i++
    emit_goto_back(&bc, loop_top);
    bc_patch_here(&bc, p);
    
    emit_return(&bc, 2);
    VMError err = bc_run(&ctx, &bc, 6);
    check_err("test_fib_i64", err, VM_OK);
    check_i64("fib(50) i64", ctx.result.i64, 12586269025LL);
}

int main() {
    TEST_SECTION("Fibonacci");
    test_fib_iterative();
    test_fib_iterative_20();
    test_fib_via_native();
    test_fib_first_10();
    test_fib_i64();
    print_summary();
    return 0;
}
