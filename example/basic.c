#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "../vm_builder.h"

static void test_nop(void) {
    VMContext ctx;
    vm_init(&ctx);
    Bytecode bc;
    bc_init(&bc);
    
    emit_nop(&bc);
    emit_nop(&bc);
    emit_nop(&bc);
    emit_return_void(&bc);
    
    VMError err = bc_run(&ctx, &bc, 1);
    check_err("test_nop", err, VM_OK);
}

static void test_const_i8(void) {
    VMContext ctx;
    vm_init(&ctx);
    Bytecode bc;
    bc_init(&bc);
    
    emit_const_i8(&bc, 0, 42);
    emit_return(&bc, 0);
    VMError err = bc_run(&ctx, &bc, 1);
    check_err("test_const_i8 (42) err", err, VM_OK);
    check_i32("test_const_i8 (42)", ctx.result.i32, 42);
    
    bc_init(&bc);
    emit_const_i8(&bc, 0, -5);
    emit_return(&bc, 0);
    err = bc_run(&ctx, &bc, 1);
    check_err("test_const_i8 (-5) err", err, VM_OK);
    check_i32("test_const_i8 (-5)", ctx.result.i32, -5);
}

static void test_const_i16(void) {
    VMContext ctx;
    vm_init(&ctx);
    Bytecode bc;
    bc_init(&bc);
    
    emit_const_i16(&bc, 0, 1000);
    emit_return(&bc, 0);
    VMError err = bc_run(&ctx, &bc, 1);
    check_err("test_const_i16 (1000) err", err, VM_OK);
    check_i32("test_const_i16 (1000)", ctx.result.i32, 1000);
    
    bc_init(&bc);
    emit_const_i16(&bc, 0, -1000);
    emit_return(&bc, 0);
    err = bc_run(&ctx, &bc, 1);
    check_err("test_const_i16 (-1000) err", err, VM_OK);
    check_i32("test_const_i16 (-1000)", ctx.result.i32, -1000);
}

static void test_const_i32(void) {
    VMContext ctx;
    vm_init(&ctx);
    Bytecode bc;
    bc_init(&bc);
    
    emit_const_i32(&bc, 0, 0x7FFFFFFF);
    emit_return(&bc, 0);
    VMError err = bc_run(&ctx, &bc, 1);
    check_err("test_const_i32 (MAX) err", err, VM_OK);
    check_i32("test_const_i32 (MAX)", ctx.result.i32, 0x7FFFFFFF);
    
    bc_init(&bc);
    emit_const_i32(&bc, 0, -1);
    emit_return(&bc, 0);
    err = bc_run(&ctx, &bc, 1);
    check_err("test_const_i32 (-1) err", err, VM_OK);
    check_i32("test_const_i32 (-1)", ctx.result.i32, -1);
}

static void test_const_i64(void) {
    VMContext ctx;
    vm_init(&ctx);
    Bytecode bc;
    bc_init(&bc);
    
    emit_const_i64(&bc, 0, 9223372036854775807LL); // INT64_MAX
    emit_return(&bc, 0);
    VMError err = bc_run(&ctx, &bc, 1);
    check_err("test_const_i64 (MAX) err", err, VM_OK);
    check_i64("test_const_i64 (MAX)", ctx.result.i64, 9223372036854775807LL);
    
    bc_init(&bc);
    emit_const_i64(&bc, 0, -9223372036854775807LL - 1); // INT64_MIN
    emit_return(&bc, 0);
    err = bc_run(&ctx, &bc, 1);
    check_err("test_const_i64 (MIN) err", err, VM_OK);
    check_i64("test_const_i64 (MIN)", ctx.result.i64, -9223372036854775807LL - 1);
}

static void test_const_f32(void) {
    VMContext ctx;
    vm_init(&ctx);
    Bytecode bc;
    bc_init(&bc);
    
    emit_const_f32(&bc, 0, 3.14f);
    emit_return(&bc, 0);
    VMError err = bc_run(&ctx, &bc, 1);
    check_err("test_const_f32 err", err, VM_OK);
    check_f32("test_const_f32", ctx.result.f32, 3.14f, 0.001f);
}

static void test_const_f64(void) {
    VMContext ctx;
    vm_init(&ctx);
    Bytecode bc;
    bc_init(&bc);
    
    emit_const_f64(&bc, 0, 2.718281828);
    emit_return(&bc, 0);
    VMError err = bc_run(&ctx, &bc, 1);
    check_err("test_const_f64 err", err, VM_OK);
    check_f64("test_const_f64", ctx.result.f64, 2.718281828, 0.000000001);
}

static void test_move(void) {
    VMContext ctx;
    vm_init(&ctx);
    Bytecode bc;
    bc_init(&bc);
    
    emit_const_i32(&bc, 0, 42);
    emit_move(&bc, 1, 0);
    emit_move(&bc, 2, 1);
    emit_return(&bc, 2);
    
    VMError err = bc_run(&ctx, &bc, 3);
    check_err("test_move err", err, VM_OK);
    check_i32("test_move", ctx.result.i32, 42);
}

static void test_move_wide(void) {
    VMContext ctx;
    vm_init(&ctx);
    Bytecode bc;
    bc_init(&bc);
    
    emit_const_i32(&bc, 10, 88);
    emit_move(&bc, 0, 10);
    emit_return(&bc, 0);
    
    VMError err = bc_run(&ctx, &bc, 11);
    check_err("test_move_wide err", err, VM_OK);
    check_i32("test_move_wide", ctx.result.i32, 88);
}

static void test_return_void(void) {
    VMContext ctx;
    vm_init(&ctx);
    Bytecode bc;
    bc_init(&bc);
    
    emit_return_void(&bc);
    
    VMError err = bc_run(&ctx, &bc, 1);
    check_err("test_return_void err", err, VM_OK);
}

int main(void) {
    TEST_SECTION("basic.c");
    test_nop();
    test_const_i8();
    test_const_i16();
    test_const_i32();
    test_const_i64();
    test_const_f32();
    test_const_f64();
    test_move();
    test_move_wide();
    test_return_void();
    
    print_summary();
    return 0;
}
