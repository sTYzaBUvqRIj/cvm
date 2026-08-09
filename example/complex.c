#include "../vm_builder.h"
#include <stdio.h>
#include <stdint.h>

void test_calculator() {
    VMContext ctx;
    vm_init(&ctx);
    Bytecode bc;
    bc_init(&bc);
    // ((10+5)*4-8)/9
    emit_const_i32(&bc, 0, 10);
    emit_const_i32(&bc, 1, 5);
    emit_const_i32(&bc, 2, 4);
    emit_const_i32(&bc, 3, 8);
    emit_const_i32(&bc, 4, 9);
    
    emit_add_i32(&bc, 5, 0, 1);
    emit_mul_i32(&bc, 6, 5, 2);
    emit_sub_i32(&bc, 7, 6, 3);
    emit_div_i32(&bc, 8, 7, 4);
    
    emit_return(&bc, 8);
    VMError err = bc_run(&ctx, &bc, 9);
    check_err("test_calculator", err, VM_OK);
    check_i32("calculator ((10+5)*4-8)/9", ctx.result.i32, 5);
}

VMError bc_run_is_prime(int32_t n, int32_t* is_prime) {
    VMContext ctx;
    vm_init(&ctx);
    Bytecode bc;
    bc_init(&bc);
    
    emit_const_i32(&bc, 0, n); // n
    emit_const_i32(&bc, 1, 2); // 2
    
    uint32_t p1 = emit_if_ge(&bc, 0, 1); // if n >= 2 skip return 0
    emit_const_i32(&bc, 2, 0);
    emit_return(&bc, 2);
    bc_patch_here(&bc, p1);
    
    uint32_t p2 = emit_if_ne(&bc, 0, 1); // if n != 2 skip return 1
    emit_const_i32(&bc, 2, 1);
    emit_return(&bc, 2);
    bc_patch_here(&bc, p2);
    
    emit_const_i32(&bc, 2, 2); // i = 2
    emit_const_i32(&bc, 3, 0); // n%i
    emit_const_i32(&bc, 4, 1); // 1
    
    uint32_t loop_top = bc.size;
    emit_mul_i32(&bc, 5, 2, 2); // i*i
    uint32_t p3 = emit_if_gt(&bc, 5, 0); // if i*i > n exit loop
    
    emit_rem_i32(&bc, 3, 0, 2); // n%i
    uint32_t p4 = emit_if_nez(&bc, 3); // if n%i != 0 skip return 0
    emit_const_i32(&bc, 6, 0);
    emit_return(&bc, 6);
    bc_patch_here(&bc, p4);
    
    emit_add_i32(&bc, 2, 2, 4); // i++
    emit_goto_back(&bc, loop_top);
    
    bc_patch_here(&bc, p3);
    emit_const_i32(&bc, 6, 1);
    emit_return(&bc, 6);
    
    VMError err = bc_run(&ctx, &bc, 7);
    *is_prime = ctx.result.i32;
    return err;
}

void test_prime_check() {
    int32_t res;
    bc_run_is_prime(7, &res); check_i32("is_prime(7)", res, 1);
    bc_run_is_prime(9, &res); check_i32("is_prime(9)", res, 0);
    bc_run_is_prime(13, &res); check_i32("is_prime(13)", res, 1);
    bc_run_is_prime(1, &res); check_i32("is_prime(1)", res, 0);
    bc_run_is_prime(2, &res); check_i32("is_prime(2)", res, 1);
}

void test_statistics() {
    VMContext ctx;
    vm_init(&ctx);
    int32_t data[6] = {3, 1, 4, 1, 5, 9};
    VMRegister regs[10] = {0};
    regs[0].ptr = data;
    
    Bytecode bc;
    bc_init(&bc);
    // compute sum, min, max
    emit_const_i32(&bc, 1, 6); // n
    emit_const_i32(&bc, 2, 0); // i
    emit_const_i32(&bc, 3, 0); // sum
    emit_load32s(&bc, 4, 0);   // min = data[0]
    emit_move(&bc, 5, 4);      // max = data[0]
    emit_const_i32(&bc, 6, 1); // step 1
    emit_const_i32(&bc, 7, 4); // bytes step
    
    uint32_t loop_top = bc.size;
    uint32_t p = emit_if_ge(&bc, 2, 1);
    
    emit_load32s(&bc, 8, 0); // val = *ptr
    emit_add_i32(&bc, 3, 3, 8); // sum += val
    
    // min
    uint32_t p_min = emit_if_ge(&bc, 8, 4); // if val >= min skip
    emit_move(&bc, 4, 8);
    bc_patch_here(&bc, p_min);
    
    // max
    uint32_t p_max = emit_if_le(&bc, 8, 5); // if val <= max skip
    emit_move(&bc, 5, 8);
    bc_patch_here(&bc, p_max);
    
    emit_add_i32(&bc, 2, 2, 6); // i++
    emit_lea(&bc, 0, 0, 4);     // ptr += 4 bytes (sizeof int32_t)
    emit_goto_back(&bc, loop_top);
    bc_patch_here(&bc, p);
    
    // we want to return min, max, sum? only one return. Just check registers.
    emit_return_void(&bc);
    
    VMError err = bc_run_regs(&ctx, &bc, regs, 10);
    check_err("test_statistics", err, VM_OK);
    check_i32("sum", regs[3].i32, 23);
    check_i32("min", regs[4].i32, 1);
    check_i32("max", regs[5].i32, 9);
}

