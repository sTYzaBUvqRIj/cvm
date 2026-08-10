/*
 * Copyright (C) 2026 CVM Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * debugger_profiler.c — tests for Debugger Hooks and Profiler
 */

#include "../vm.h"
#include "../vm_builder.h"

#include <stdio.h>
#include <string.h>

static int g_step_events = 0;
static int g_breakpoint_events = 0;
static int g_error_events = 0;
static uint32_t g_last_breakpoint_pc = 0;

static void debug_hook_callback(VMContext* ctx, VMDebugEvent event, uint32_t pc, uint16_t opcode)
{
    (void)ctx;
    (void)opcode;
    switch (event) {
    case VM_DEBUG_EVENT_STEP:
        g_step_events++;
        break;
    case VM_DEBUG_EVENT_BREAKPOINT:
        g_breakpoint_events++;
        g_last_breakpoint_pc = pc;
        break;
    case VM_DEBUG_EVENT_ERROR:
        g_error_events++;
        break;
    }
}

static void test_debugger_hooks(void)
{
    printf("\n--- test_debugger_hooks ---\n");

    VMContext ctx;
    vm_init(&ctx);
    ctx.debug_hook = debug_hook_callback;

    Bytecode bc;
    bc_init(&bc);

    /*
     * 0: CONST_I32 r0, 10
     * 7: CONST_I32 r1, 20
     * 14: ADD_I32   r2, r0, r1
     * 19: RETURN    r2
     */
    emit_const_i32(&bc, 0, 10);
    emit_const_i32(&bc, 1, 20);
    emit_add_i32  (&bc, 2, 0, 1);
    emit_return   (&bc, 2);

    /* Set breakpoint at pc = 14 (ADD_I32) */
    uint32_t bps[] = { 14 };
    ctx.breakpoints = bps;
    ctx.breakpoint_count = 1;

    g_step_events = 0;
    g_breakpoint_events = 0;

    VMRegister regs[4];
    memset(regs, 0, sizeof(regs));

    /* Run 1: Should stop at breakpoint pc = 14 */
    VMError err = vm_execute(&ctx, regs, 4, 0, bc.data, bc.size);
    check_err("breakpoint run 1 err == VM_OK", err, VM_OK);
    check_u32("breakpoint event count == 1", g_breakpoint_events, 1);
    check_u32("last breakpoint pc == 14", g_last_breakpoint_pc, 14);
    check_u32("paused flag set", (ctx.flags & VM_FLAG_PAUSED) != 0, 1);

    /* Step past breakpoint: single step once (executes ADD_I32 at pc = 14) */
    ctx.flags |= VM_FLAG_SINGLE_STEP;
    err = vm_execute(&ctx, regs, 4, ctx.pc, bc.data, bc.size);
    check_err("step past breakpoint err == VM_OK", err, VM_OK);
    check_i32("r2 computed", regs[2].i32, 30);

    /* Continue to completion */
    err = vm_execute(&ctx, regs, 4, ctx.pc, bc.data, bc.size);
    check_err("run to completion err == VM_OK", err, VM_OK);
    check_i32("final result == 30", ctx.result.i32, 30);
    check_u32("total step events > 0", g_step_events > 0, 1);
}

static void test_profiler_counters(void)
{
    printf("\n--- test_profiler_counters ---\n");

    VMContext ctx;
    vm_init(&ctx);
    ctx.profiler_enabled = 1;

    Bytecode bc;
    bc_init(&bc);

    /*
     * Loop 100 times:
     * r0 = count = 100
     * r1 = i = 0
     * r2 = sum = 0
     * r3 = step = 1
     * Loop:
     *   sum += i
     *   i += 1
     *   if i < count goto Loop
     */
    emit_const_i32(&bc, 0, 100);
    emit_const_i32(&bc, 1, 0);
    emit_const_i32(&bc, 2, 0);
    emit_const_i32(&bc, 3, 1);

    uint32_t loop_start = bc.size;
    emit_add_i32(&bc, 2, 2, 1);
    emit_add_i32(&bc, 1, 1, 3);
    uint32_t patch = emit_if_lt(&bc, 1, 0);
    bc_patch_back(&bc, patch, loop_start);
    emit_return(&bc, 2);

    VMRegister regs[4];
    memset(regs, 0, sizeof(regs));

    VMError err = bc_run_regs(&ctx, &bc, regs, 4);
    check_err("profiler run err == VM_OK", err, VM_OK);
    check_i32("sum 0..99 == 4950", ctx.result.i32, 4950);

    /*
     * Opcode count verification:
     * CONST_I32: 4
     * ADD_I32: 200 (2 per iteration * 100)
     * IF_LT: 100
     * RETURN: 1
     * Total: 305 instructions
     */
    check_u32("total_instructions == 305", (uint32_t)ctx.total_instructions, 305);
    check_u32("opcode_counts[OP_ADD_I32] == 200", (uint32_t)ctx.opcode_counts[OP_ADD_I32], 200);
    check_u32("opcode_counts[OP_IF_LT] == 100", (uint32_t)ctx.opcode_counts[OP_IF_LT], 100);
    check_u32("opcode_counts[OP_CONST_I32] == 4", (uint32_t)ctx.opcode_counts[OP_CONST_I32], 4);
    check_u32("opcode_counts[OP_RETURN] == 1", (uint32_t)ctx.opcode_counts[OP_RETURN], 1);

    /* Dump profiler report to stdout for visual verification */
    vm_profiler_dump(&ctx, stdout);
}

int main(void)
{
    printf("=== Debugger & Profiler Tests ===\n");
    test_debugger_hooks();
    test_profiler_counters();
    print_summary();
    return g_fail > 0 ? 1 : 0;
}
