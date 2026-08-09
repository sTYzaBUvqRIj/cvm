/*
 * file_execution.c — Demonstrates the full write-then-read-then-execute cycle
 *
 * This example:
 *   1. Builds several bytecode programs using vm_builder.h
 *   2. Saves each to a .cvmb file with cvmb_write()
 *   3. Reads each file back with cvmb_read()
 *   4. Executes the loaded bytecode
 *   5. Verifies the results are identical
 *
 * Compile:
 *   gcc -std=c99 -Wall -Wno-unused-function -I.. -o file_execution file_execution.c ../vm.c -lm
 */

#include "../vm_builder.h"
#include "../vm_loader.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* =========================================================================
 * Helper: build, write, reload, run, compare
 * ====================================================================== */

typedef struct {
    const char* label;
    const char* path;
    Bytecode    bc;
    uint16_t    reg_count;
    int32_t     expected_i32;
} RoundTrip;

/* Execute a freshly-built Bytecode and return result.i32 */
static int32_t run_direct(VMContext* ctx, Bytecode* bc, uint16_t reg_count)
{
    VMRegister regs[64];
    memset(regs, 0, sizeof(VMRegister) * reg_count);
    vm_execute(ctx, regs, reg_count, 0, bc->data, bc->size);
    return ctx->result.i32;
}

/* Write to file, read back, execute, return result.i32 */
static int32_t run_via_file(VMContext* ctx, const char* path,
                            const Bytecode* bc, uint16_t reg_count)
{
    /* ---- write ---- */
    if (!cvmb_write(path, bc->data, bc->size, reg_count, 0)) {
        fprintf(stderr, "  cvmb_write failed for %s\n", path);
        return -1;
    }

    /* ---- read ---- */
    CVMBHeader hdr;
    uint8_t* code = cvmb_read(path, &hdr);
    if (!code) {
        fprintf(stderr, "  cvmb_read failed for %s\n", path);
        return -1;
    }

    /* ---- run ---- */
    VMRegister regs[64];
    memset(regs, 0, sizeof(VMRegister) * hdr.reg_count);
    vm_execute(ctx, regs, hdr.reg_count, hdr.entry_pc, code, hdr.code_size);
    int32_t result = ctx->result.i32;
    cvmb_free(code);
    return result;
}

/* =========================================================================
 * Programs
 * ====================================================================== */

/* Program 1: constant 42 */
static void build_const42(Bytecode* bc)
{
    bc_init(bc);
    emit_const_i32(bc, 0, 42);
    emit_return(bc, 0);
}

/* Program 2: sum 1..100 = 5050 */
static void build_sum100(Bytecode* bc)
{
    bc_init(bc);
    emit_const_i32(bc, 0, 1);    /* r0 = i = 1 */
    emit_const_i32(bc, 1, 100);  /* r1 = limit = 100 */
    emit_const_i32(bc, 2, 0);    /* r2 = sum = 0 */
    emit_const_i32(bc, 3, 1);    /* r3 = 1 (increment) */

    uint32_t loop = bc->size;
    emit_add_i32(bc, 2, 2, 0);   /* sum += i */
    emit_add_i32(bc, 0, 0, 3);   /* i++ */
    uint32_t p = emit_if_le(bc, 0, 1);
    bc_patch_back(bc, p, loop);

    emit_return(bc, 2);
}

/* Program 3: Fibonacci(15) = 610 */
static void build_fib15(Bytecode* bc)
{
    bc_init(bc);
    emit_const_i32(bc, 0, 15);   /* r0 = n */
    emit_const_i32(bc, 1, 0);    /* r1 = prev = 0 */
    emit_const_i32(bc, 2, 1);    /* r2 = curr = 1 */
    emit_const_i32(bc, 3, 1);    /* r3 = i = 1 */
    emit_const_i32(bc, 4, 1);    /* r4 = 1 */
    /* r5 = temp */

    uint32_t loop = bc->size;
    uint32_t done = emit_if_ge(bc, 3, 0);  /* if i >= n, done */
    emit_move(bc, 5, 2);                    /* temp = curr */
    emit_add_i32(bc, 2, 1, 2);             /* curr = prev + curr */
    emit_move(bc, 1, 5);                    /* prev = temp */
    emit_add_i32(bc, 3, 3, 4);             /* i++ */
    uint32_t p = emit_goto_16_fwd(bc);
    bc_patch_back(bc, p, loop);

    bc_patch_here(bc, done);
    emit_return(bc, 2);
}

