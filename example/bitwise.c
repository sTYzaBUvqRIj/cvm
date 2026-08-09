#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "../vm.h"
#include "../vm_builder.h"


static void test_and_i32(void) {
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    emit_const_i32(&bc, 0, 0xFF00);
    emit_const_i32(&bc, 1, 0x0FF0);
    emit_and_i32(&bc, 2, 0, 1);
    
    emit_const_i32(&bc, 3, 0xFFFFFFFF);
    emit_const_i32(&bc, 4, 0);
    emit_and_i32(&bc, 5, 3, 4);

    emit_const_i32(&bc, 6, 0xABCD);
    emit_const_i32(&bc, 7, 0xFF);
    emit_and_i32(&bc, 8, 6, 7);

    emit_add_i32(&bc, 9, 2, 5);
    emit_add_i32(&bc, 9, 9, 8);
    emit_return(&bc, 9);
    bc_run(&ctx, &bc, 10);
    check_i32("and_i32", ctx.result.i32, 0x0F00 + 0 + 0xCD);
}

static void test_or_i32(void) {
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    emit_const_i32(&bc, 0, 0x00FF);
    emit_const_i32(&bc, 1, 0xFF00);
    emit_or_i32(&bc, 2, 0, 1);

    emit_const_i32(&bc, 3, 0);
    emit_const_i32(&bc, 4, 8); // bit 3
    emit_or_i32(&bc, 5, 3, 4);

    emit_add_i32(&bc, 6, 2, 5);
    emit_return(&bc, 6);
    bc_run(&ctx, &bc, 7);
    check_u32("or_i32", (uint32_t)ctx.result.i32, 0xFFFF + 8);
}

static void test_xor_i32(void) {
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    emit_const_i32(&bc, 0, 0xFF);
    emit_const_i32(&bc, 1, 0x0F);
    emit_xor_i32(&bc, 2, 0, 1);
    
    emit_xor_i32(&bc, 3, 2, 1); // double xor restores

    emit_add_i32(&bc, 4, 2, 3);
    emit_return(&bc, 4);
    bc_run(&ctx, &bc, 5);
    check_i32("xor_i32", ctx.result.i32, 0xF0 + 0xFF);
}

static void test_not_i32(void) {
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    emit_const_i32(&bc, 0, 0);
    emit_not_i32(&bc, 1, 0);

    emit_const_i32(&bc, 2, -1);
    emit_not_i32(&bc, 3, 2);

    emit_const_i32(&bc, 4, 0x0F0F0F0F);
    emit_not_i32(&bc, 5, 4);
    
    emit_add_i32(&bc, 6, 1, 3);
    emit_return(&bc, 6);
    bc_run(&ctx, &bc, 7);
    check_i32("not_i32", ctx.result.i32, -1 + 0);
    
    bc_init(&bc);
    emit_const_i32(&bc, 0, 0x0F0F0F0F);
    emit_not_i32(&bc, 1, 0);
    emit_return(&bc, 1);
    bc_run(&ctx, &bc, 2);
    check_u32("not_i32 mask", (uint32_t)ctx.result.i32, 0xF0F0F0F0);
}

static void test_and_i64(void) {
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    emit_const_i64(&bc, 0, 0xFFFF000000000000LL);
    emit_const_i64(&bc, 1, 0xDEADBEEFCAFEBABELL);
    emit_and_i64(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(&ctx, &bc, 3);
    check_u64("and_i64", (uint64_t)ctx.result.i64, 0xDEAD000000000000ULL);
}

static void test_or_i64(void) {
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    emit_const_i64(&bc, 0, 0x00000000CAFEBABELL);
    emit_const_i64(&bc, 1, 0xDEADBEEF00000000LL);
    emit_or_i64(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(&ctx, &bc, 3);
    check_u64("or_i64", (uint64_t)ctx.result.i64, 0xDEADBEEFCAFEBABELL);
}

static void test_xor_i64(void) {
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    emit_const_i64(&bc, 0, 0xFFFFFFFFFFFFFFFFLL);
    emit_const_i64(&bc, 1, 0x00000000FFFFFFFFLL);
    emit_xor_i64(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    bc_run(&ctx, &bc, 3);
    check_u64("xor_i64", (uint64_t)ctx.result.i64, 0xFFFFFFFF00000000ULL);
}

static void test_not_i64(void) {
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    emit_const_i64(&bc, 0, 0LL);
    emit_not_i64(&bc, 1, 0);
    emit_return(&bc, 1);
    bc_run(&ctx, &bc, 2);
    check_i64("not_i64", ctx.result.i64, -1LL);
}

static void test_bitmask_permissions(void) {
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    // rwxrwxrwx = 0777 octal = 511
    emit_const_i32(&bc, 0, 511);
    emit_const_i32(&bc, 1, 0700); // octal 0700 = 448
    emit_and_i32(&bc, 2, 0, 1); // owner perms
    emit_const_i32(&bc, 3, 0400); // octal 0400 = 256
    emit_and_i32(&bc, 4, 2, 3); // check read
    emit_return(&bc, 4);
    bc_run(&ctx, &bc, 5);
    check_i32("bitmask_permissions", ctx.result.i32, 256);
}

static void test_bit_extraction(void) {
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    emit_const_i32(&bc, 0, 0xABCD);
    emit_const_i32(&bc, 1, 4);
    emit_shr_i32(&bc, 2, 0, 1); // ABCD >> 4 = ABC
    emit_const_i32(&bc, 3, 0xF);
    emit_and_i32(&bc, 4, 2, 3); // ABC & F = C
    emit_return(&bc, 4);
    bc_run(&ctx, &bc, 5);
    check_i32("bit_extraction", ctx.result.i32, 0xC);
}

int main(void) {
    TEST_SECTION("Bitwise I32");
    test_and_i32();
    test_or_i32();
    test_xor_i32();
    test_not_i32();

    TEST_SECTION("Bitwise I64");
    test_and_i64();
    test_or_i64();
    test_xor_i64();
    test_not_i64();

    TEST_SECTION("Bitwise Applications");
    test_bitmask_permissions();
    test_bit_extraction();

    print_summary();
    return 0;
}
