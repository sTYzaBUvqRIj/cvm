/*
 * Copyright (C) 2026 CVM Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * hash_algorithms.c — tests for DJB2, FNV-1a (32-bit), and FNV-1a (64-bit) Hashing
 */

#include "../vm.h"
#include "../vm_builder.h"

#include <stdio.h>
#include <string.h>

/* =========================================================================
 * Host Reference C Hash Implementations
 * ========================================================================= */

static uint32_t djb2_hash_host(const char* str)
{
    uint32_t hash = 5381;
    while (*str) {
        hash = ((hash << 5) + hash) + (uint8_t)(*str);
        str++;
    }
    return hash;
}

static uint32_t fnv1a32_hash_host(const char* str)
{
    uint32_t hash = 2166136261u;
    while (*str) {
        hash ^= (uint8_t)(*str);
        hash *= 16777619u;
        str++;
    }
    return hash;
}

static uint64_t fnv1a64_hash_host(const char* str)
{
    uint64_t hash = 14695981039346656037ULL;
    while (*str) {
        hash ^= (uint8_t)(*str);
        hash *= 1099511628211ULL;
        str++;
    }
    return hash;
}

/* =========================================================================
 * Test 1: DJB2 Hash in VM Bytecode
 * ========================================================================= */
static void test_djb2_hash(void)
{
    printf("\n--- test_djb2_hash ---\n");

    const char text[] = "Hello, world!";
    const uint32_t expected = djb2_hash_host(text);

    VMContext ctx;
    vm_init(&ctx);

    Bytecode bc;
    bc_init(&bc);

    /*
     * Registers:
     * r0 = ptr (char*)
     * r1 = hash = 5381
     * r2 = byte (*ptr)
     * r3 = 5 (shift amount)
     * r4 = temp (hash << 5)
     * r5 = 1 (ptr step)
     * r6 = 0 (loop terminate byte)
     */
    emit_const_i32(&bc, 1, 5381);
    emit_const_i32(&bc, 3, 5);
    emit_const_i32(&bc, 5, 1);
    emit_const_i32(&bc, 6, 0);

    uint32_t loop_head = bc.size;
    emit_load8    (&bc, 2, 0);               /* r2 = *ptr */
    uint32_t p_done = emit_if_eq(&bc, 2, 6);  /* if *ptr == 0 goto done */

    emit_shl_i32  (&bc, 4, 1, 3);            /* r4 = hash << 5 */
    emit_add_i32  (&bc, 1, 4, 1);            /* r1 = (hash << 5) + hash */
    emit_add_i32  (&bc, 1, 1, 2);            /* r1 = ((hash << 5) + hash) + byte */

    emit_lea      (&bc, 0, 0, 1);            /* ptr++ */
    emit_goto_back(&bc, loop_head);

    bc_patch_here(&bc, p_done);
    emit_return   (&bc, 1);                  /* return hash */

    VMRegister regs[7];
    memset(regs, 0, sizeof(regs));
    regs[0].ptr = (void*)text;

    VMError err = bc_run_regs(&ctx, &bc, regs, 7);
    check_err("djb2 hash err == VM_OK", err, VM_OK);
    check_u32("djb2 hash matches host C", ctx.result.u32, expected);
    vm_cleanup(&ctx);
}

/* =========================================================================
 * Test 2: FNV-1a 32-bit Hash in VM Bytecode
 * ========================================================================= */
static void test_fnv1a32_hash(void)
{
    printf("\n--- test_fnv1a32_hash ---\n");

    const char text[] = "CVM_Register_VM";
    const uint32_t expected = fnv1a32_hash_host(text);

    VMContext ctx;
    vm_init(&ctx);

    Bytecode bc;
    bc_init(&bc);

    /*
     * Registers:
     * r0 = ptr (char*)
     * r1 = hash = 2166136261u (0x811C9DC5)
     * r2 = prime = 16777619u (0x01000193)
     * r3 = byte (*ptr)
     * r4 = 0 (terminate byte)
     */
    emit_const_i32(&bc, 1, (int32_t)2166136261u);
    emit_const_i32(&bc, 2, (int32_t)16777619u);
    emit_const_i32(&bc, 4, 0);

    uint32_t loop_head = bc.size;
    emit_load8    (&bc, 3, 0);               /* r3 = *ptr */
    uint32_t p_done = emit_if_eq(&bc, 3, 4);  /* if *ptr == 0 goto done */

    emit_xor_i32  (&bc, 1, 1, 3);            /* hash ^= byte */
    emit_mul_i32  (&bc, 1, 1, 2);            /* hash *= FNV_PRIME_32 */

    emit_lea      (&bc, 0, 0, 1);            /* ptr++ */
    emit_goto_back(&bc, loop_head);

    bc_patch_here(&bc, p_done);
    emit_return   (&bc, 1);                  /* return hash */

    VMRegister regs[5];
    memset(regs, 0, sizeof(regs));
    regs[0].ptr = (void*)text;

    VMError err = bc_run_regs(&ctx, &bc, regs, 5);
    check_err("fnv1a32 hash err == VM_OK", err, VM_OK);
    check_u32("fnv1a32 hash matches host C", ctx.result.u32, expected);
    vm_cleanup(&ctx);
}

/* =========================================================================
 * Test 3: FNV-1a 64-bit Hash in VM Bytecode
 * ========================================================================= */
static void test_fnv1a64_hash(void)
{
    printf("\n--- test_fnv1a64_hash ---\n");

    const char text[] = "High_Performance_64Bit_Hashing";
    const uint64_t expected = fnv1a64_hash_host(text);

    VMContext ctx;
    vm_init(&ctx);

    Bytecode bc;
    bc_init(&bc);

    /*
     * Registers:
     * r0 = ptr (char*)
     * r1 = hash = 14695981039346656037ULL (0xCBF29CE484222325ULL)
     * r2 = prime = 1099511628211ULL (0x100000001B3ULL)
     * r3 = byte (*ptr)
     * r4 = 0 (terminate byte)
     */
    emit_const_i64(&bc, 1, (int64_t)14695981039346656037ULL);
    emit_const_i64(&bc, 2, (int64_t)1099511628211ULL);
    emit_const_i32(&bc, 4, 0);

    uint32_t loop_head = bc.size;
    emit_load8    (&bc, 3, 0);               /* r3 = *ptr (zero-extended) */
    uint32_t p_done = emit_if_eq(&bc, 3, 4);  /* if *ptr == 0 goto done */

    emit_xor_i64  (&bc, 1, 1, 3);            /* hash ^= byte */
    emit_mul_i64  (&bc, 1, 1, 2);            /* hash *= FNV_PRIME_64 */

    emit_lea      (&bc, 0, 0, 1);            /* ptr++ */
    emit_goto_back(&bc, loop_head);

    bc_patch_here(&bc, p_done);
    emit_return   (&bc, 1);                  /* return hash */

    VMRegister regs[5];
    memset(regs, 0, sizeof(regs));
    regs[0].ptr = (void*)text;

    VMError err = bc_run_regs(&ctx, &bc, regs, 5);
    check_err("fnv1a64 hash err == VM_OK", err, VM_OK);
    check_u64("fnv1a64 hash matches host C", ctx.result.u64, expected);
    vm_cleanup(&ctx);
}

int main(void)
{
    printf("=== DJB2, FNV-1a 32-bit & FNV-1a 64-bit Hash Tests ===\n");
    test_djb2_hash();
    test_fnv1a32_hash();
    test_fnv1a64_hash();
    print_summary();
    return g_fail > 0 ? 1 : 0;
}
