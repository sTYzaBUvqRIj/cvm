#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "../vm.h"
#include "../vm_builder.h"

#define DECLARE_BRANCH_TEST(NAME, OP_FN, VAL1, VAL2, EXPECT_TAKEN) \
static void test_##NAME##_taken(void) { \
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc); \
    emit_const_i32(&bc, 0, VAL1); \
    emit_const_i32(&bc, 1, VAL2); \
    emit_const_i32(&bc, 2, 0); \
    uint32_t skip = OP_FN(&bc, 0, 1); \
    emit_const_i32(&bc, 2, 0); \
    uint32_t end = emit_goto_16_fwd(&bc); \
    bc_patch_here(&bc, skip); \
    emit_const_i32(&bc, 2, 1); \
    bc_patch_here(&bc, end); \
    emit_return(&bc, 2); \
    bc_run(&ctx, &bc, 3); \
    check_i32(#NAME " taken", ctx.result.i32, 1); \
} \
static void test_##NAME##_not_taken(void) { \
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc); \
    emit_const_i32(&bc, 0, VAL2); \
    emit_const_i32(&bc, 1, VAL1); \
    emit_const_i32(&bc, 2, 0); \
    uint32_t skip = OP_FN(&bc, 0, 1); \
    emit_const_i32(&bc, 2, 1); \
    uint32_t end = emit_goto_16_fwd(&bc); \
    bc_patch_here(&bc, skip); \
    emit_const_i32(&bc, 2, 0); \
    bc_patch_here(&bc, end); \
    emit_return(&bc, 2); \
    bc_run(&ctx, &bc, 3); \
    check_i32(#NAME " not taken", ctx.result.i32, 1); \
}

#define DECLARE_BRANCH_Z_TEST(NAME, OP_FN, VAL_TAKEN, VAL_NOT_TAKEN) \
static void test_##NAME##_taken(void) { \
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc); \
    emit_const_i32(&bc, 0, VAL_TAKEN); \
    emit_const_i32(&bc, 2, 0); \
    uint32_t skip = OP_FN(&bc, 0); \
    emit_const_i32(&bc, 2, 0); \
    uint32_t end = emit_goto_16_fwd(&bc); \
    bc_patch_here(&bc, skip); \
    emit_const_i32(&bc, 2, 1); \
    bc_patch_here(&bc, end); \
    emit_return(&bc, 2); \
    bc_run(&ctx, &bc, 3); \
    check_i32(#NAME " taken", ctx.result.i32, 1); \
} \
static void test_##NAME##_not_taken(void) { \
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc); \
    emit_const_i32(&bc, 0, VAL_NOT_TAKEN); \
    emit_const_i32(&bc, 2, 0); \
    uint32_t skip = OP_FN(&bc, 0); \
    emit_const_i32(&bc, 2, 1); \
    uint32_t end = emit_goto_16_fwd(&bc); \
    bc_patch_here(&bc, skip); \
    emit_const_i32(&bc, 2, 0); \
    bc_patch_here(&bc, end); \
    emit_return(&bc, 2); \
    bc_run(&ctx, &bc, 3); \
    check_i32(#NAME " not taken", ctx.result.i32, 1); \
}

/* Use the 6-argument macro for all register-pair branch tests. */
#undef DECLARE_BRANCH_TEST
#define DECLARE_BRANCH_TEST(NAME, OP_FN, VAL_TAKEN1, VAL_TAKEN2, VAL_NOT1, VAL_NOT2) \
static void test_##NAME##_taken(void) { \
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc); \
    emit_const_i32(&bc, 0, VAL_TAKEN1); \
    emit_const_i32(&bc, 1, VAL_TAKEN2); \
    emit_const_i32(&bc, 2, 0); \
    uint32_t skip = OP_FN(&bc, 0, 1); \
    emit_const_i32(&bc, 2, 0); \
    uint32_t end = emit_goto_16_fwd(&bc); \
    bc_patch_here(&bc, skip); \
    emit_const_i32(&bc, 2, 1); \
    bc_patch_here(&bc, end); \
    emit_return(&bc, 2); \
    bc_run(&ctx, &bc, 3); \
    check_i32(#NAME " taken", ctx.result.i32, 1); \
} \
static void test_##NAME##_not_taken(void) { \
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc); \
    emit_const_i32(&bc, 0, VAL_NOT1); \
    emit_const_i32(&bc, 1, VAL_NOT2); \
    emit_const_i32(&bc, 2, 0); \
    uint32_t skip = OP_FN(&bc, 0, 1); \
    emit_const_i32(&bc, 2, 1); \
    uint32_t end = emit_goto_16_fwd(&bc); \
    bc_patch_here(&bc, skip); \
    emit_const_i32(&bc, 2, 0); \
    bc_patch_here(&bc, end); \
    emit_return(&bc, 2); \
    bc_run(&ctx, &bc, 3); \
    check_i32(#NAME " not taken", ctx.result.i32, 1); \
}

