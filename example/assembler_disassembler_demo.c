/*
 * Copyright (C) 2026 CVM Contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * assembler_disassembler_demo.c — Functional tests & demonstration for
 *                                  vm_assembler.h & vm_disassembler.h
 */

#include "../vm.h"
#include "../vm_assembler.h"
#include "../vm_disassembler.h"
#include "../vm_loader.h"
#include "../vm_builder.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void test_basic_assembly(void)
{
    TEST_SECTION("Basic Assembly & Execution");

    const char* asm_source =
        "# Simple addition and multiplication demo\n"
        "CONST_I32 r0, 10\n"
        "CONST_I32 r1, 20\n"
        "ADD_I32   r2, r0, r1\n"   /* r2 = 30 */
        "CONST_I32 r3, 2\n"
        "MUL_I32   r4, r2, r3\n"   /* r4 = 60 */
        "RETURN    r4\n";

    VMAssembleResult res = vm_assemble(asm_source);
    check_bool("vm_assemble success", res.success);

    if (res.success) {
        VMContext ctx;
        vm_init(&ctx);
        VMRegister regs[16];
        memset(regs, 0, sizeof(regs));

        VMError err = vm_execute(&ctx, regs, 16, 0, res.bytecode, (uint32_t)res.size);
        check_err("vm_execute status", err, VM_OK);
        check_i32("Result calculation (10+20)*2", ctx.result.i32, 60);

        /* Disassemble and print */
        char* disasm = vm_disassemble(res.bytecode, res.size);
        check_bool("vm_disassemble string non-null", disasm != NULL);
        if (disasm) {
            printf("  Disassembly output:\n%s", disasm);
            free(disasm);
        }
    }
    vm_assemble_free(&res);
}

static void test_loop_assembly(void)
{
    TEST_SECTION("Loop with Labels Assembly");

    const char* asm_source =
        "# Calculate sum of 1..100 = 5050\n"
        "CONST_I32 r0, 1\n"       /* i = 1 */
        "CONST_I32 r1, 100\n"     /* limit = 100 */
        "CONST_I32 r2, 0\n"       /* sum = 0 */
        "CONST_I32 r3, 1\n"       /* step = 1 */
        "loop_start:\n"
        "ADD_I32   r2, r2, r0\n"   /* sum += i */
        "ADD_I32   r0, r0, r3\n"   /* i++ */
        "IF_LE     r0, r1, loop_start\n" /* if i <= 100 goto loop_start */
        "RETURN    r2\n";

    VMAssembleResult res = vm_assemble(asm_source);
    check_bool("vm_assemble loop with label success", res.success);

    if (res.success) {
        VMContext ctx;
        vm_init(&ctx);
        VMRegister regs[16];
        memset(regs, 0, sizeof(regs));

        VMError err = vm_execute(&ctx, regs, 16, 0, res.bytecode, (uint32_t)res.size);
        check_err("vm_execute loop", err, VM_OK);
        check_i32("Sum 1..100 result", ctx.result.i32, 5050);
    }
    vm_assemble_free(&res);
}

static void test_disasm_reasm_roundtrip(void)
{
    TEST_SECTION("Disassembler <-> Assembler Round-Trip Verification");

    /* Build a program with various instruction types using vm_builder.h */
    Bytecode bc;
    bc_init(&bc);
    emit_const_i32(&bc, 0, 42);
    emit_const_f64(&bc, 1, 3.14159265);
    emit_add_i32(&bc, 2, 0, 0);
    emit_sub_i32(&bc, 3, 2, 0);
    emit_mul_i32(&bc, 4, 3, 0);
    emit_i32_to_f64(&bc, 5, 4);
    emit_return(&bc, 4);

    /* 1. Disassemble bytecode to string */
    char* disasm_str = vm_disassemble(bc.data, bc.size);
    check_bool("Disassembly generated", disasm_str != NULL);

    if (disasm_str) {
        /* 2. Re-assemble disassembled string back to bytecode */
        VMAssembleResult reasm = vm_assemble(disasm_str);
        check_bool("Re-assembly success", reasm.success);

        if (reasm.success) {
            check_i32("Bytecode size match", (int32_t)reasm.size, (int32_t)bc.size);
            check_bool("Bytecode content exact match",
                       memcmp(bc.data, reasm.bytecode, bc.size) == 0);
        }
        vm_assemble_free(&reasm);
        free(disasm_str);
    }
}

int main(void)
{
    printf("=== CVM Assembler & Disassembler Suite ===\n");
    test_basic_assembly();
    test_loop_assembly();
    test_disasm_reasm_roundtrip();
    print_summary();
    return g_fail > 0 ? 1 : 0;
}
