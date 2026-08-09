/*
 * vm_loader.h — File-based bytecode loader for the C/C++ Register VM
 *
 * Provides:
 *   1. A simple binary file format (.cvmb) for bytecode files
 *   2. cvmb_write() / cvmb_read() / cvmb_free()
 *   3. vm_register_stdlib() — a set of useful native functions
 *
 * =========================================================================
 * .cvmb file format  (all multi-byte fields are little-endian)
 *
 *   Offset  Size  Field
 *   ──────  ────  ────────────────────────────────────────────────────────
 *      0      4   magic      0x43 0x56 0x4D 0x42  ("CVMB")
 *      4      1   version    0x01
 *      5      1   flags      reserved — write 0, ignore on read
 *      6      2   reg_count  u16  number of VM registers to allocate
 *      8      4   entry_pc   u32  initial program counter (usually 0)
 *     12      4   code_size  u32  bytecode length in bytes
 *     16    ...   bytecode   raw VM instructions
 *
 *   Total header: 16 bytes.
 *
 * =========================================================================
 * Standard native function IDs  (registered by vm_register_stdlib)
 *
 *   ID   Signature                Description
 *   ──   ─────────────────────── ─────────────────────────────────────────
 *    0   print_i32  (i32)         printf("%d\n", args[0].i32)
 *    1   print_i64  (i64)         printf("%lld\n", args[0].i64)
 *    2   print_f32  (f32)         printf("%g\n",  args[0].f32)
 *    3   print_f64  (f64)         printf("%g\n",  args[0].f64)
 *    4   print_str  (ptr)         fputs(args[0].ptr, stdout); putchar('\n')
 *    5   print_nl   ()            putchar('\n')
 *    6   strlen_vm  (ptr) → i32   (int32_t)strlen(args[0].ptr)
 *    7   abs_i32    (i32) → i32   abs(args[0].i32)
 *    8   min_i32    (i32,i32)→i32 smaller of two i32 values
 *    9   max_i32    (i32,i32)→i32 larger  of two i32 values
 *   10   memset_vm  (ptr,i32,i32) memset(ptr, val, len)
 *   11   exit_vm    (i32)         exit(args[0].i32)
 *
 * =========================================================================
 * Usage example
 *
 *   // ---- Writer side ----
 *   #include "vm_builder.h"
 *   #include "vm_loader.h"
 *
 *   Bytecode bc; bc_init(&bc);
 *   emit_const_i32(&bc, 0, 42);
 *   emit_return(&bc, 0);
 *   cvmb_write("out.cvmb", bc.data, bc.size, 4, 0);   // reg_count=4, pc=0
 *
 *   // ---- Runner side ----
 *   #include "vm_loader.h"
 *
 *   CVMBHeader hdr;
 *   uint8_t* code = cvmb_read("out.cvmb", &hdr);      // heap-allocated
 *   VMContext ctx; vm_init(&ctx);
 *   vm_register_stdlib(&ctx);
 *   VMRegister regs[256];
 *   memset(regs, 0, sizeof(VMRegister) * hdr.reg_count);
 *   VMError err = vm_execute(&ctx, regs, hdr.reg_count,
 *                            hdr.entry_pc, code, hdr.code_size);
 *   cvmb_free(code);
 */

#ifndef VM_LOADER_H
#define VM_LOADER_H

#include "vm.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* =========================================================================
 * File format constants
 * ====================================================================== */

#define CVMB_MAGIC_0  0x43u   /* 'C' */
#define CVMB_MAGIC_1  0x56u   /* 'V' */
#define CVMB_MAGIC_2  0x4Du   /* 'M' */
#define CVMB_MAGIC_3  0x42u   /* 'B' */
#define CVMB_VERSION  0x01u
#define CVMB_HEADER_SIZE 16u

/* =========================================================================
 * Header struct (populated by cvmb_read)
 * ====================================================================== */

typedef struct {
    uint8_t  version;
    uint8_t  flags;
    uint16_t reg_count;   /* number of registers to allocate             */
    uint32_t entry_pc;    /* initial program counter                     */
    uint32_t code_size;   /* bytecode size in bytes                      */
} CVMBHeader;

/* =========================================================================
 * cvmb_write
 *
 * Write bytecode and a header to a .cvmb file.
 *
 *   path       — output file path
 *   code       — bytecode buffer
 *   code_size  — length of bytecode in bytes
 *   reg_count  — number of registers the program needs
 *   entry_pc   — initial program counter (usually 0)
 *
 * Returns 1 on success, 0 on failure (error printed to stderr).
 * ====================================================================== */