DECLARE_BRANCH_TEST(if_eq, emit_if_eq, 5, 5, 5, 6)
DECLARE_BRANCH_TEST(if_ne, emit_if_ne, 5, 6, 5, 5)
DECLARE_BRANCH_TEST(if_lt, emit_if_lt, 4, 5, 5, 5)
DECLARE_BRANCH_TEST(if_ge, emit_if_ge, 5, 5, 4, 5)
DECLARE_BRANCH_TEST(if_gt, emit_if_gt, 6, 5, 5, 5)
DECLARE_BRANCH_TEST(if_le, emit_if_le, 5, 5, 6, 5)

DECLARE_BRANCH_Z_TEST(if_eqz, emit_if_eqz, 0, 1)
DECLARE_BRANCH_Z_TEST(if_nez, emit_if_nez, 1, 0)
DECLARE_BRANCH_Z_TEST(if_ltz, emit_if_ltz, -1, 0)
DECLARE_BRANCH_Z_TEST(if_gez, emit_if_gez, 0, -1)
DECLARE_BRANCH_Z_TEST(if_gtz, emit_if_gtz, 1, 0)
DECLARE_BRANCH_Z_TEST(if_lez, emit_if_lez, 0, 1)


static void test_forward_branch(void) {
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    // nested if/else-if
    emit_const_i32(&bc, 0, 1); // selector
    emit_const_i32(&bc, 1, 100);
    emit_const_i32(&bc, 2, 0); // result

    uint32_t skip1 = emit_if_nez(&bc, 0); // if (r0 != 0) skip block 1
    emit_const_i32(&bc, 2, 10);
    uint32_t end1 = emit_goto_16_fwd(&bc);
    bc_patch_here(&bc, skip1);

    uint32_t skip2 = emit_if_nez(&bc, 1); // if (r1 != 0) skip block 2
    emit_const_i32(&bc, 2, 20);
    uint32_t end2 = emit_goto_16_fwd(&bc);
    bc_patch_here(&bc, skip2);

    emit_const_i32(&bc, 2, 30); // else
    bc_patch_here(&bc, end1);
    bc_patch_here(&bc, end2);

    emit_return(&bc, 2);
    bc_run(&ctx, &bc, 3);
    check_i32("forward_branch", ctx.result.i32, 30);
}

static void test_backward_branch(void) {
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    emit_const_i32(&bc, 0, 5); // count
    emit_const_i32(&bc, 1, 0); // sum
    emit_const_i32(&bc, 2, 1); // one
    
    uint32_t loop_top = bc.size;
    emit_add_i32(&bc, 1, 1, 0); // sum += count
    emit_sub_i32(&bc, 0, 0, 2); // count--
    
    uint32_t p = emit_if_nez(&bc, 0);
    bc_patch_back(&bc, p, loop_top);
    emit_return(&bc, 1);
    bc_run(&ctx, &bc, 3);
    check_i32("backward_branch", ctx.result.i32, 15);
}

