#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "../vm.h"
#include "../vm_builder.h"

static void test_cmp_i32_less(void) {
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    emit_const_i32(&bc, 0, 3);
    emit_const_i32(&bc, 1, 5);
    emit_cmp_i32(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(&ctx, &bc, 3);
    check_i32("cmp_i32_less: 3 vs 5", ctx.result.i32, -1);
}

static void test_cmp_i32_equal(void) {
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    emit_const_i32(&bc, 0, 7);
    emit_const_i32(&bc, 1, 7);
    emit_cmp_i32(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(&ctx, &bc, 3);
    check_i32("cmp_i32_equal: 7 vs 7", ctx.result.i32, 0);
}

static void test_cmp_i32_greater(void) {
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    emit_const_i32(&bc, 0, 10);
    emit_const_i32(&bc, 1, 2);
    emit_cmp_i32(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(&ctx, &bc, 3);
    check_i32("cmp_i32_greater: 10 vs 2", ctx.result.i32, 1);
}

static void test_cmp_i32_negative(void) {
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    emit_const_i32(&bc, 0, -5);
    emit_const_i32(&bc, 1, -3);
    emit_cmp_i32(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(&ctx, &bc, 3);
    check_i32("cmp_i32_negative: -5 vs -3", ctx.result.i32, -1);
}

static void test_cmp_i32_zero(void) {
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    emit_const_i32(&bc, 0, 0);
    emit_const_i32(&bc, 1, 0);
    emit_cmp_i32(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(&ctx, &bc, 3);
    check_i32("cmp_i32_zero: 0 vs 0", ctx.result.i32, 0);

    bc_init(&bc);
    emit_const_i32(&bc, 0, 0);
    emit_const_i32(&bc, 1, 1);
    emit_cmp_i32(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(&ctx, &bc, 3);
    check_i32("cmp_i32_zero: 0 vs 1", ctx.result.i32, -1);

    bc_init(&bc);
    emit_const_i32(&bc, 0, 1);
    emit_const_i32(&bc, 1, 0);
    emit_cmp_i32(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(&ctx, &bc, 3);
    check_i32("cmp_i32_zero: 1 vs 0", ctx.result.i32, 1);
}

static void test_cmp_i64_less(void) {
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    emit_const_i64(&bc, 0, 100LL);
    emit_const_i64(&bc, 1, 200LL);
    emit_cmp_i64(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(&ctx, &bc, 3);
    check_i32("cmp_i64_less: 100LL vs 200LL", ctx.result.i32, -1);
}

static void test_cmp_i64_equal(void) {
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    emit_const_i64(&bc, 0, INT64_MAX);
    emit_const_i64(&bc, 1, INT64_MAX);
    emit_cmp_i64(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(&ctx, &bc, 3);
    check_i32("cmp_i64_equal: MAX vs MAX", ctx.result.i32, 0);
}

static void test_cmp_i64_greater(void) {
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    emit_const_i64(&bc, 0, -1LL);
    emit_const_i64(&bc, 1, INT64_MIN);
    emit_cmp_i64(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(&ctx, &bc, 3);
    check_i32("cmp_i64_greater: -1LL vs MIN", ctx.result.i32, 1);
}

static void test_cmp_f32_less(void) {
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    emit_const_f32(&bc, 0, 1.0f);
    emit_const_f32(&bc, 1, 2.0f);
    emit_cmp_f32(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(&ctx, &bc, 3);
    check_i32("cmp_f32_less: 1.0 vs 2.0", ctx.result.i32, -1);
}

static void test_cmp_f32_equal(void) {
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    emit_const_f32(&bc, 0, 3.14f);
    emit_const_f32(&bc, 1, 3.14f);
    emit_cmp_f32(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(&ctx, &bc, 3);
    check_i32("cmp_f32_equal: 3.14 vs 3.14", ctx.result.i32, 0);
}

static void test_cmp_f32_greater(void) {
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    emit_const_f32(&bc, 0, 5.0f);
    emit_const_f32(&bc, 1, 2.5f);
    emit_cmp_f32(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(&ctx, &bc, 3);
    check_i32("cmp_f32_greater: 5.0 vs 2.5", ctx.result.i32, 1);
}

static void test_cmp_f32_nan(void) {
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    emit_const_f32(&bc, 0, NAN);
    emit_const_f32(&bc, 1, 1.0f);
    emit_cmp_f32(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(&ctx, &bc, 3);
    check_i32("cmp_f32_nan: NaN vs 1.0", ctx.result.i32, -1);
}

static void test_cmp_f32_inf(void) {
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    emit_const_f32(&bc, 0, INFINITY);
    emit_const_f32(&bc, 1, 1.0f);
    emit_cmp_f32(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(&ctx, &bc, 3);
    check_i32("cmp_f32_inf: INF vs 1.0", ctx.result.i32, 1);

    bc_init(&bc);
    emit_const_f32(&bc, 0, -INFINITY);
    emit_const_f32(&bc, 1, 1.0f);
    emit_cmp_f32(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(&ctx, &bc, 3);
    check_i32("cmp_f32_inf: -INF vs 1.0", ctx.result.i32, -1);
}

static void test_cmp_f64_less(void) {
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    emit_const_f64(&bc, 0, 1.0);
    emit_const_f64(&bc, 1, 2.0);
    emit_cmp_f64(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(&ctx, &bc, 3);
    check_i32("cmp_f64_less: 1.0 vs 2.0", ctx.result.i32, -1);
}

static void test_cmp_f64_equal(void) {
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    emit_const_f64(&bc, 0, 3.14);
    emit_const_f64(&bc, 1, 3.14);
    emit_cmp_f64(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(&ctx, &bc, 3);
    check_i32("cmp_f64_equal: 3.14 vs 3.14", ctx.result.i32, 0);
}

static void test_cmp_f64_greater(void) {
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    emit_const_f64(&bc, 0, 5.0);
    emit_const_f64(&bc, 1, 2.5);
    emit_cmp_f64(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(&ctx, &bc, 3);
    check_i32("cmp_f64_greater: 5.0 vs 2.5", ctx.result.i32, 1);
}

static void test_cmp_f64_nan(void) {
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    emit_const_f64(&bc, 0, NAN);
    emit_const_f64(&bc, 1, 1.0);
    emit_cmp_f64(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(&ctx, &bc, 3);
    check_i32("cmp_f64_nan: NaN vs 1.0", ctx.result.i32, -1);
}

static void test_cmp_f64_inf(void) {
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    emit_const_f64(&bc, 0, INFINITY);
    emit_const_f64(&bc, 1, 1.0);
    emit_cmp_f64(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(&ctx, &bc, 3);
    check_i32("cmp_f64_inf: INF vs 1.0", ctx.result.i32, 1);

    bc_init(&bc);
    emit_const_f64(&bc, 0, -INFINITY);
    emit_const_f64(&bc, 1, 1.0);
    emit_cmp_f64(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(&ctx, &bc, 3);
    check_i32("cmp_f64_inf: -INF vs 1.0", ctx.result.i32, -1);
}


int main(void) {
    TEST_SECTION("Comparisons I32");
    test_cmp_i32_less();
    test_cmp_i32_equal();
    test_cmp_i32_greater();
    test_cmp_i32_negative();
    test_cmp_i32_zero();

    TEST_SECTION("Comparisons I64");
    test_cmp_i64_less();
    test_cmp_i64_equal();
    test_cmp_i64_greater();

    TEST_SECTION("Comparisons F32");
    test_cmp_f32_less();
    test_cmp_f32_equal();
    test_cmp_f32_greater();
    test_cmp_f32_nan();
    test_cmp_f32_inf();

    TEST_SECTION("Comparisons F64");
    test_cmp_f64_less();
    test_cmp_f64_equal();
    test_cmp_f64_greater();
    test_cmp_f64_nan();
    test_cmp_f64_inf();

    print_summary();
    return 0;
}