static int cvmb_write(const char* path,
                      const uint8_t* code, uint32_t code_size,
                      uint16_t reg_count, uint32_t entry_pc)
{
    FILE* f = fopen(path, "wb");
    if (!f) { perror(path); return 0; }

    /* Build 16-byte header */
    uint8_t hdr[CVMB_HEADER_SIZE];
    hdr[0] = CVMB_MAGIC_0;
    hdr[1] = CVMB_MAGIC_1;
    hdr[2] = CVMB_MAGIC_2;
    hdr[3] = CVMB_MAGIC_3;
    hdr[4] = CVMB_VERSION;
    hdr[5] = 0;                               /* flags */
    hdr[6] = (uint8_t)(reg_count);
    hdr[7] = (uint8_t)(reg_count >> 8);
    hdr[8]  = (uint8_t)(entry_pc);
    hdr[9]  = (uint8_t)(entry_pc >>  8);
    hdr[10] = (uint8_t)(entry_pc >> 16);
    hdr[11] = (uint8_t)(entry_pc >> 24);
    hdr[12] = (uint8_t)(code_size);
    hdr[13] = (uint8_t)(code_size >>  8);
    hdr[14] = (uint8_t)(code_size >> 16);
    hdr[15] = (uint8_t)(code_size >> 24);

    if (fwrite(hdr, 1, CVMB_HEADER_SIZE, f) != CVMB_HEADER_SIZE ||
        fwrite(code, 1, (size_t)code_size, f) != (size_t)code_size)
    {
        fprintf(stderr, "cvmb_write: write error\n");
        fclose(f);
        return 0;
    }
    fclose(f);
    return 1;
}

/* =========================================================================
 * cvmb_read
 *
 * Read a .cvmb file, populate *hdr, and return a heap-allocated buffer
 * containing the bytecode.  The caller must cvmb_free() the buffer.
 *
 * Returns NULL on failure (error printed to stderr).
 * ====================================================================== */
static uint8_t* cvmb_read(const char* path, CVMBHeader* hdr)
{
    FILE* f = fopen(path, "rb");
    if (!f) { perror(path); return NULL; }

    uint8_t raw[CVMB_HEADER_SIZE];
    if (fread(raw, 1, CVMB_HEADER_SIZE, f) != CVMB_HEADER_SIZE) {
        fprintf(stderr, "cvmb_read: file too short for header\n");
        fclose(f); return NULL;
    }

    /* Validate magic */
    if (raw[0] != CVMB_MAGIC_0 || raw[1] != CVMB_MAGIC_1 ||
        raw[2] != CVMB_MAGIC_2 || raw[3] != CVMB_MAGIC_3)
    {
        fprintf(stderr, "cvmb_read: bad magic (not a .cvmb file)\n");
        fclose(f); return NULL;
    }

    hdr->version   = raw[4];
    hdr->flags     = raw[5];
    hdr->reg_count = (uint16_t)(raw[6]) | ((uint16_t)(raw[7]) << 8);
    hdr->entry_pc  = (uint32_t)(raw[8])  | ((uint32_t)(raw[9])  << 8)
                   | ((uint32_t)(raw[10]) << 16) | ((uint32_t)(raw[11]) << 24);
    hdr->code_size = (uint32_t)(raw[12]) | ((uint32_t)(raw[13]) << 8)
                   | ((uint32_t)(raw[14]) << 16) | ((uint32_t)(raw[15]) << 24);

    if (hdr->reg_count == 0) {
        fprintf(stderr, "cvmb_read: reg_count is 0\n");
        fclose(f); return NULL;
    }
    if (hdr->code_size == 0) {
        fprintf(stderr, "cvmb_read: code_size is 0\n");
        fclose(f); return NULL;
    }

    uint8_t* code = (uint8_t*)malloc((size_t)hdr->code_size);
    if (!code) {
        fprintf(stderr, "cvmb_read: out of memory\n");
        fclose(f); return NULL;
    }
    if (fread(code, 1, (size_t)hdr->code_size, f) != (size_t)hdr->code_size) {
        fprintf(stderr, "cvmb_read: bytecode truncated\n");
        free(code); fclose(f); return NULL;
    }
    fclose(f);
    return code;
}

/* =========================================================================
 * cvmb_free
 *
 * Free a bytecode buffer returned by cvmb_read().
 * ====================================================================== */
static void cvmb_free(uint8_t* code)
{
    free(code);
}

/* =========================================================================
 * Standard native function implementations (IDs 0–11)
 *
 * Each is static so it is local to the including translation unit.
 * ====================================================================== */

static VMError vm_stdlib_print_i32(VMContext* ctx, uint32_t argc,
                                   VMRegister* args, VMRegister* out)
{
    (void)ctx; (void)out;
    if (argc < 1) return VM_ERR_BAD_ARGC;
    printf("%d\n", args[0].i32);
    return VM_OK;
}