void test_bubble_sort() {
    VMContext ctx;
    vm_init(&ctx);
    int32_t arr[5] = {5, 3, 1, 4, 2};
    VMRegister regs[12] = {0};
    regs[0].ptr = arr;
    
    Bytecode bc;
    bc_init(&bc);
    emit_const_i32(&bc, 1, 5); // n
    emit_const_i32(&bc, 2, 0); // i
    emit_const_i32(&bc, 3, 1); // step 1
    emit_const_i32(&bc, 4, 4); // bytes step 4
    
    uint32_t outer_loop = bc.size;
    emit_sub_i32(&bc, 5, 1, 3); // n-1
    uint32_t p_outer = emit_if_ge(&bc, 2, 5); // if i >= n-1 exit
    
    emit_const_i32(&bc, 6, 0); // j=0
    emit_sub_i32(&bc, 7, 5, 2); // limit = n-1-i
    
    emit_move(&bc, 10, 0); // ptr for inner loop
    
    uint32_t inner_loop = bc.size;
    uint32_t p_inner = emit_if_ge(&bc, 6, 7); // if j >= limit exit
    
    emit_load32s(&bc, 8, 10); // arr[j]
    emit_lea(&bc, 11, 10, 4); // ptr2 = ptr + 4
    emit_load32s(&bc, 9, 11); // arr[j+1]
    
    uint32_t p_swap = emit_if_le(&bc, 8, 9); // if arr[j] <= arr[j+1] skip swap
    emit_store32(&bc, 10, 9);
    emit_store32(&bc, 11, 8);
    bc_patch_here(&bc, p_swap);
    
    emit_add_i32(&bc, 6, 6, 3); // j++
    emit_lea(&bc, 10, 10, 4); // ptr += 4
    emit_goto_back(&bc, inner_loop);
    
    bc_patch_here(&bc, p_inner);
    
    emit_add_i32(&bc, 2, 2, 3); // i++
    emit_goto_back(&bc, outer_loop);
    
    bc_patch_here(&bc, p_outer);
    emit_return_void(&bc);
    
    VMError err = bc_run_regs(&ctx, &bc, regs, 12);
    check_err("test_bubble_sort", err, VM_OK);
    check_i32("arr[0]", arr[0], 1);
    check_i32("arr[1]", arr[1], 2);
    check_i32("arr[2]", arr[2], 3);
    check_i32("arr[3]", arr[3], 4);
    check_i32("arr[4]", arr[4], 5);
}

void test_matrix2x2_add() {
    VMContext ctx;
    vm_init(&ctx);
    int32_t A[4] = {1, 2, 3, 4};
    int32_t B[4] = {5, 6, 7, 8};
    int32_t C[4] = {0, 0, 0, 0};
    VMRegister regs[10];
    memset(regs, 0, sizeof(regs));
    regs[0].ptr = A;
    regs[1].ptr = B;
    regs[2].ptr = C;
    
    Bytecode bc;
    bc_init(&bc);
    emit_const_i32(&bc, 3, 0); // i
    emit_const_i32(&bc, 4, 4); // n
    emit_const_i32(&bc, 5, 1); // step 1
    emit_const_i32(&bc, 6, 4); // bytes step 4
    
    uint32_t loop_top = bc.size;
    uint32_t p = emit_if_ge(&bc, 3, 4);
    
    emit_load32s(&bc, 7, 0); // valA
    emit_load32s(&bc, 8, 1); // valB
    emit_add_i32(&bc, 7, 7, 8); // sum
    emit_store32(&bc, 2, 7); // store C
    
    emit_add_i32(&bc, 3, 3, 5); // i++
    emit_lea(&bc, 0, 0, 4);     /* A_ptr += 4 */
    emit_lea(&bc, 1, 1, 4);     /* B_ptr += 4 */
    emit_lea(&bc, 2, 2, 4);     /* C_ptr += 4 */
    emit_goto_back(&bc, loop_top);
    
    bc_patch_here(&bc, p);
    emit_return_void(&bc);
    
    VMError err = bc_run_regs(&ctx, &bc, regs, 10);
    check_err("test_matrix2x2_add", err, VM_OK);
    check_i32("C[0]", C[0], 6);
    check_i32("C[1]", C[1], 8);
    check_i32("C[2]", C[2], 10);
    check_i32("C[3]", C[3], 12);
}

