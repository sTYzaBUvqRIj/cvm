#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "../vm_builder.h"

static void test_add_i32(VMContext* ctx) {
    Bytecode bc;
    bc_init(&bc);
    emit_const_i32(&bc, 0, 5);
    emit_const_i32(&bc, 1, 3);
    emit_add_i32(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(ctx, &bc, 3);
    check_i32("add_i32 (5+3)", ctx->result.i32, 8);
    
    bc_init(&bc);
    emit_const_i32(&bc, 0, 0);
    emit_const_i32(&bc, 1, 0);
    emit_add_i32(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(ctx, &bc, 3);
    check_i32("add_i32 (0+0)", ctx->result.i32, 0);

    bc_init(&bc);
    emit_const_i32(&bc, 0, -5);
    emit_const_i32(&bc, 1, 3);
    emit_add_i32(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(ctx, &bc, 3);
    check_i32("add_i32 (-5+3)", ctx->result.i32, -2);
    
    bc_init(&bc);
    emit_const_i32(&bc, 0, 0x7FFFFFFF);
    emit_const_i32(&bc, 1, 0);
    emit_add_i32(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(ctx, &bc, 3);
    check_i32("add_i32 (INT32_MAX+0)", ctx->result.i32, 0x7FFFFFFF);
}

static void test_sub_i32(VMContext* ctx) {
    Bytecode bc;
    bc_init(&bc);
    emit_const_i32(&bc, 0, 10);
    emit_const_i32(&bc, 1, 4);
    emit_sub_i32(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(ctx, &bc, 3);
    check_i32("sub_i32 (10-4)", ctx->result.i32, 6);

    bc_init(&bc);
    emit_const_i32(&bc, 0, 0);
    emit_const_i32(&bc, 1, 5);
    emit_sub_i32(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(ctx, &bc, 3);
    check_i32("sub_i32 (0-5)", ctx->result.i32, -5);
    
    bc_init(&bc);
    emit_const_i32(&bc, 0, -3);
    emit_const_i32(&bc, 1, -3);
    emit_sub_i32(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(ctx, &bc, 3);
    check_i32("sub_i32 (-3-(-3))", ctx->result.i32, 0);
}

static void test_mul_i32(VMContext* ctx) {
    Bytecode bc;
    bc_init(&bc);
    emit_const_i32(&bc, 0, 7);
    emit_const_i32(&bc, 1, 6);
    emit_mul_i32(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(ctx, &bc, 3);
    check_i32("mul_i32 (7*6)", ctx->result.i32, 42);

    bc_init(&bc);
    emit_const_i32(&bc, 0, 0);
    emit_const_i32(&bc, 1, 999);
    emit_mul_i32(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(ctx, &bc, 3);
    check_i32("mul_i32 (0*999)", ctx->result.i32, 0);

    bc_init(&bc);
    emit_const_i32(&bc, 0, -4);
    emit_const_i32(&bc, 1, -5);
    emit_mul_i32(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(ctx, &bc, 3);
    check_i32("mul_i32 (-4*-5)", ctx->result.i32, 20);

    bc_init(&bc);
    emit_const_i32(&bc, 0, -3);
    emit_const_i32(&bc, 1, 4);
    emit_mul_i32(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(ctx, &bc, 3);
    check_i32("mul_i32 (-3*4)", ctx->result.i32, -12);
}

static void test_div_i32(VMContext* ctx) {
    Bytecode bc;
    bc_init(&bc);
    emit_const_i32(&bc, 0, 20);
    emit_const_i32(&bc, 1, 4);
    emit_div_i32(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(ctx, &bc, 3);
    check_i32("div_i32 (20/4)", ctx->result.i32, 5);

    bc_init(&bc);
    emit_const_i32(&bc, 0, 7);
    emit_const_i32(&bc, 1, 2);
    emit_div_i32(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(ctx, &bc, 3);
    check_i32("div_i32 (7/2)", ctx->result.i32, 3);

    bc_init(&bc);
    emit_const_i32(&bc, 0, -7);
    emit_const_i32(&bc, 1, 2);
    emit_div_i32(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(ctx, &bc, 3);
    check_i32("div_i32 (-7/2)", ctx->result.i32, -3);

    bc_init(&bc);
    emit_const_i32(&bc, 0, 7);
    emit_const_i32(&bc, 1, -2);
    emit_div_i32(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(ctx, &bc, 3);
    check_i32("div_i32 (7/-2)", ctx->result.i32, -3);
}

static void test_rem_i32(VMContext* ctx) {
    Bytecode bc;
    bc_init(&bc);
    emit_const_i32(&bc, 0, 17);
    emit_const_i32(&bc, 1, 5);
    emit_rem_i32(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(ctx, &bc, 3);
    check_i32("rem_i32 (17%5)", ctx->result.i32, 2);

    bc_init(&bc);
    emit_const_i32(&bc, 0, 7);
    emit_const_i32(&bc, 1, 7);
    emit_rem_i32(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(ctx, &bc, 3);
    check_i32("rem_i32 (7%7)", ctx->result.i32, 0);

    bc_init(&bc);
    emit_const_i32(&bc, 0, -7);
    emit_const_i32(&bc, 1, 3);
    emit_rem_i32(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(ctx, &bc, 3);
    check_i32("rem_i32 (-7%3)", ctx->result.i32, -1);

    bc_init(&bc);
    emit_const_i32(&bc, 0, 7);
    emit_const_i32(&bc, 1, -3);
    emit_rem_i32(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(ctx, &bc, 3);
    check_i32("rem_i32 (7%-3)", ctx->result.i32, 1);
}

static void test_neg_i32(VMContext* ctx) {
    Bytecode bc;
    bc_init(&bc);
    emit_const_i32(&bc, 0, 5);
    emit_neg_i32(&bc, 1, 0);
    emit_return(&bc, 1);
    bc_run(ctx, &bc, 2);
    check_i32("neg_i32 (5)", ctx->result.i32, -5);

    bc_init(&bc);
    emit_const_i32(&bc, 0, -5);
    emit_neg_i32(&bc, 1, 0);
    emit_return(&bc, 1);
    bc_run(ctx, &bc, 2);
    check_i32("neg_i32 (-5)", ctx->result.i32, 5);

    bc_init(&bc);
    emit_const_i32(&bc, 0, 0);
    emit_neg_i32(&bc, 1, 0);
    emit_return(&bc, 1);
    bc_run(ctx, &bc, 2);
    check_i32("neg_i32 (0)", ctx->result.i32, 0);
}

static void test_add_i64(VMContext* ctx) {
    Bytecode bc;
    bc_init(&bc);
    emit_const_i64(&bc, 0, 1000000000LL);
    emit_const_i64(&bc, 1, 3);
    emit_mul_i64(&bc, 2, 0, 1);
    emit_const_i64(&bc, 3, 1000000000LL);
    emit_add_i64(&bc, 4, 2, 3);
    emit_return(&bc, 4);
    bc_run(ctx, &bc, 5);
    check_i64("add_i64 large", ctx->result.i64, 4000000000LL);
}

static void test_sub_i64(VMContext* ctx) {
    Bytecode bc;
    bc_init(&bc);
    emit_const_i64(&bc, 0, 9223372036854775807LL);
    emit_const_i64(&bc, 1, 1);
    emit_sub_i64(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(ctx, &bc, 3);
    check_i64("sub_i64 INT64_MAX-1", ctx->result.i64, 9223372036854775806LL);
}

static void test_mul_i64(VMContext* ctx) {
    Bytecode bc;
    bc_init(&bc);
    emit_const_i64(&bc, 0, 1000000LL);
    emit_const_i64(&bc, 1, 1000000LL);
    emit_mul_i64(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(ctx, &bc, 3);
    check_i64("mul_i64 1M*1M", ctx->result.i64, 1000000000000LL);
}

static void test_div_i64(VMContext* ctx) {
    Bytecode bc;
    bc_init(&bc);
    emit_const_i64(&bc, 0, 1000000000000LL);
    emit_const_i64(&bc, 1, 1000LL);
    emit_div_i64(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(ctx, &bc, 3);
    check_i64("div_i64 1T/1K", ctx->result.i64, 1000000000LL);
}

static void test_rem_i64(VMContext* ctx) {
    Bytecode bc;
    bc_init(&bc);
    emit_const_i64(&bc, 0, 1000000007LL);
    emit_const_i64(&bc, 1, 1000000LL);
    emit_rem_i64(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(ctx, &bc, 3);
    check_i64("rem_i64", ctx->result.i64, 7LL);
}

static void test_neg_i64(VMContext* ctx) {
    Bytecode bc;
    bc_init(&bc);
    emit_const_i64(&bc, 0, 1234567890123LL);
    emit_neg_i64(&bc, 1, 0);
    emit_return(&bc, 1);
    bc_run(ctx, &bc, 2);
    check_i64("neg_i64", ctx->result.i64, -1234567890123LL);
}

static void test_chain(VMContext* ctx) {
    Bytecode bc;
    bc_init(&bc);
    emit_const_i32(&bc, 0, 10);
    emit_const_i32(&bc, 1, 2);
    emit_add_i32(&bc, 2, 0, 1); // 12
    emit_const_i32(&bc, 3, 3);
    emit_mul_i32(&bc, 4, 2, 3); // 36
    emit_const_i32(&bc, 5, 4);
    emit_sub_i32(&bc, 6, 4, 5); // 32
    emit_return(&bc, 6);
    bc_run(ctx, &bc, 7);
    check_i32("chain (10+2)*3-4", ctx->result.i32, 32);
}

int main(void) {
    TEST_SECTION("arithmetic.c");
    VMContext ctx;
    vm_init(&ctx);
    
    test_add_i32(&ctx);
    test_sub_i32(&ctx);
    test_mul_i32(&ctx);
    test_div_i32(&ctx);
    test_rem_i32(&ctx);
    test_neg_i32(&ctx);
    
    test_add_i64(&ctx);
    test_sub_i64(&ctx);
    test_mul_i64(&ctx);
    test_div_i64(&ctx);
    test_rem_i64(&ctx);
    test_neg_i64(&ctx);
    
    test_chain(&ctx);
    
    print_summary();
    return 0;
}
