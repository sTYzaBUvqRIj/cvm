#include "../vm_builder.h"
#include <stdio.h>
#include <string.h>

void test_lea_offset() {
    TEST_SECTION("test_lea_offset");
    int32_t arr[2] = {0, 0};
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    VMRegister regs[8]; memset(regs, 0, sizeof(regs));
    regs[0].ptr = &arr[0];
    
    emit_lea(&bc, 1, 0, 4);
    emit_const_i32(&bc, 2, 99);
    emit_store32(&bc, 1, 2);
    
    emit_return_void(&bc);
    bc_run_regs(&ctx, &bc, regs, 8);
    check_i32("arr[1]==99", arr[1], 99);
}

void test_lea_negative() {
    TEST_SECTION("test_lea_negative");
    int32_t arr[2] = {0, 0};
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    VMRegister regs[8]; memset(regs, 0, sizeof(regs));
    regs[0].ptr = &arr[1];
    
    emit_lea(&bc, 2, 0, -4);
    emit_const_i32(&bc, 3, 42);
    emit_store32(&bc, 2, 3);
    
    emit_return_void(&bc);
    bc_run_regs(&ctx, &bc, regs, 8);
    check_i32("arr[0]==42", arr[0], 42);
}

void test_lea_zero() {
    TEST_SECTION("test_lea_zero");
    int32_t val = 0;
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    VMRegister regs[8]; memset(regs, 0, sizeof(regs));
    regs[0].ptr = &val;
    
    emit_lea(&bc, 1, 0, 0);
    emit_const_i32(&bc, 2, 77);
    emit_store32(&bc, 1, 2);
    
    emit_return_void(&bc);
    bc_run_regs(&ctx, &bc, regs, 8);
    check_bool("r1==r0", regs[1].ptr == regs[0].ptr);
    check_i32("val==77", val, 77);
}

void test_load_ptr() {
    TEST_SECTION("test_load_ptr");
    int32_t some_int = 0;
    void* pval = &some_int;
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    VMRegister regs[8]; memset(regs, 0, sizeof(regs));
    regs[0].ptr = &pval;
    
    emit_load_ptr(&bc, 1, 0);
    emit_const_i32(&bc, 2, 123);
    emit_store32(&bc, 1, 2);
    
    emit_return_void(&bc);
    bc_run_regs(&ctx, &bc, regs, 8);
    check_bool("r1.ptr==&some_int", regs[1].ptr == &some_int);
    check_i32("some_int==123", some_int, 123);
}

void test_store_ptr() {
    TEST_SECTION("test_store_ptr");
    void* slot = NULL;
    int32_t some_value = 456;
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    VMRegister regs[8]; memset(regs, 0, sizeof(regs));
    regs[0].ptr = &slot;
    regs[1].ptr = &some_value;
    
    emit_store_ptr(&bc, 0, 1);
    
    emit_return_void(&bc);
    bc_run_regs(&ctx, &bc, regs, 8);
    check_bool("slot==&some_value", slot == &some_value);
}

void test_array_sum_via_ptr() {
    TEST_SECTION("test_array_sum_via_ptr");
    int32_t arr[5] = {1, 2, 3, 4, 5};
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    VMRegister regs[8]; memset(regs, 0, sizeof(regs));
    regs[0].ptr = arr;
    
    emit_const_i32(&bc, 1, 5); // count
    emit_const_i32(&bc, 2, 0); // sum
    emit_const_i32(&bc, 4, 1); // constant 1 for decrement
    
    uint32_t loop_top = bc.size;
    
    emit_load32s(&bc, 3, 0);
    emit_add_i32(&bc, 2, 2, 3);
    emit_lea(&bc, 0, 0, 4);
    emit_sub_i32(&bc, 1, 1, 4);
    
    uint32_t p = emit_if_gtz(&bc, 1);
    bc_patch_back(&bc, p, loop_top);
    
    emit_return(&bc, 2);
    
    bc_run_regs(&ctx, &bc, regs, 8);
    check_i32("sum==15", ctx.result.i32, 15);
}

void test_struct_access() {
    TEST_SECTION("test_struct_access");
    struct { int32_t x; int32_t y; } pt = {10, 20};
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    VMRegister regs[8]; memset(regs, 0, sizeof(regs));
    regs[0].ptr = &pt;
    
    emit_load32s(&bc, 3, 0); // load x
    
    emit_lea(&bc, 1, 0, 4);
    emit_load32s(&bc, 2, 1); // load y
    
    // Store new values back
    emit_const_i32(&bc, 4, 100);
    emit_store32(&bc, 0, 4); // store x
    
    emit_const_i32(&bc, 5, 200);
    emit_store32(&bc, 1, 5); // store y
    
    emit_return_void(&bc);
    bc_run_regs(&ctx, &bc, regs, 8);
    
    check_i32("x==10", regs[3].i32, 10);
    check_i32("y==20", regs[2].i32, 20);
    check_i32("pt.x==100", pt.x, 100);
    check_i32("pt.y==200", pt.y, 200);
}

void test_ptr_chain() {
    TEST_SECTION("test_ptr_chain");
    int val = 42;
    int* p = &val;
    int** pp = &p;
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    VMRegister regs[8]; memset(regs, 0, sizeof(regs));
    regs[0].ptr = &pp;
    
    emit_load_ptr(&bc, 1, 0);
    emit_load_ptr(&bc, 2, 1);
    emit_load32s(&bc, 3, 2);
    
    emit_return_void(&bc);
    bc_run_regs(&ctx, &bc, regs, 8);
    check_i32("val==42", regs[3].i32, 42);
}

int main() {
    test_lea_offset();
    test_lea_negative();
    test_lea_zero();
    test_load_ptr();
    test_store_ptr();
    test_array_sum_via_ptr();
    test_struct_access();
    test_ptr_chain();
    print_summary();
    return 0;
}
