/*
 * Copyright (C) 2026 CVM Contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * cvma2cvmb.c — CVM Assembly Compiler / Assembler CLI Tool
 *
 * Converts CVM Assembly text files (.cvma) into CVM Binary Bytecode (.cvmb)
 *
 * Usage:
 *   cvma2cvmb <input.cvma> [-o output.cvmb] [--regs N] [--verbose]
 */

#include "../vm.h"
#include "../vm_assembler.h"
#include "../vm_loader.h"
#include "../vm_disassembler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static void print_usage(const char* prog)
{
    fprintf(stderr,
        "Usage: %s <input.cvma> [options]\n"
        "\n"
        "Options:\n"
        "  -o <output.cvmb>  Specify output file path (default: input with .cvmb extension)\n"
        "  --regs N          Set register count (override directive/auto-detect)\n"
        "  --entry N         Set entry program counter (default: 0)\n"
        "  --verbose, -v     Print assembly details and disassembly preview\n"
        "  --help, -h        Show this help message\n",
        prog);
}

/* Scan max register index referenced in text e.g. "r15" -> 15 */
static uint16_t detect_max_register(const char* text)
{
    uint16_t max_reg = 0;
    const char* p = text;
    while (*p) {
        if ((*p == 'r' || *p == 'R') && (p == text || (!isalnum((unsigned char)*(p - 1)) && *(p - 1) != '_'))) {
            p++;
            if (isdigit((unsigned char)*p)) {
                char* endp;
                long val = strtol(p, &endp, 10);
                if (val >= 0 && val <= 255) {
                    if ((uint16_t)val > max_reg) {
                        max_reg = (uint16_t)val;
                    }
                }
                p = endp;
                continue;
            }
        }
        p++;
    }
    return max_reg;
}

/* Scan for .registers or .regs directive in source */
static int scan_directive_regs(const char* text, uint16_t* out_regs)
{
    const char* p = text;
    while ((p = strstr(p, ".reg")) != NULL) {
        if (strncmp(p, ".registers", 10) == 0 || strncmp(p, ".regs", 5) == 0) {
            const char* space = strpbrk(p, " \t");
            if (space) {
                while (*space && isspace((unsigned char)*space)) space++;
                if (isdigit((unsigned char)*space)) {
                    long val = strtol(space, NULL, 10);
                    if (val > 0 && val <= 256) {
                        *out_regs = (uint16_t)val;
                        return 1;
                    }
                }
            }
        }
        p++;
    }
    return 0;
}

int main(int argc, char** argv)
{
    const char* input_path = NULL;
    char default_out_path[512] = "";
    const char* output_path = NULL;
    int reg_override = 0;
    uint32_t entry_pc = 0;
    int verbose = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: -o requires output file path\n");
                return 1;
            }
            output_path = argv[++i];
        } else if (strcmp(argv[i], "--regs") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --regs requires number\n");
                return 1;
            }
            reg_override = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--entry") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --entry requires number\n");
                return 1;
            }
            entry_pc = (uint32_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--verbose") == 0 || strcmp(argv[i], "-v") == 0) {
            verbose = 1;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "Error: unknown option '%s'\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        } else {
            if (input_path) {
                fprintf(stderr, "Error: multiple input files specified\n");
                return 1;
            }
            input_path = argv[i];
        }
    }

    if (!input_path) {
        print_usage(argv[0]);
        return 1;
    }

    /* Auto output filename: replaces .cvma with .cvmb or appends .cvmb */
    if (!output_path) {
        strncpy(default_out_path, input_path, sizeof(default_out_path) - 6);
        char* ext = strrchr(default_out_path, '.');
        if (ext && (strcmp(ext, ".cvma") == 0 || strcmp(ext, ".asm") == 0 || strcmp(ext, ".s") == 0)) {
            strcpy(ext, ".cvmb");
        } else {
            strcat(default_out_path, ".cvmb");
        }
        output_path = default_out_path;
    }

    /* Read source text file */
    FILE* f = fopen(input_path, "rb");
    if (!f) {
        perror(input_path);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    rewind(f);

    if (fsize < 0 || fsize > 10 * 1024 * 1024) {
        fprintf(stderr, "Error: file size invalid or too large\n");
        fclose(f);
        return 1;
    }

    char* source_text = (char*)malloc((size_t)fsize + 1);
    if (!source_text) {
        fprintf(stderr, "Error: out of memory reading file\n");
        fclose(f);
        return 1;
    }
    size_t nread = fread(source_text, 1, (size_t)fsize, f);
    source_text[nread] = '\0';
    fclose(f);

    /* Determine register count */
    uint16_t reg_count = 16;
    if (reg_override > 0) {
        reg_count = (uint16_t)reg_override;
    } else {
        uint16_t dir_regs = 0;
        if (scan_directive_regs(source_text, &dir_regs)) {
            reg_count = dir_regs;
        } else {
            uint16_t max_detected = detect_max_register(source_text);
            if (max_detected + 1 > reg_count) {
                reg_count = max_detected + 1;
            }
        }
    }

    if (verbose) {
        printf("[cvma2cvmb] Assembling '%s' -> '%s' (allocated regs: %u)...\n",
               input_path, output_path, (unsigned)reg_count);
    }

    /* Assemble text to bytecode */
    VMAssembleResult res = vm_assemble(source_text);
    if (!res.success) {
        fprintf(stderr, "[cvma2cvmb] ERROR in '%s' line %d: %s\n",
                input_path, res.error_line, res.error_msg);
        free(source_text);
        return 1;
    }

    /* Write binary .cvmb file */
    if (!cvmb_write(output_path, res.bytecode, (uint32_t)res.size, reg_count, entry_pc)) {
        fprintf(stderr, "[cvma2cvmb] ERROR: Failed to write output file '%s'\n", output_path);
        vm_assemble_free(&res);
        free(source_text);
        return 1;
    }

    printf("[cvma2cvmb] SUCCESS: Successfully assembled '%s' -> '%s'\n", input_path, output_path);
    printf("[cvma2cvmb] Header: magic=CVMB, version=1, reg_count=%u, entry_pc=%u, code_size=%u bytes\n",
           (unsigned)reg_count, (unsigned)entry_pc, (unsigned)res.size);

    if (verbose) {
        printf("\n--- Disassembly Preview ---\n");
        char* disasm = vm_disassemble(res.bytecode, res.size);
        if (disasm) {
            printf("%s", disasm);
            free(disasm);
        }
    }

    vm_assemble_free(&res);
    free(source_text);
    return 0;
}
