#include "../vm_builder.h"
#include <stdio.h>
#include <stdint.h>
#include <math.h>

void test_ok() {
    VMContext ctx;
    vm_init(&ctx);
    Bytecode bc;
    bc_init(&bc);
    emit_const_i32(&bc, 0, 42);
    emit_return(&bc, 0);
    VMError err = bc_run(&ctx, &bc, 1);
    check_err("test_ok", err, VM_OK);
    check_i32("test_ok result", ctx.result.i32, 42);
}

void test_invalid_opcode() {
    VMContext ctx;
    vm_init(&ctx);
    Bytecode bc;
    bc_init(&bc);
    bc_u16(&bc, 0xFFFF);
    VMError err = bc_run(&ctx, &bc, 1);
    check_err("test_invalid_opcode", err, VM_ERR_INVALID_OPCODE);
}

void test_truncated_bytecode() {
    VMContext ctx;
    vm_init(&ctx);
    Bytecode bc;
    bc_init(&bc);
    bc_u16(&bc, 0x0002); // CONST_I32 opcode, just a guess, but size matters
    bc_u8(&bc, 0);
    // don't emit the rest of the bytes
    VMError err = bc_run(&ctx, &bc, 1);
    check_err("test_truncated_bytecode", err, VM_ERR_OUT_OF_BOUNDS);
}

void test_out_of_bounds_branch() {
    VMContext ctx;
    vm_init(&ctx);
    Bytecode bc;
    bc_init(&bc);
    emit_goto_16(&bc, 1000); // Out of bounds
    VMError err = bc_run(&ctx, &bc, 1);
    check_err("test_out_of_bounds_branch", err, VM_ERR_OUT_OF_BOUNDS);
}

void test_negative_branch_oob() {
    VMContext ctx;
    vm_init(&ctx);
    Bytecode bc;
    bc_init(&bc);
    emit_goto_16(&bc, -1000); // Out of bounds negative
    VMError err = bc_run(&ctx, &bc, 1);
    check_err("test_negative_branch_oob", err, VM_ERR_OUT_OF_BOUNDS);
}

void test_div_zero_i32() {
    VMContext ctx;
    vm_init(&ctx);
    Bytecode bc;
    bc_init(&bc);
    emit_const_i32(&bc, 0, 10);
    emit_const_i32(&bc, 1, 0);
    emit_div_i32(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    VMError err = bc_run(&ctx, &bc, 3);
    check_err("test_div_zero_i32", err, VM_ERR_DIV_ZERO);
}

void test_div_zero_i64() {
    VMContext ctx;
    vm_init(&ctx);
    Bytecode bc;
    bc_init(&bc);
    emit_const_i64(&bc, 0, 10);
    emit_const_i64(&bc, 1, 0);
    emit_div_i64(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    VMError err = bc_run(&ctx, &bc, 3);
    check_err("test_div_zero_i64", err, VM_ERR_DIV_ZERO);
}

void test_rem_zero_i32() {
    VMContext ctx;
    vm_init(&ctx);
    Bytecode bc;
    bc_init(&bc);
    emit_const_i32(&bc, 0, 10);
    emit_const_i32(&bc, 1, 0);
    emit_rem_i32(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    VMError err = bc_run(&ctx, &bc, 3);
    check_err("test_rem_zero_i32", err, VM_ERR_DIV_ZERO);
}

void test_invalid_register() {
    VMContext ctx;
    vm_init(&ctx);
    Bytecode bc;
    bc_init(&bc);
    emit_const_i32(&bc, 5, 42); // Reg 5 used
    emit_return(&bc, 5);
    VMError err = bc_run(&ctx, &bc, 2); // Only 2 regs provided
    check_err("test_invalid_register", err, VM_ERR_INVALID_REGISTER);
}

void test_bad_function_id() {
    VMContext ctx;
    vm_init(&ctx);
    Bytecode bc;
    bc_init(&bc);
    emit_call0(&bc, 0, VM_MAX_NATIVE_FUNCS); // out of range id
    emit_return(&bc, 0);
    VMError err = bc_run(&ctx, &bc, 1);
    check_err("test_bad_function_id", err, VM_ERR_BAD_FUNCTION);
}

void test_null_function() {
    VMContext ctx;
    vm_init(&ctx);
    Bytecode bc;
    bc_init(&bc);
    emit_call0(&bc, 0, 10); // ID 10 is not registered
    emit_return(&bc, 0);
    VMError err = bc_run(&ctx, &bc, 1);
    check_err("test_null_function", err, VM_ERR_BAD_FUNCTION);
}

void test_bad_argc() {
    VMContext ctx;
    vm_init(&ctx);
    Bytecode bc;
    bc_init(&bc);
    // encode OP_CALL with argc = VM_MAX_CALL_ARGC + 1
    // opcode for CALL_N might be unknown here, but we will just emit raw bytes assuming 0x0041 is CALL_N or something,
    // actually instructions say "bc_u16(OP_CALL)" but we don't know OP_CALL.
    // Let's use emit_call_n and pass VM_MAX_CALL_ARGC+1
    uint8_t args[100] = {0};
    emit_call_n(&bc, 0, 0, VM_MAX_CALL_ARGC + 1, args);
    VMError err = bc_run(&ctx, &bc, 100);
    check_err("test_bad_argc", err, VM_ERR_BAD_ARGC);
}

void test_float_div_zero_is_ok() {
    VMContext ctx;
    vm_init(&ctx);
    Bytecode bc;
    bc_init(&bc);
    emit_const_f32(&bc, 0, 1.0f);
    emit_const_f32(&bc, 1, 0.0f);
    emit_div_f32(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    VMError err = bc_run(&ctx, &bc, 3);
    check_err("test_float_div_zero_is_ok", err, VM_OK);
    check_bool("test_float_div_zero_is_ok is infinity", isinf(ctx.result.f32));
}

void test_int32_min_div_neg1() {
    VMContext ctx;
    vm_init(&ctx);
    Bytecode bc;
    bc_init(&bc);
    emit_const_i32(&bc, 0, INT32_MIN);
    emit_const_i32(&bc, 1, -1);
    emit_div_i32(&bc, 2, 0, 1);
    emit_return(&bc, 2);
    VMError err = bc_run(&ctx, &bc, 3);
    check_err("test_int32_min_div_neg1", err, VM_OK);
    check_i32("test_int32_min_div_neg1 result", ctx.result.i32, INT32_MIN);
}

int main() {
    TEST_SECTION("Error Handling");
    test_ok();
    test_invalid_opcode();
    test_truncated_bytecode();
    test_out_of_bounds_branch();
    test_negative_branch_oob();
    test_div_zero_i32();
    test_div_zero_i64();
    test_rem_zero_i32();
    test_invalid_register();
    test_bad_function_id();
    test_null_function();
    test_bad_argc();
    test_float_div_zero_is_ok();
    test_int32_min_div_neg1();
    print_summary();
    return 0;
}
