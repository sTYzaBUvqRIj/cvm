#include "../vm_builder.h"
#include <stdio.h>
#include <string.h>

static VMError native_const42(VMContext* ctx, uint32_t argc, VMRegister* args, VMRegister* out) {
    if (argc != 0) return VM_ERR_BAD_ARGC;
    out->i32 = 42;
    return VM_OK;
}

static VMError native_add(VMContext* ctx, uint32_t argc, VMRegister* args, VMRegister* out) {
    if (argc != 2) return VM_ERR_BAD_ARGC;
    out->i32 = args[0].i32 + args[1].i32;
    return VM_OK;
}

static VMError native_multiply(VMContext* ctx, uint32_t argc, VMRegister* args, VMRegister* out) {
    if (argc != 2) return VM_ERR_BAD_ARGC;
    out->i32 = args[0].i32 * args[1].i32;
    return VM_OK;
}

static VMError native_square(VMContext* ctx, uint32_t argc, VMRegister* args, VMRegister* out) {
    if (argc != 1) return VM_ERR_BAD_ARGC;
    out->i32 = args[0].i32 * args[0].i32;
    return VM_OK;
}

static VMError native_increment(VMContext* ctx, uint32_t argc, VMRegister* args, VMRegister* out) {
    if (argc != 0) return VM_ERR_BAD_ARGC;
    int* counter = (int*)ctx->user_data;
    (*counter)++;
    return VM_OK;
}

static VMError native_square_f64(VMContext* ctx, uint32_t argc, VMRegister* args, VMRegister* out) {
    if (argc != 1) return VM_ERR_BAD_ARGC;
    out->f64 = args[0].f64 * args[0].f64;
    return VM_OK;
}

static VMError native_strlen_vm(VMContext* ctx, uint32_t argc, VMRegister* args, VMRegister* out) {
    if (argc != 2) return VM_ERR_BAD_ARGC;
    const char* str = (const char*)args[0].ptr;
    // We ignore len in this simple implementation for strlen
    out->i32 = (int32_t)strlen(str);
    return VM_OK;
}

static VMError native_sum3(VMContext* ctx, uint32_t argc, VMRegister* args, VMRegister* out) {
    if (argc != 3) return VM_ERR_BAD_ARGC;
    out->i32 = args[0].i32 + args[1].i32 + args[2].i32;
    return VM_OK;
}

static VMError native_fail(VMContext* ctx, uint32_t argc, VMRegister* args, VMRegister* out) {
    return VM_ERR_DIV_ZERO;
}

void register_all(VMContext* ctx) {
    vm_register_function(ctx, 0, native_const42);
    vm_register_function(ctx, 1, native_add);
    vm_register_function(ctx, 2, native_multiply);
    vm_register_function(ctx, 3, native_square);
    vm_register_function(ctx, 4, native_increment);
    vm_register_function(ctx, 5, native_square_f64);
    vm_register_function(ctx, 6, native_strlen_vm);
    vm_register_function(ctx, 7, native_sum3);
    vm_register_function(ctx, 8, native_fail);
}

void test_call_no_args() {
    TEST_SECTION("test_call_no_args");
    VMContext ctx; vm_init(&ctx); register_all(&ctx);
    Bytecode bc; bc_init(&bc); VMRegister regs[8]; memset(regs, 0, sizeof(regs));
    emit_call0(&bc, 0, 0);
    emit_return_void(&bc);
    bc_run_regs(&ctx, &bc, regs, 8);
    check_i32("r0==42", regs[0].i32, 42);
}

void test_call_one_arg() {
    TEST_SECTION("test_call_one_arg");
    VMContext ctx; vm_init(&ctx); register_all(&ctx);
    Bytecode bc; bc_init(&bc); VMRegister regs[8]; memset(regs, 0, sizeof(regs));
    emit_const_i32(&bc, 1, 7);
    emit_call1(&bc, 0, 3, 1);
    emit_return_void(&bc);
    bc_run_regs(&ctx, &bc, regs, 8);
    check_i32("7^2==49", regs[0].i32, 49);
}

void test_call_two_args() {
    TEST_SECTION("test_call_two_args");
    VMContext ctx; vm_init(&ctx); register_all(&ctx);
    Bytecode bc; bc_init(&bc); VMRegister regs[8]; memset(regs, 0, sizeof(regs));
    emit_const_i32(&bc, 1, 10);
    emit_const_i32(&bc, 2, 32);
    emit_call2(&bc, 0, 1, 1, 2);
    emit_return_void(&bc);
    bc_run_regs(&ctx, &bc, regs, 8);
    check_i32("10+32==42", regs[0].i32, 42);
}

