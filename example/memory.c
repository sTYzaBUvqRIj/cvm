#include "../vm_builder.h"
#include <stdio.h>
#include <string.h>

void test_store8_load8() {
    TEST_SECTION("test_store8_load8");
    uint8_t host_var = 0;
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    VMRegister regs[8]; memset(regs, 0, sizeof(regs));
    regs[0].ptr = &host_var;
    emit_const_i32(&bc, 1, 0xAB);
    emit_store8(&bc, 0, 1);
    emit_load8(&bc, 2, 0);
    emit_return_void(&bc);
    bc_run_regs(&ctx, &bc, regs, 8);
    check_u64("u64=0xAB", regs[2].u64, 0xAB);
    check_i32("host_var=0xAB", host_var, 0xAB);
}

void test_store8_load8s() {
    TEST_SECTION("test_store8_load8s");
    uint8_t host_var = 0;
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    VMRegister regs[8]; memset(regs, 0, sizeof(regs));
    regs[0].ptr = &host_var;
    emit_const_i32(&bc, 1, 0xFF);
    emit_store8(&bc, 0, 1);
    emit_load8s(&bc, 2, 0);
    emit_return_void(&bc);
    bc_run_regs(&ctx, &bc, regs, 8);
    check_i64("i64=-1", regs[2].i64, -1);
    check_i32("host_var=0xFF", host_var, 0xFF);
}

void test_store16_load16() {
    TEST_SECTION("test_store16_load16");
    uint16_t host_var = 0;
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    VMRegister regs[8]; memset(regs, 0, sizeof(regs));
    regs[0].ptr = &host_var;
    emit_const_i32(&bc, 1, 0x1234);
    emit_store16(&bc, 0, 1);
    emit_load16(&bc, 2, 0);
    emit_return_void(&bc);
    bc_run_regs(&ctx, &bc, regs, 8);
    check_u64("u64=0x1234", regs[2].u64, 0x1234);
    check_i32("host_var=0x1234", host_var, 0x1234);
}

void test_store16_load16s() {
    TEST_SECTION("test_store16_load16s");
    uint16_t host_var = 0;
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    VMRegister regs[8]; memset(regs, 0, sizeof(regs));
    regs[0].ptr = &host_var;
    emit_const_i32(&bc, 1, 0xFFFF);
    emit_store16(&bc, 0, 1);
    emit_load16s(&bc, 2, 0);
    emit_return_void(&bc);
    bc_run_regs(&ctx, &bc, regs, 8);
    check_i64("i64=-1", regs[2].i64, -1);
    check_i32("host_var=0xFFFF", host_var, 0xFFFF);
}

void test_store32_load32() {
    TEST_SECTION("test_store32_load32");
    uint32_t host_var = 0;
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    VMRegister regs[8]; memset(regs, 0, sizeof(regs));
    regs[0].ptr = &host_var;
    emit_const_i64(&bc, 1, 0xDEADBEEFLL);
    emit_store32(&bc, 0, 1);
    emit_load32(&bc, 2, 0);
    emit_return_void(&bc);
    bc_run_regs(&ctx, &bc, regs, 8);
    check_u64("u64=0xDEADBEEF", regs[2].u64, 0xDEADBEEFLL);
    check_u32("host_var=0xDEADBEEF", host_var, 0xDEADBEEF);
}

void test_store32_load32s() {
    TEST_SECTION("test_store32_load32s");
    int32_t host_var = 0;
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    VMRegister regs[8]; memset(regs, 0, sizeof(regs));
    regs[0].ptr = &host_var;
    emit_const_i32(&bc, 1, -1);
    emit_store32(&bc, 0, 1);
    emit_load32s(&bc, 2, 0);
    emit_return_void(&bc);
    bc_run_regs(&ctx, &bc, regs, 8);
    check_i64("i64=-1", regs[2].i64, -1);
    check_i32("host_var=-1", host_var, -1);
}

void test_store64_load64() {
    TEST_SECTION("test_store64_load64");
    uint64_t host_var = 0;
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    VMRegister regs[8]; memset(regs, 0, sizeof(regs));
    regs[0].ptr = &host_var;
    emit_const_i64(&bc, 1, 0xCAFEBABEDEADBEEFLL);
    emit_store64(&bc, 0, 1);
    emit_load64(&bc, 2, 0);
    emit_return_void(&bc);
    bc_run_regs(&ctx, &bc, regs, 8);
    check_u64("exact u64", regs[2].u64, 0xCAFEBABEDEADBEEFLL);
    check_u64("host_var", host_var, 0xCAFEBABEDEADBEEFLL);
}

void test_store_ptr_load_ptr() {
    TEST_SECTION("test_store_ptr_load_ptr");
    void* host_var = NULL;
    int dummy = 42;
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    VMRegister regs[8]; memset(regs, 0, sizeof(regs));
    regs[0].ptr = &host_var;
    regs[1].ptr = &dummy;
    emit_store_ptr(&bc, 0, 1);
    emit_load_ptr(&bc, 2, 0);
    emit_return_void(&bc);
    bc_run_regs(&ctx, &bc, regs, 8);
    check_bool("ptr==original", regs[2].ptr == &dummy);
    check_bool("host_var==original", host_var == &dummy);
}

void test_byte_array() {
    TEST_SECTION("test_byte_array");
    uint8_t arr[4] = {0};
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    VMRegister regs[8]; memset(regs, 0, sizeof(regs));
    regs[0].ptr = arr;
    
    emit_const_i32(&bc, 1, 10);
    emit_lea(&bc, 2, 0, 0);
    emit_store8(&bc, 2, 1);
    
    emit_const_i32(&bc, 1, 20);
    emit_lea(&bc, 2, 0, 1);
    emit_store8(&bc, 2, 1);
    
    emit_const_i32(&bc, 1, 30);
    emit_lea(&bc, 2, 0, 2);
    emit_store8(&bc, 2, 1);
    
    emit_const_i32(&bc, 1, 40);
    emit_lea(&bc, 2, 0, 3);
    emit_store8(&bc, 2, 1);
    
    emit_lea(&bc, 3, 0, 2);
    emit_load8(&bc, 4, 3);
    emit_return_void(&bc);
    bc_run_regs(&ctx, &bc, regs, 8);
    
    check_i32("arr[2]==30", regs[4].i32, 30);
    check_i32("arr[0]", arr[0], 10);
    check_i32("arr[1]", arr[1], 20);
    check_i32("arr[2]", arr[2], 30);
    check_i32("arr[3]", arr[3], 40);
}

void test_load_modify_store() {
    TEST_SECTION("test_load_modify_store");
    int32_t host_var = 50;
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    VMRegister regs[8]; memset(regs, 0, sizeof(regs));
    regs[0].ptr = &host_var;
    
    emit_load32s(&bc, 1, 0);
    emit_const_i32(&bc, 2, 100);
    emit_add_i32(&bc, 1, 1, 2);
    emit_store32(&bc, 0, 1);
    
    emit_return_void(&bc);
    bc_run_regs(&ctx, &bc, regs, 8);
    
    check_i32("host_var==150", host_var, 150);
}

int main() {
    test_store8_load8();
    test_store8_load8s();
    test_store16_load16();
    test_store16_load16s();
    test_store32_load32();
    test_store32_load32s();
    test_store64_load64();
    test_store_ptr_load_ptr();
    test_byte_array();
    test_load_modify_store();
    print_summary();
    return 0;
}