/* Program 4: factorial(10) = 3628800 */
static void build_fact10(Bytecode* bc)
{
    bc_init(bc);
    emit_const_i32(bc, 0, 10);   /* r0 = n */
    emit_const_i32(bc, 1, 1);    /* r1 = result = 1 */
    emit_const_i32(bc, 2, 1);    /* r2 = i = 1 */
    emit_const_i32(bc, 3, 1);    /* r3 = step */

    uint32_t loop = bc->size;
    uint32_t done = emit_if_gt(bc, 2, 0);  /* if i > n, done */
    emit_mul_i32(bc, 1, 1, 2);             /* result *= i */
    emit_add_i32(bc, 2, 2, 3);             /* i++ */
    uint32_t p = emit_goto_16_fwd(bc);
    bc_patch_back(bc, p, loop);

    bc_patch_here(bc, done);
    emit_return(bc, 1);
}

/* Program 5: GCD(48, 18) = 6  (Euclidean) */
static void build_gcd(Bytecode* bc)
{
    bc_init(bc);
    emit_const_i32(bc, 0, 48);   /* r0 = a */
    emit_const_i32(bc, 1, 18);   /* r1 = b */
    /* r2 = temp */

    uint32_t loop = bc->size;
    uint32_t done = emit_if_eqz(bc, 1);   /* while b != 0 */
    emit_rem_i32(bc, 2, 0, 1);            /* temp = a % b */
    emit_move(bc, 0, 1);                   /* a = b */
    emit_move(bc, 1, 2);                   /* b = temp */
    uint32_t p = emit_goto_16_fwd(bc);
    bc_patch_back(bc, p, loop);

    bc_patch_here(bc, done);
    emit_return(bc, 0);
}

/* Program 6: using stdlib native — print and return strlen */
static void build_stdlib_demo(Bytecode* bc)
{
    /* This program:
     *   call print_i32(99)      → prints "99\n" to stdout
     *   call strlen_vm("hello") → r0 = 5 (but we can't pass host ptr in file)
     *   return 99               → used for verification
     *
     * Since we cannot embed pointers in bytecode files, this demo
     * just calls print_i32 and returns a constant.            */
    bc_init(bc);
    emit_const_i32(bc, 0, 99);
    emit_call_void1(bc, 0 /* print_i32 */, 0);   /* print 99 */
    emit_const_i32(bc, 1, 42);
    emit_call_void1(bc, 0 /* print_i32 */, 1);   /* print 42 */
    emit_return(bc, 0);                            /* return 99 */
}

/* =========================================================================
 * main
 * ====================================================================== */