static VMError vm_stdlib_print_i64(VMContext* ctx, uint32_t argc,
                                   VMRegister* args, VMRegister* out)
{
    (void)ctx; (void)out;
    if (argc < 1) return VM_ERR_BAD_ARGC;
    printf("%lld\n", (long long)args[0].i64);
    return VM_OK;
}

static VMError vm_stdlib_print_f32(VMContext* ctx, uint32_t argc,
                                   VMRegister* args, VMRegister* out)
{
    (void)ctx; (void)out;
    if (argc < 1) return VM_ERR_BAD_ARGC;
    printf("%g\n", (double)args[0].f32);
    return VM_OK;
}

static VMError vm_stdlib_print_f64(VMContext* ctx, uint32_t argc,
                                   VMRegister* args, VMRegister* out)
{
    (void)ctx; (void)out;
    if (argc < 1) return VM_ERR_BAD_ARGC;
    printf("%g\n", args[0].f64);
    return VM_OK;
}

static VMError vm_stdlib_print_str(VMContext* ctx, uint32_t argc,
                                   VMRegister* args, VMRegister* out)
{
    (void)ctx; (void)out;
    if (argc < 1) return VM_ERR_BAD_ARGC;
    if (args[0].ptr) { fputs((const char*)args[0].ptr, stdout); putchar('\n'); }
    return VM_OK;
}

static VMError vm_stdlib_print_nl(VMContext* ctx, uint32_t argc,
                                  VMRegister* args, VMRegister* out)
{
    (void)ctx; (void)argc; (void)args; (void)out;
    putchar('\n');
    return VM_OK;
}

static VMError vm_stdlib_strlen(VMContext* ctx, uint32_t argc,
                                VMRegister* args, VMRegister* out)
{
    (void)ctx;
    if (argc < 1) return VM_ERR_BAD_ARGC;
    out->i32 = args[0].ptr ? (int32_t)strlen((const char*)args[0].ptr) : 0;
    return VM_OK;
}

static VMError vm_stdlib_abs_i32(VMContext* ctx, uint32_t argc,
                                 VMRegister* args, VMRegister* out)
{
    (void)ctx;
    if (argc < 1) return VM_ERR_BAD_ARGC;
    int32_t v = args[0].i32;
    out->i32 = (v < 0) ? -v : v;
    return VM_OK;
}

static VMError vm_stdlib_min_i32(VMContext* ctx, uint32_t argc,
                                 VMRegister* args, VMRegister* out)
{
    (void)ctx;
    if (argc < 2) return VM_ERR_BAD_ARGC;
    out->i32 = (args[0].i32 < args[1].i32) ? args[0].i32 : args[1].i32;
    return VM_OK;
}

static VMError vm_stdlib_max_i32(VMContext* ctx, uint32_t argc,
                                 VMRegister* args, VMRegister* out)
{
    (void)ctx;
    if (argc < 2) return VM_ERR_BAD_ARGC;
    out->i32 = (args[0].i32 > args[1].i32) ? args[0].i32 : args[1].i32;
    return VM_OK;
}

static VMError vm_stdlib_memset(VMContext* ctx, uint32_t argc,
                                VMRegister* args, VMRegister* out)
{
    (void)ctx; (void)out;
    if (argc < 3) return VM_ERR_BAD_ARGC;
    if (args[0].ptr && args[2].i32 > 0)
        memset(args[0].ptr, (int)args[1].i32, (size_t)args[2].i32);
    return VM_OK;
}

static VMError vm_stdlib_exit(VMContext* ctx, uint32_t argc,
                              VMRegister* args, VMRegister* out)
{
    (void)ctx; (void)out;
    exit((argc >= 1) ? (int)args[0].i32 : 0);
    return VM_OK; /* unreachable */
}

/* =========================================================================
 * vm_register_stdlib
 *
 * Register the 12 standard native functions into a VMContext.
 * Call this before vm_execute().
 *
 * Existing registrations at these IDs will be overwritten.
 * ====================================================================== */
static void vm_register_stdlib(VMContext* ctx)
{
    vm_register_function(ctx,  0, vm_stdlib_print_i32);
    vm_register_function(ctx,  1, vm_stdlib_print_i64);
    vm_register_function(ctx,  2, vm_stdlib_print_f32);
    vm_register_function(ctx,  3, vm_stdlib_print_f64);
    vm_register_function(ctx,  4, vm_stdlib_print_str);
    vm_register_function(ctx,  5, vm_stdlib_print_nl);
    vm_register_function(ctx,  6, vm_stdlib_strlen);
    vm_register_function(ctx,  7, vm_stdlib_abs_i32);
    vm_register_function(ctx,  8, vm_stdlib_min_i32);
    vm_register_function(ctx,  9, vm_stdlib_max_i32);
    vm_register_function(ctx, 10, vm_stdlib_memset);
    vm_register_function(ctx, 11, vm_stdlib_exit);
}

#endif /* VM_LOADER_H */
