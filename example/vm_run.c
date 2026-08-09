/*
 * vm_run.c — Command-line runner for .cvmb bytecode files
 *
 * Usage:
 *   vm_run <program.cvmb> [--debug]
 *
 * Loads the file, registers the standard library, executes, and prints
 * the return value (if the program ends with OP_RETURN).
 *
 * Compile:
 *   gcc -std=c99 -Wall -I.. -o vm_run vm_run.c ../vm.c
 */

#include "../vm_loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Human-readable error messages */
static const char* vm_error_str(VMError err)
{
    switch (err) {
    case VM_OK:                   return "OK";
    case VM_ERR_INVALID_OPCODE:   return "invalid opcode";
    case VM_ERR_OUT_OF_BOUNDS:    return "out of bounds";
    case VM_ERR_DIV_ZERO:         return "division by zero";
    case VM_ERR_INVALID_REGISTER: return "invalid register";
    case VM_ERR_BAD_FUNCTION:     return "bad function id";
    case VM_ERR_BAD_ARGC:         return "bad argument count";
    default:                      return "unknown error";
    }
}

static void print_usage(const char* prog)
{
    fprintf(stderr,
        "Usage: %s <program.cvmb> [options]\n"
        "\n"
        "Options:\n"
        "  --debug     Enable instruction tracing (if VM was compiled with VM_DEBUG)\n"
        "  --regs N    Override register count (default: from file header)\n"
        "  --help      Show this help\n"
        "\n"
        "Exit code: 0 on success, non-zero on VM error.\n",
        prog);
}

int main(int argc, char** argv)
{
    const char* path    = NULL;
    int         debug   = 0;
    int         reg_override = 0;  /* 0 = use header value */

    /* ---- parse arguments ---- */
    int i;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--debug") == 0) {
            debug = 1;
        } else if (strcmp(argv[i], "--regs") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "error: --regs requires a value\n");
                return 1;
            }
            reg_override = atoi(argv[++i]);
            if (reg_override <= 0 || reg_override > 256) {
                fprintf(stderr, "error: --regs must be 1..256\n");
                return 1;
            }
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "error: unknown option '%s'\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        } else {
            if (path) {
                fprintf(stderr, "error: multiple input files not supported\n");
                return 1;
            }
            path = argv[i];
        }
    }

    if (!path) {
        print_usage(argv[0]);
        return 1;
    }

    /* ---- load .cvmb file ---- */
    CVMBHeader  hdr;
    uint8_t*    code = cvmb_read(path, &hdr);
    if (!code) return 1;  /* error already printed by cvmb_read */

    uint32_t reg_count = reg_override ? (uint32_t)reg_override : hdr.reg_count;

    if (reg_count > 256) {
        fprintf(stderr, "error: reg_count %u exceeds maximum 256\n", reg_count);
        cvmb_free(code);
        return 1;
    }

    /* ---- set up context ---- */
    VMContext ctx;
    vm_init(&ctx);
    ctx.debug = debug;
    vm_register_stdlib(&ctx);

    /* ---- allocate + zero registers ---- */
    VMRegister* regs = (VMRegister*)calloc(reg_count, sizeof(VMRegister));
    if (!regs) {
        fprintf(stderr, "error: out of memory\n");
        cvmb_free(code);
        return 1;
    }

    /* ---- print file info ---- */
    fprintf(stderr,
        "[vm_run] file:      %s\n"
        "[vm_run] version:   %u\n"
        "[vm_run] reg_count: %u\n"
        "[vm_run] entry_pc:  %u\n"
        "[vm_run] code_size: %u bytes\n",
        path, (unsigned)hdr.version,
        (unsigned)reg_count, (unsigned)hdr.entry_pc,
        (unsigned)hdr.code_size);

    /* ---- execute ---- */
    VMError err = vm_execute(&ctx, regs, reg_count,
                             hdr.entry_pc, code, hdr.code_size);

    /* ---- report result ---- */
    if (err != VM_OK) {
        fprintf(stderr, "[vm_run] error: %s (code %d)\n",
                vm_error_str(err), (int)err);
        free(regs);
        cvmb_free(code);
        return (int)err;
    }

    /* Print the return value in all common interpretations */
    fprintf(stderr, "[vm_run] exit: OK\n");
    fprintf(stderr,
        "[vm_run] result.i32 = %d\n"
        "[vm_run] result.i64 = %lld\n"
        "[vm_run] result.f32 = %g\n"
        "[vm_run] result.f64 = %g\n"
        "[vm_run] result.ptr = %p\n",
        ctx.result.i32,
        (long long)ctx.result.i64,
        (double)ctx.result.f32,
        ctx.result.f64,
        ctx.result.ptr);

    free(regs);
    cvmb_free(code);
    return 0;
}