int main(void)
{
    printf("=== File Execution Round-Trip ===\n\n");

    VMContext ctx;
    vm_init(&ctx);
    vm_register_stdlib(&ctx);  /* needed for program 6 */

    /* ------------------------------------------------------------------ */
    /* Part 1: round-trip test — direct result must match file-loaded result */
    /* ------------------------------------------------------------------ */
    printf("--- Round-trip verification ---\n");

    struct {
        const char* label;
        const char* path;
        void (*build)(Bytecode*);
        uint16_t reg_count;
        int32_t  expected;
    } cases[] = {
        { "const 42",        "test_const42.cvmb", build_const42, 1,  42      },
        { "sum 1..100",      "test_sum.cvmb",     build_sum100,  4,  5050    },
        { "fib(15)",         "test_fib.cvmb",     build_fib15,   6,  610     },
        { "factorial(10)",   "test_fact.cvmb",    build_fact10,  4,  3628800 },
        { "gcd(48,18)",      "test_gcd.cvmb",     build_gcd,     3,  6       },
        { "stdlib print",    "test_stdlib.cvmb",  build_stdlib_demo, 2, 99   },
    };

    int n = (int)(sizeof(cases) / sizeof(cases[0]));
    Bytecode bc;
    int i;
    for (i = 0; i < n; i++) {
        cases[i].build(&bc);

        int32_t direct = run_direct(&ctx, &bc, cases[i].reg_count);
        int32_t filed  = run_via_file(&ctx, cases[i].path, &bc, cases[i].reg_count);

        if (direct == cases[i].expected && filed == cases[i].expected) {
            printf("  [PASS] %-20s  direct=%d  file=%d\n",
                   cases[i].label, direct, filed);
            g_pass++;
        } else {
            printf("  [FAIL] %-20s  expected=%d  direct=%d  file=%d\n",
                   cases[i].label, cases[i].expected, direct, filed);
            g_fail++;
        }
    }

    /* ------------------------------------------------------------------ */
    /* Part 2: file format validation */
    /* ------------------------------------------------------------------ */
    printf("\n--- Header validation ---\n");

    /* Write and verify header fields survive round-trip */
    build_sum100(&bc);
    const char* hdr_path = "test_hdr_check.cvmb";
    cvmb_write(hdr_path, bc.data, bc.size, 8 /*reg_count*/, 0 /*entry*/);

    CVMBHeader hdr;
    uint8_t* code = cvmb_read(hdr_path, &hdr);
    if (code) {
        check_i32("version",    (int32_t)hdr.version,   1);
        check_i32("reg_count",  (int32_t)hdr.reg_count, 8);
        check_i32("entry_pc",   (int32_t)hdr.entry_pc,  0);
        check_i32("code_size",  (int32_t)hdr.code_size, (int32_t)bc.size);
        cvmb_free(code);
    } else {
        g_fail++;
    }

    /* Bad magic → cvmb_read must return NULL */
    {
        FILE* f = fopen("bad_magic.cvmb", "wb");
        if (f) {
            uint8_t bad[16] = { 'X','X','X','X', 1,0, 4,0, 0,0,0,0, 1,0,0,0 };
            uint8_t payload = (uint8_t)OP_NOP;
            fwrite(bad, 1, 16, f);
            fwrite(&payload, 1, 1, f);
            fclose(f);

            CVMBHeader dummy;
            uint8_t* r = cvmb_read("bad_magic.cvmb", &dummy);
            check_bool("bad magic rejected", r == NULL);
            if (r) cvmb_free(r);
            remove("bad_magic.cvmb");
        }
    }

    /* ------------------------------------------------------------------ */
    /* Part 3: show file sizes */
    /* ------------------------------------------------------------------ */
    printf("\n--- Generated .cvmb file sizes ---\n");
    {
        const char* files[] = {
            "test_const42.cvmb","test_sum.cvmb","test_fib.cvmb",
            "test_fact.cvmb","test_gcd.cvmb","test_stdlib.cvmb",
            "test_hdr_check.cvmb"
        };
        int nf = (int)(sizeof(files)/sizeof(files[0]));
        int j;
        for (j = 0; j < nf; j++) {
            FILE* f = fopen(files[j], "rb");
            if (f) {
                fseek(f, 0, SEEK_END);
                long sz = ftell(f);
                fclose(f);
                printf("  %-28s  %ld bytes  (%ld header + %ld code)\n",
                       files[j], sz, (long)CVMB_HEADER_SIZE,
                       sz - (long)CVMB_HEADER_SIZE);
                remove(files[j]);    /* clean up */
            }
        }
    }

    print_summary();
    return g_fail > 0 ? 1 : 0;
}