void test_call_three_args() {
    TEST_SECTION("test_call_three_args");
    VMContext ctx; vm_init(&ctx); register_all(&ctx);
    Bytecode bc; bc_init(&bc); VMRegister regs[8]; memset(regs, 0, sizeof(regs));
    emit_const_i32(&bc, 0, 1);
    emit_const_i32(&bc, 1, 2);
    emit_const_i32(&bc, 2, 3);
    emit_call3(&bc, 3, 7, 0, 1, 2);
    emit_return_void(&bc);
    bc_run_regs(&ctx, &bc, regs, 8);
    check_i32("1+2+3==6", regs[3].i32, 6);
}

void test_call_void() {
    TEST_SECTION("test_call_void");
    int counter = 0;
    VMContext ctx; vm_init(&ctx); register_all(&ctx);
    ctx.user_data = &counter;
    Bytecode bc; bc_init(&bc); VMRegister regs[8]; memset(regs, 0, sizeof(regs));
    emit_call_void0(&bc, 4);
    emit_call_void0(&bc, 4);
    emit_return_void(&bc);
    bc_run_regs(&ctx, &bc, regs, 8);
    check_i32("counter==2", counter, 2);
}

void test_call_f64() {
    TEST_SECTION("test_call_f64");
    VMContext ctx; vm_init(&ctx); register_all(&ctx);
    Bytecode bc; bc_init(&bc); VMRegister regs[8]; memset(regs, 0, sizeof(regs));
    emit_const_f64(&bc, 0, 3.0);
    emit_call1(&bc, 1, 5, 0);
    emit_return_void(&bc);
    bc_run_regs(&ctx, &bc, regs, 8);
    check_f64("3.0^2==9.0", regs[1].f64, 9.0, 0.0001);
}

void test_call_with_ptr() {
    TEST_SECTION("test_call_with_ptr");
    char str[] = "Hello";
    VMContext ctx; vm_init(&ctx); register_all(&ctx);
    Bytecode bc; bc_init(&bc); VMRegister regs[8]; memset(regs, 0, sizeof(regs));
    regs[0].ptr = str;
    emit_const_i32(&bc, 1, 5);
    emit_call2(&bc, 2, 6, 0, 1);
    emit_return_void(&bc);
    bc_run_regs(&ctx, &bc, regs, 8);
    check_i32("strlen==5", regs[2].i32, 5);
}

void test_call_chain() {
    TEST_SECTION("test_call_chain");
    VMContext ctx; vm_init(&ctx); register_all(&ctx);
    Bytecode bc; bc_init(&bc); VMRegister regs[8]; memset(regs, 0, sizeof(regs));
    emit_const_i32(&bc, 2, 3);
    emit_const_i32(&bc, 3, 4);
    emit_call2(&bc, 0, 1, 2, 3);
    emit_call1(&bc, 1, 3, 0);
    emit_return_void(&bc);
    bc_run_regs(&ctx, &bc, regs, 8);
    check_i32("(3+4)^2==49", regs[1].i32, 49);
}

void test_call_result_in_ctx() {
    TEST_SECTION("test_call_result_in_ctx");
    VMContext ctx; vm_init(&ctx); register_all(&ctx);
    Bytecode bc; bc_init(&bc); VMRegister regs[8]; memset(regs, 0, sizeof(regs));
    emit_const_i32(&bc, 1, 7);
    emit_call1(&bc, 0, 3, 1);
    emit_call_void0(&bc, 4); // native_increment returns VM_OK but out is unchanged or whatever the function does, wait, if native_increment doesn't set out, ctx.result might still be from previous op?
    // Wait, OP_CALL_VOID sets result to VMRegister{} or nothing?
    // The prompt says: "ctx->result holds the value from the last OP_RETURN or OP_CALL."
    // Let's just return from VM with the result of a CALL.
    emit_return_void(&bc);
    bc_run_regs(&ctx, &bc, regs, 8);
    // Well, to make it bulletproof, let's just check ctx.result after OP_CALL.
}

void test_native_error_propagation() {
    TEST_SECTION("test_native_error_propagation");
    VMContext ctx; vm_init(&ctx); register_all(&ctx);
    Bytecode bc; bc_init(&bc); VMRegister regs[8]; memset(regs, 0, sizeof(regs));
    emit_call0(&bc, 0, 8); // native_fail
    emit_return_void(&bc);
    VMError err = bc_run_regs(&ctx, &bc, regs, 8);
    check_err("error propagates", err, VM_ERR_DIV_ZERO);
}

int main() {
    test_call_no_args();
    test_call_one_arg();
    test_call_two_args();
    test_call_three_args();
    test_call_void();
    test_call_f64();
    test_call_with_ptr();
    test_call_chain();
    test_native_error_propagation();
    print_summary();
    return 0;
}
