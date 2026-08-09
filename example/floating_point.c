#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "../vm_builder.h"

static void test_add_f32(VMContext* ctx) {
    Bytecode bc;
    bc_init(&bc);
    emit_const_f32(&bc, 0, 1.5f);
    emit_const_f32(&bc, 1, 2.5f);
    emit_add_f32(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(ctx, &bc, 3);
    check_f32("add_f32 (1.5+2.5)", ctx->result.f32, 4.0f, 1e-5f);

    bc_init(&bc);
    emit_const_f32(&bc, 0, -1.0f);
    emit_const_f32(&bc, 1, 1.0f);
    emit_add_f32(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(ctx, &bc, 3);
    check_f32("add_f32 (-1.0+1.0)", ctx->result.f32, 0.0f, 1e-5f);
}

static void test_sub_f32(VMContext* ctx) {
    Bytecode bc;
    bc_init(&bc);
    emit_const_f32(&bc, 0, 5.0f);
    emit_const_f32(&bc, 1, 3.0f);
    emit_sub_f32(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(ctx, &bc, 3);
    check_f32("sub_f32 (5.0-3.0)", ctx->result.f32, 2.0f, 1e-5f);

    bc_init(&bc);
    emit_const_f32(&bc, 0, 0.5f);
    emit_const_f32(&bc, 1, 1.0f);
    emit_sub_f32(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(ctx, &bc, 3);
    check_f32("sub_f32 (0.5-1.0)", ctx->result.f32, -0.5f, 1e-5f);
}

static void test_mul_f32(VMContext* ctx) {
    Bytecode bc;
    bc_init(&bc);
    emit_const_f32(&bc, 0, 3.0f);
    emit_const_f32(&bc, 1, 4.0f);
    emit_mul_f32(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(ctx, &bc, 3);
    check_f32("mul_f32 (3.0*4.0)", ctx->result.f32, 12.0f, 1e-5f);

    bc_init(&bc);
    emit_const_f32(&bc, 0, -2.0f);
    emit_const_f32(&bc, 1, 3.0f);
    emit_mul_f32(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(ctx, &bc, 3);
    check_f32("mul_f32 (-2.0*3.0)", ctx->result.f32, -6.0f, 1e-5f);
}

static void test_div_f32(VMContext* ctx) {
    Bytecode bc;
    bc_init(&bc);
    emit_const_f32(&bc, 0, 10.0f);
    emit_const_f32(&bc, 1, 4.0f);
    emit_div_f32(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(ctx, &bc, 3);
    check_f32("div_f32 (10.0/4.0)", ctx->result.f32, 2.5f, 1e-5f);

    bc_init(&bc);
    emit_const_f32(&bc, 0, -9.0f);
    emit_const_f32(&bc, 1, 3.0f);
    emit_div_f32(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(ctx, &bc, 3);
    check_f32("div_f32 (-9.0/3.0)", ctx->result.f32, -3.0f, 1e-5f);
}

static void test_neg_f32(VMContext* ctx) {
    Bytecode bc;
    bc_init(&bc);
    emit_const_f32(&bc, 0, 3.14f);
    emit_neg_f32(&bc, 1, 0);
    emit_return(&bc, 1);
    bc_run(ctx, &bc, 2);
    check_f32("neg_f32 (3.14)", ctx->result.f32, -3.14f, 1e-5f);
}

static void test_add_f64(VMContext* ctx) {
    Bytecode bc;
    bc_init(&bc);
    emit_const_f64(&bc, 0, 1.1);
    emit_const_f64(&bc, 1, 2.2);
    emit_add_f64(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(ctx, &bc, 3);
    check_f64("add_f64 (1.1+2.2)", ctx->result.f64, 3.3, 1e-9);
}

static void test_sub_f64(VMContext* ctx) {
    Bytecode bc;
    bc_init(&bc);
    emit_const_f64(&bc, 0, 10.0);
    emit_const_f64(&bc, 1, 3.14159);
    emit_sub_f64(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(ctx, &bc, 3);
    check_f64("sub_f64 (10.0-3.14159)", ctx->result.f64, 6.85841, 1e-9);
}

static void test_mul_f64(VMContext* ctx) {
    Bytecode bc;
    bc_init(&bc);
    emit_const_f64(&bc, 0, 2.0);
    emit_const_f64(&bc, 1, 3.14159);
    emit_mul_f64(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(ctx, &bc, 3);
    check_f64("mul_f64 (2.0*3.14159)", ctx->result.f64, 6.28318, 1e-9);
}

static void test_div_f64(VMContext* ctx) {
    Bytecode bc;
    bc_init(&bc);
    emit_const_f64(&bc, 0, 22.0);
    emit_const_f64(&bc, 1, 7.0);
    emit_div_f64(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(ctx, &bc, 3);
    check_f64("div_f64 (22.0/7.0)", ctx->result.f64, 3.142857142857, 1e-6);
}

static void test_neg_f64(VMContext* ctx) {
    Bytecode bc;
    bc_init(&bc);
    emit_const_f64(&bc, 0, -1.5);
    emit_neg_f64(&bc, 1, 0);
    emit_return(&bc, 1);
    bc_run(ctx, &bc, 2);
    check_f64("neg_f64 (-1.5)", ctx->result.f64, 1.5, 1e-9);
}

static void test_f32_infinity(VMContext* ctx) {
    Bytecode bc;
    bc_init(&bc);
    emit_const_f32(&bc, 0, 1.0f);
    emit_const_f32(&bc, 1, 0.0f);
    emit_div_f32(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(ctx, &bc, 3);
    check_bool("f32_infinity", isinf(ctx->result.f32));
}

static void test_f64_nan_propagation(VMContext* ctx) {
    Bytecode bc;
    bc_init(&bc);
    emit_const_f64(&bc, 0, NAN);
    emit_const_f64(&bc, 1, 1.0);
    emit_add_f64(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(ctx, &bc, 3);
    check_f64_nan("f64_nan_propagation", ctx->result.f64);
}

static void test_f32_chain(VMContext* ctx) {
    Bytecode bc;
    bc_init(&bc);
    emit_const_f32(&bc, 0, 1.5f);
    emit_const_f32(&bc, 1, 2.5f);
    emit_add_f32(&bc, 2, 0, 1); // 4.0f
    
    emit_const_f32(&bc, 3, 4.0f);
    emit_const_f32(&bc, 4, 1.0f);
    emit_sub_f32(&bc, 5, 3, 4); // 3.0f
    
    emit_mul_f32(&bc, 6, 2, 5); // 12.0f
    emit_return(&bc, 6);
    
    bc_run(ctx, &bc, 7);
    check_f32("f32_chain", ctx->result.f32, 12.0f, 1e-5f);
}

int main(void) {
    TEST_SECTION("floating_point.c");
    VMContext ctx;
    vm_init(&ctx);
    
    test_add_f32(&ctx);
    test_sub_f32(&ctx);
    test_mul_f32(&ctx);
    test_div_f32(&ctx);
    test_neg_f32(&ctx);
    
    test_add_f64(&ctx);
    test_sub_f64(&ctx);
    test_mul_f64(&ctx);
    test_div_f64(&ctx);
    test_neg_f64(&ctx);
    
    test_f32_infinity(&ctx);
    test_f64_nan_propagation(&ctx);
    test_f32_chain(&ctx);
    
    print_summary();
    return 0;
}