void test_string_length() {
    VMContext ctx;
    vm_init(&ctx);
    char str[] = "Hello VM";
    VMRegister regs[5] = {0};
    regs[0].ptr = str;
    
    Bytecode bc;
    bc_init(&bc);
    emit_const_i32(&bc, 1, 0); // count
    emit_const_i32(&bc, 2, 1); // step 1
    
    uint32_t loop_top = bc.size;
    emit_load8s(&bc, 3, 0); // char
    uint32_t p = emit_if_eqz(&bc, 3); // if char == 0 exit
    
    emit_add_i32(&bc, 1, 1, 2); /* count++ */
    emit_lea(&bc, 0, 0, 1);     /* ptr += 1 (char) */
    emit_goto_back(&bc, loop_top);
    
    bc_patch_here(&bc, p);
    emit_return(&bc, 1);
    
    VMError err = bc_run_regs(&ctx, &bc, regs, 5);
    check_err("test_string_length", err, VM_OK);
    check_i32("strlen", ctx.result.i32, 8);
}

static void run_abs(VMContext* ctx, int32_t val, int32_t exp, const char* label)
{
    Bytecode bc;
    bc_init(&bc);
    emit_const_i32(&bc, 0, val);
    uint32_t p = emit_if_gez(&bc, 0); /* if val >= 0 skip neg */
    emit_neg_i32(&bc, 0, 0);
    bc_patch_here(&bc, p);
    emit_return(&bc, 0);
    bc_run(ctx, &bc, 1);
    check_i32(label, ctx->result.i32, exp);
}

static void test_absolute_value(void) {
    VMContext ctx;
    vm_init(&ctx);
    run_abs(&ctx, -42, 42,  "abs(-42)");
    run_abs(&ctx,  42, 42,  "abs(42)");
    run_abs(&ctx,   0,  0,  "abs(0)");
}

static void run_min(VMContext* ctx, int32_t a, int32_t b, int32_t exp, const char* label)
{
    Bytecode bc;
    bc_init(&bc);
    emit_const_i32(&bc, 0, a);
    emit_const_i32(&bc, 1, b);
    uint32_t p = emit_if_le(&bc, 0, 1); /* if a <= b, keep a */
    emit_move(&bc, 0, 1);               /* else a = b */
    bc_patch_here(&bc, p);
    emit_return(&bc, 0);
    bc_run(ctx, &bc, 2);
    check_i32(label, ctx->result.i32, exp);
}

static void test_min_of_two(void) {
    VMContext ctx;
    vm_init(&ctx);
    run_min(&ctx,  5, 10,  5, "min(5,10)");
    run_min(&ctx, 10,  5,  5, "min(10,5)");
    run_min(&ctx, -1, -5, -5, "min(-1,-5)");
}

static void run_clamp(VMContext* ctx, int32_t val, int32_t lo, int32_t hi,
                      int32_t exp, const char* label)
{
    Bytecode bc;
    bc_init(&bc);
    emit_const_i32(&bc, 0, val);
    emit_const_i32(&bc, 1, lo);
    emit_const_i32(&bc, 2, hi);
    /* if val >= lo, skip lo-clamp */
    uint32_t p1 = emit_if_ge(&bc, 0, 1);
    emit_move(&bc, 0, 1);
    bc_patch_here(&bc, p1);
    /* if val <= hi, skip hi-clamp */
    uint32_t p2 = emit_if_le(&bc, 0, 2);
    emit_move(&bc, 0, 2);
    bc_patch_here(&bc, p2);
    emit_return(&bc, 0);
    bc_run(ctx, &bc, 3);
    check_i32(label, ctx->result.i32, exp);
}

static void test_clamp(void) {
    VMContext ctx;
    vm_init(&ctx);
    run_clamp(&ctx, 15,  0, 10, 10, "clamp(15,0,10)");
    run_clamp(&ctx, -5,  0, 10,  0, "clamp(-5,0,10)");
    run_clamp(&ctx,  7,  0, 10,  7, "clamp(7,0,10)");
}

int main(void) {
    TEST_SECTION("Complex");
    test_calculator();
    test_prime_check();
    test_statistics();
    test_bubble_sort();
    test_matrix2x2_add();
    test_string_length();
    test_absolute_value();
    test_min_of_two();
    test_clamp();
    print_summary();
    return g_fail > 0 ? 1 : 0;
}