static void test_goto(void) {
    /* GOTO (i8): skip over a CONST_I32 instruction (7 bytes: 2 op + 1 dst + 4 val).
     * We capture the position of the offset byte, emit GOTO with offset=0,
     * emit the instruction to skip, then patch the offset. */
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    emit_const_i32(&bc, 0, 10);         /* r0 = 10 */
    /* GOTO [i8 offset].  Instruction layout: [op:u16][off:i8] = 3 bytes.
     * after emit: next_pc points past the 3rd byte.
     * We'll patch off = target - next_pc_after_goto. */
    uint32_t goto_op_pos = bc.size;     /* start of GOTO instruction */
    emit_goto(&bc, 0);                  /* placeholder offset */
    /* next_pc = goto_op_pos + 3 (2 op + 1 i8) */
    uint32_t next_pc = goto_op_pos + 3;
    /* CONST_I32: 2+1+4 = 7 bytes.  We want to skip it. */
    emit_const_i32(&bc, 0, 20);         /* this should be skipped */
    uint32_t target = bc.size;          /* land here */
    /* patch: offset = target - next_pc */
    bc.data[goto_op_pos + 2] = (uint8_t)((int8_t)(target - next_pc));
    emit_return(&bc, 0);
    bc_run(&ctx, &bc, 1);
    check_i32("goto_i8", ctx.result.i32, 10); /* 10, not 20 */

    /* GOTO_16: backward jump (a mini-loop that runs once extra deliberately). */
    bc_init(&bc);
    emit_const_i32(&bc, 0, 0);  /* counter */
    emit_const_i32(&bc, 1, 1);  /* one */
    uint32_t loop_t = bc.size;
    emit_add_i32(&bc, 0, 0, 1); /* counter++ */
    /* if counter < 3, loop back */
    emit_const_i32(&bc, 2, 3);
    uint32_t p = emit_if_lt(&bc, 0, 2);
    bc_patch_back(&bc, p, loop_t);
    emit_return(&bc, 0);
    bc_run(&ctx, &bc, 3);
    check_i32("goto_16_backward", ctx.result.i32, 3);
}

static void test_nested_branches(void) {
    VMContext ctx; vm_init(&ctx); Bytecode bc; bc_init(&bc);
    emit_const_i32(&bc, 0, 1); // i
    emit_const_i32(&bc, 1, 10); // limit
    emit_const_i32(&bc, 2, 0); // count of even
    emit_const_i32(&bc, 3, 2); // two
    emit_const_i32(&bc, 4, 1); // one
    emit_const_i32(&bc, 5, 0); // temp

    uint32_t loop_top = bc.size;
    emit_rem_i32(&bc, 5, 0, 3); // temp = i % 2
    uint32_t skip_inc = emit_if_nez(&bc, 5); // if i % 2 != 0, skip
    emit_add_i32(&bc, 2, 2, 4); // count++
    bc_patch_here(&bc, skip_inc);
    
    emit_add_i32(&bc, 0, 0, 4); // i++
    uint32_t p = emit_if_le(&bc, 0, 1); // if i <= 10
    bc_patch_back(&bc, p, loop_top);
    
    emit_return(&bc, 2);
    bc_run(&ctx, &bc, 6);
    check_i32("nested_branches", ctx.result.i32, 5); // 2, 4, 6, 8, 10
}


int main(void) {
    TEST_SECTION("Conditional Branching (Register Pair)");
    test_if_eq_taken(); test_if_eq_not_taken();
    test_if_ne_taken(); test_if_ne_not_taken();
    test_if_lt_taken(); test_if_lt_not_taken();
    test_if_ge_taken(); test_if_ge_not_taken();
    test_if_gt_taken(); test_if_gt_not_taken();
    test_if_le_taken(); test_if_le_not_taken();

    TEST_SECTION("Conditional Branching (Zero)");
    test_if_eqz_taken(); test_if_eqz_not_taken();
    test_if_nez_taken(); test_if_nez_not_taken();
    test_if_ltz_taken(); test_if_ltz_not_taken();
    test_if_gez_taken(); test_if_gez_not_taken();
    test_if_gtz_taken(); test_if_gtz_not_taken();
    test_if_lez_taken(); test_if_lez_not_taken();

    TEST_SECTION("Complex Branches");
    test_forward_branch();
    test_backward_branch();
    test_goto();
    test_nested_branches();

    print_summary();
    return 0;
}
