#include "../vm_builder.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

void test_i32_to_i8() {
    TEST_SECTION("test_i32_to_i8");
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    VMRegister regs[4]; memset(regs, 0, sizeof(regs));
    emit_const_i32(&bc, 0, 127); emit_i32_to_i8(&bc, 0, 0);
    emit_const_i32(&bc, 1, 128); emit_i32_to_i8(&bc, 1, 1);
    emit_const_i32(&bc, 2, -1); emit_i32_to_i8(&bc, 2, 2);
    emit_const_i32(&bc, 3, 300); emit_i32_to_i8(&bc, 3, 3);
    emit_return_void(&bc);
    bc_run_regs(&ctx, &bc, regs, 4);
    check_i64("127->127", regs[0].i64, 127);
    check_i64("128->-128", regs[1].i64, -128);
    check_i64("-1->-1", regs[2].i64, -1);
    check_i64("300->44", regs[3].i64, 44);
}

void test_i32_to_i16() {
    TEST_SECTION("test_i32_to_i16");
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    VMRegister regs[3]; memset(regs, 0, sizeof(regs));
    emit_const_i32(&bc, 0, 32767); emit_i32_to_i16(&bc, 0, 0);
    emit_const_i32(&bc, 1, 32768); emit_i32_to_i16(&bc, 1, 1);
    emit_const_i32(&bc, 2, -1); emit_i32_to_i16(&bc, 2, 2);
    emit_return_void(&bc);
    bc_run_regs(&ctx, &bc, regs, 3);
    check_i64("32767->32767", regs[0].i64, 32767);
    check_i64("32768->-32768", regs[1].i64, -32768);
    check_i64("-1->-1", regs[2].i64, -1);
}

void test_i32_to_i64() {
    TEST_SECTION("test_i32_to_i64");
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    VMRegister regs[3]; memset(regs, 0, sizeof(regs));
    emit_const_i32(&bc, 0, 42); emit_i32_to_i64(&bc, 0, 0);
    emit_const_i32(&bc, 1, -1); emit_i32_to_i64(&bc, 1, 1);
    emit_const_i32(&bc, 2, 2147483647); emit_i32_to_i64(&bc, 2, 2);
    emit_return_void(&bc);
    bc_run_regs(&ctx, &bc, regs, 3);
    check_i64("42->42LL", regs[0].i64, 42LL);
    check_i64("-1->-1LL", regs[1].i64, -1LL);
    check_i64("INT32_MAX", regs[2].i64, 2147483647LL);
}

void test_i32_to_f32() {
    TEST_SECTION("test_i32_to_f32");
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    VMRegister regs[3]; memset(regs, 0, sizeof(regs));
    emit_const_i32(&bc, 0, 10); emit_i32_to_f32(&bc, 0, 0);
    emit_const_i32(&bc, 1, -5); emit_i32_to_f32(&bc, 1, 1);
    emit_const_i32(&bc, 2, 1000000); emit_i32_to_f32(&bc, 2, 2);
    emit_return_void(&bc);
    bc_run_regs(&ctx, &bc, regs, 3);
    check_f32("10->10.0f", regs[0].f32, 10.0f, 0.0001f);
    check_f32("-5->-5.0f", regs[1].f32, -5.0f, 0.0001f);
    check_f32("1000000", regs[2].f32, 1000000.0f, 0.1f);
}

void test_i32_to_f64() {
    TEST_SECTION("test_i32_to_f64");
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    VMRegister regs[1]; memset(regs, 0, sizeof(regs));
    emit_const_i32(&bc, 0, 999999999); emit_i32_to_f64(&bc, 0, 0);
    emit_return_void(&bc);
    bc_run_regs(&ctx, &bc, regs, 1);
    check_f64("999999999", regs[0].f64, 999999999.0, 0.000001);
}

void test_i64_to_i32() {
    TEST_SECTION("test_i64_to_i32");
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    VMRegister regs[2]; memset(regs, 0, sizeof(regs));
    emit_const_i64(&bc, 0, 42LL); emit_i64_to_i32(&bc, 0, 0);
    emit_const_i64(&bc, 1, 9223372036854775807LL); emit_i64_to_i32(&bc, 1, 1);
    emit_return_void(&bc);
    bc_run_regs(&ctx, &bc, regs, 2);
    check_i32("42LL->42", regs[0].i32, 42);
    check_i32("INT64_MAX->-1", regs[1].i32, -1);
}

void test_i64_to_f32() {
    TEST_SECTION("test_i64_to_f32");
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    VMRegister regs[1]; memset(regs, 0, sizeof(regs));
    emit_const_i64(&bc, 0, 1000000LL); emit_i64_to_f32(&bc, 0, 0);
    emit_return_void(&bc);
    bc_run_regs(&ctx, &bc, regs, 1);
    check_f32("1000000LL->1000000.0f", regs[0].f32, 1000000.0f, 0.1f);
}

