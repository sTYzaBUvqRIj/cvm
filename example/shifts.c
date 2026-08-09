/*
 * shifts.c — Shift operations (SHL / SHR / USHR) for i32 and i64
 *
 * SHR  = arithmetic (sign-preserving) right shift
 * USHR = logical    (zero-filling)    right shift
 *
 * Shift amount is taken from a register:
 *   i32 shifts: amount masked to low 5 bits (& 31)
 *   i64 shifts: amount masked to low 6 bits (& 63)
 */

#include "../vm_builder.h"

/* =========================================================================
 * Helpers: run a single two-register shift and return result
 * ====================================================================== */

static int32_t run_shl_i32(VMContext* ctx, int32_t val, int32_t amt)
{
    Bytecode bc; bc_init(&bc);
    emit_const_i32(&bc, 0, val);
    emit_const_i32(&bc, 1, amt);
    emit_shl_i32(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(ctx, &bc, 3);
    return ctx->result.i32;
}

static int32_t run_shr_i32(VMContext* ctx, int32_t val, int32_t amt)
{
    Bytecode bc; bc_init(&bc);
    emit_const_i32(&bc, 0, val);
    emit_const_i32(&bc, 1, amt);
    emit_shr_i32(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(ctx, &bc, 3);
    return ctx->result.i32;
}

static uint32_t run_ushr_i32(VMContext* ctx, int32_t val, int32_t amt)
{
    Bytecode bc; bc_init(&bc);
    emit_const_i32(&bc, 0, val);
    emit_const_i32(&bc, 1, amt);
    emit_ushr_i32(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(ctx, &bc, 3);
    return ctx->result.u32;
}

static int64_t run_shl_i64(VMContext* ctx, int64_t val, int32_t amt)
{
    Bytecode bc; bc_init(&bc);
    emit_const_i64(&bc, 0, val);
    emit_const_i32(&bc, 1, amt);
    emit_shl_i64(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(ctx, &bc, 3);
    return ctx->result.i64;
}

static int64_t run_shr_i64(VMContext* ctx, int64_t val, int32_t amt)
{
    Bytecode bc; bc_init(&bc);
    emit_const_i64(&bc, 0, val);
    emit_const_i32(&bc, 1, amt);
    emit_shr_i64(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(ctx, &bc, 3);
    return ctx->result.i64;
}

static uint64_t run_ushr_i64(VMContext* ctx, int64_t val, int32_t amt)
{
    Bytecode bc; bc_init(&bc);
    emit_const_i64(&bc, 0, val);
    emit_const_i32(&bc, 1, amt);
    emit_ushr_i64(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(ctx, &bc, 3);
    return ctx->result.u64;
}

/* =========================================================================
 * Tests
 * ====================================================================== */

static void test_shl_i32(void)
{
    VMContext ctx; vm_init(&ctx);
    /* 1 << 4 = 16 */
    check_i32("shl_i32: 1<<4", run_shl_i32(&ctx, 1, 4), 16);
    /* 1 << 0 = 1 */
    check_i32("shl_i32: 1<<0", run_shl_i32(&ctx, 1, 0), 1);
    /* 0xFF << 8 = 0xFF00 */
    check_i32("shl_i32: 0xFF<<8", run_shl_i32(&ctx, 0xFF, 8), 0xFF00);
    /* 1 << 31 = INT32_MIN (wraps as signed i32) */
    check_u32("shl_i32: 1<<31", (uint32_t)run_shl_i32(&ctx, 1, 31), 0x80000000u);
}

static void test_shr_i32(void)
{
    VMContext ctx; vm_init(&ctx);
    /* arithmetic right shift: sign preserved */
    check_i32("shr_i32: 256>>3",  run_shr_i32(&ctx, 256, 3),  32);
    check_i32("shr_i32: -8>>1",   run_shr_i32(&ctx, -8,  1),  -4);
    check_i32("shr_i32: -1>>31",  run_shr_i32(&ctx, -1,  31), -1);
    check_i32("shr_i32: 128>>4",  run_shr_i32(&ctx, 128, 4),  8);
    /* shifting 0 is always 0 */
    check_i32("shr_i32: 0>>7",    run_shr_i32(&ctx, 0, 7), 0);
}

static void test_ushr_i32(void)
{
    VMContext ctx; vm_init(&ctx);
    /* logical: high bits filled with 0, not sign */
    /* (uint32_t)(-1) >> 1 = 0x7FFFFFFF */
    check_u32("ushr_i32: -1>>1",   run_ushr_i32(&ctx, -1,  1),  0x7FFFFFFFu);
    /* 256 >> 4 = 16 */
    check_u32("ushr_i32: 256>>4",  run_ushr_i32(&ctx, 256, 4),  16u);
    /* 0x80000000 >> 1 = 0x40000000 (NOT sign-extended) */
    check_u32("ushr_i32: min>>1",  run_ushr_i32(&ctx, (int32_t)0x80000000u, 1), 0x40000000u);
    /* shift by 0: unchanged */
    check_u32("ushr_i32: >>0",     run_ushr_i32(&ctx, -256, 0), 0xFFFFFF00u);
}

static void test_shl_i64(void)
{
    VMContext ctx; vm_init(&ctx);
    /* 1LL << 32 = 4294967296LL */
    check_i64("shl_i64: 1<<32", run_shl_i64(&ctx, 1LL, 32), 4294967296LL);
    /* 1LL << 0 = 1LL */
    check_i64("shl_i64: 1<<0",  run_shl_i64(&ctx, 1LL, 0),  1LL);
    /* 1LL << 63 = INT64_MIN */
    check_i64("shl_i64: 1<<63", run_shl_i64(&ctx, 1LL, 63), (int64_t)(1ULL << 63));
}

static void test_shr_i64(void)
{
    VMContext ctx; vm_init(&ctx);
    /* arithmetic: sign bit propagates */
    check_i64("shr_i64: -8>>1",              run_shr_i64(&ctx, -8LL, 1),  -4LL);
    check_i64("shr_i64: INT64_MAX>>62",
              run_shr_i64(&ctx, (int64_t)0x7FFFFFFFFFFFFFFFLL, 62), 1LL);
    check_i64("shr_i64: -1>>63",             run_shr_i64(&ctx, -1LL, 63), -1LL);
    check_i64("shr_i64: 1024>>5",            run_shr_i64(&ctx, 1024LL, 5), 32LL);
}

static void test_ushr_i64(void)
{
    VMContext ctx; vm_init(&ctx);
    /* -1LL (all bits set) >> 1 = 0x7FFFFFFFFFFFFFFF */
    check_u64("ushr_i64: -1LL>>1",
              run_ushr_i64(&ctx, -1LL, 1), 0x7FFFFFFFFFFFFFFFULL);
    /* Only high 32 bits set, shift right 32 = 0xFFFFFFFF */
    check_u64("ushr_i64: -1LL>>32",
              run_ushr_i64(&ctx, -1LL, 32), 0x00000000FFFFFFFFull);
    check_u64("ushr_i64: >>0",
              run_ushr_i64(&ctx, -1LL, 0), 0xFFFFFFFFFFFFFFFFull);
}

static void test_shift_zero(void)
{
    VMContext ctx; vm_init(&ctx);
    /* shifting by 0 must return the original value unchanged */
    const int32_t v = (int32_t)0xDEADBEEFu;
    check_u32("shift_zero shl",  (uint32_t)run_shl_i32(&ctx, v, 0),  (uint32_t)v);
    check_u32("shift_zero shr",  (uint32_t)run_shr_i32(&ctx, v, 0),  (uint32_t)v);
    check_u32("shift_zero ushr", run_ushr_i32(&ctx, v, 0), (uint32_t)v);
}

static void test_shift_semantics(void)
{
    VMContext ctx; vm_init(&ctx);
    /* -256 = 0xFFFFFF00
     * SHR  (arithmetic) >> 4: keeps sign: 0xFFFFFFF0 = -16
     * USHR (logical)    >> 4: zero-fill:  0x0FFFFFF0            */
    check_i32("shift_semantics shr",
              run_shr_i32(&ctx, -256, 4), -16);
    check_u32("shift_semantics ushr",
              run_ushr_i32(&ctx, -256, 4), 0x0FFFFFF0u);
}

static void test_multiply_by_power2(void)
{
    VMContext ctx; vm_init(&ctx);
    /* 15 * 8 via SHL by 3 */
    check_i32("multiply_power2: 15<<3", run_shl_i32(&ctx, 15, 3), 15 * 8);
    /* 7 * 4 via SHL by 2 */
    check_i32("multiply_power2: 7<<2",  run_shl_i32(&ctx, 7,  2), 7 * 4);
}

static void test_divide_by_power2(void)
{
    VMContext ctx; vm_init(&ctx);
    /* 120 / 4 via USHR by 2 (120 is positive, USHR = DIV for positive) */
    check_u32("divide_power2: 120>>2", run_ushr_i32(&ctx, 120, 2), 30u);
    /* 1024 / 8 via USHR by 3 */
    check_u32("divide_power2: 1024>>3", run_ushr_i32(&ctx, 1024, 3), 128u);
}

/* =========================================================================
 * main
 * ====================================================================== */

int main(void)
{
    printf("=== Shift Operations ===\n");

    TEST_SECTION("Shifts I32 — SHL");
    test_shl_i32();

    TEST_SECTION("Shifts I32 — SHR (arithmetic)");
    test_shr_i32();

    TEST_SECTION("Shifts I32 — USHR (logical)");
    test_ushr_i32();

    TEST_SECTION("Shifts I64 — SHL");
    test_shl_i64();

    TEST_SECTION("Shifts I64 — SHR (arithmetic)");
    test_shr_i64();

    TEST_SECTION("Shifts I64 — USHR (logical)");
    test_ushr_i64();

    TEST_SECTION("Shift Properties");
    test_shift_zero();
    test_shift_semantics();
    test_multiply_by_power2();
    test_divide_by_power2();

    print_summary();
    return g_fail > 0 ? 1 : 0;
}