void test_i64_to_f64() {
    TEST_SECTION("test_i64_to_f64");
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    VMRegister regs[1]; memset(regs, 0, sizeof(regs));
    emit_const_i64(&bc, 0, 9007199254740992LL); emit_i64_to_f64(&bc, 0, 0);
    emit_return_void(&bc);
    bc_run_regs(&ctx, &bc, regs, 1);
    check_f64("9007199254740992LL->9007199254740992.0", regs[0].f64, 9007199254740992.0, 0.0);
}

void test_f32_to_i32() {
    TEST_SECTION("test_f32_to_i32");
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    VMRegister regs[3]; memset(regs, 0, sizeof(regs));
    emit_const_f32(&bc, 0, 3.7f); emit_f32_to_i32(&bc, 0, 0);
    emit_const_f32(&bc, 1, -3.7f); emit_f32_to_i32(&bc, 1, 1);
    emit_const_f32(&bc, 2, 0.9f); emit_f32_to_i32(&bc, 2, 2);
    emit_return_void(&bc);
    bc_run_regs(&ctx, &bc, regs, 3);
    check_i32("3.7f->3", regs[0].i32, 3);
    check_i32("-3.7f->-3", regs[1].i32, -3);
    check_i32("0.9f->0", regs[2].i32, 0);
}

void test_f32_to_i64() {
    TEST_SECTION("test_f32_to_i64");
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    VMRegister regs[1]; memset(regs, 0, sizeof(regs));
    emit_const_f32(&bc, 0, 1e9f); emit_f32_to_i64(&bc, 0, 0);
    emit_return_void(&bc);
    bc_run_regs(&ctx, &bc, regs, 1);
    check_i64("1e9f->1000000000LL", regs[0].i64, 1000000000LL);
}

void test_f32_to_f64() {
    TEST_SECTION("test_f32_to_f64");
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    VMRegister regs[1]; memset(regs, 0, sizeof(regs));
    emit_const_f32(&bc, 0, 3.14f); emit_f32_to_f64(&bc, 0, 0);
    emit_return_void(&bc);
    bc_run_regs(&ctx, &bc, regs, 1);
    check_f64("3.14f->3.14", regs[0].f64, 3.14, 0.000001);
}

void test_f64_to_i32() {
    TEST_SECTION("test_f64_to_i32");
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    VMRegister regs[2]; memset(regs, 0, sizeof(regs));
    emit_const_f64(&bc, 0, 3.999); emit_f64_to_i32(&bc, 0, 0);
    emit_const_f64(&bc, 1, -3.999); emit_f64_to_i32(&bc, 1, 1);
    emit_return_void(&bc);
    bc_run_regs(&ctx, &bc, regs, 2);
    check_i32("3.999->3", regs[0].i32, 3);
    check_i32("-3.999->-3", regs[1].i32, -3);
}

void test_f64_to_i64() {
    TEST_SECTION("test_f64_to_i64");
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    VMRegister regs[1]; memset(regs, 0, sizeof(regs));
    emit_const_f64(&bc, 0, 1e15); emit_f64_to_i64(&bc, 0, 0);
    emit_return_void(&bc);
    bc_run_regs(&ctx, &bc, regs, 1);
    check_i64("1e15->1000000000000000LL", regs[0].i64, 1000000000000000LL);
}

void test_f64_to_f32() {
    TEST_SECTION("test_f64_to_f32");
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    VMRegister regs[1]; memset(regs, 0, sizeof(regs));
    emit_const_f64(&bc, 0, 3.14159265358979); emit_f64_to_f32(&bc, 0, 0);
    emit_return_void(&bc);
    bc_run_regs(&ctx, &bc, regs, 1);
    check_f32("3.14159265358979->3.14159f", regs[0].f32, 3.1415927f, 0.000001f);
}

void test_chain_conversion() {
    TEST_SECTION("test_chain_conversion");
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    VMRegister regs[2]; memset(regs, 0, sizeof(regs));
    emit_const_i32(&bc, 0, 255);
    emit_i32_to_f64(&bc, 0, 0);
    emit_const_f64(&bc, 1, 2.0);
    emit_mul_f64(&bc, 0, 0, 1);
    emit_f64_to_i32(&bc, 0, 0);
    emit_return_void(&bc);
    bc_run_regs(&ctx, &bc, regs, 2);
    check_i32("chain conversion", regs[0].i32, 510);
}

int main() {
    test_i32_to_i8();
    test_i32_to_i16();
    test_i32_to_i64();
    test_i32_to_f32();
    test_i32_to_f64();
    test_i64_to_i32();
    test_i64_to_f32();
    test_i64_to_f64();
    test_f32_to_i32();
    test_f32_to_i64();
    test_f32_to_f64();
    test_f64_to_i32();
    test_f64_to_i64();
    test_f64_to_f32();
    test_chain_conversion();
    print_summary();
    return 0;
}
