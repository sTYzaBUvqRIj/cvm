/*
 * Copyright (C) 2026 CVM Contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * vm_assembler.h  —  Two-Pass Bytecode Assembler for C/C++ Register VM
 *
 * Header-only library for assembling textual CVM assembly source code
 * into binary bytecode. Supports labels, comments, hex/dec immediates,
 * floats, and all 95 CVM opcodes.
 */

#ifndef VM_ASSEMBLER_H
#define VM_ASSEMBLER_H

#ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(disable: 4505 4996)
#elif defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wunused-function"
#endif

#include "vm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Result structure for vm_assemble() */
typedef struct {
    uint8_t* bytecode;    /* Heap-allocated binary bytecode (caller frees with vm_assemble_free) */
    size_t   size;        /* Bytecode length in bytes */
    size_t   capacity;    /* Buffer capacity */
    int      success;     /* 1 on success, 0 on failure */
    int      error_line;  /* Line number where error occurred (1-indexed) */
    char     error_msg[256]; /* Error description if success == 0 */
} VMAssembleResult;

/* Label entry table */
typedef struct {
    char   name[64];
    size_t pc;
} VMAssmLabel;

typedef struct {
    VMAssmLabel items[512];
    size_t      count;
} VMAssmLabelTable;

/* Portable case-insensitive string comparison */
static int vm_assm_stricmp(const char* s1, const char* s2)
{
    while (*s1 && *s2) {
        int c1 = tolower((unsigned char)*s1);
        int c2 = tolower((unsigned char)*s2);
        if (c1 != c2) return c1 - c2;
        s1++; s2++;
    }
    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}

/* Add a label to table */
static int vm_assm_add_label(VMAssmLabelTable* table, const char* name, size_t pc)
{
    if (table->count >= sizeof(table->items) / sizeof(table->items[0])) {
        return 0;
    }
    for (size_t i = 0; i < table->count; i++) {
        if (vm_assm_stricmp(table->items[i].name, name) == 0) {
            return 0; /* Duplicate label */
        }
    }
    strncpy(table->items[table->count].name, name, sizeof(table->items[0].name) - 1);
    table->items[table->count].name[sizeof(table->items[0].name) - 1] = '\0';
    table->items[table->count].pc = pc;
    table->count++;
    return 1;
}

/* Find label PC */
static int vm_assm_find_label(const VMAssmLabelTable* table, const char* name, size_t* out_pc)
{
    for (size_t i = 0; i < table->count; i++) {
        if (vm_assm_stricmp(table->items[i].name, name) == 0) {
            *out_pc = table->items[i].pc;
            return 1;
        }
    }
    return 0;
}

/* Buffer emitters */
static void vm_assm_emit_u8(VMAssembleResult* res, uint8_t v)
{
    if (res->size + 1 > res->capacity) {
        res->capacity = res->capacity ? res->capacity * 2 : 256;
        res->bytecode = (uint8_t*)realloc(res->bytecode, res->capacity);
    }
    res->bytecode[res->size++] = v;
}

static void vm_assm_emit_u16(VMAssembleResult* res, uint16_t v)
{
    vm_assm_emit_u8(res, (uint8_t)(v & 0xFF));
    vm_assm_emit_u8(res, (uint8_t)((v >> 8) & 0xFF));
}

static void vm_assm_emit_u32(VMAssembleResult* res, uint32_t v)
{
    vm_assm_emit_u8(res, (uint8_t)(v & 0xFF));
    vm_assm_emit_u8(res, (uint8_t)((v >> 8) & 0xFF));
    vm_assm_emit_u8(res, (uint8_t)((v >> 16) & 0xFF));
    vm_assm_emit_u8(res, (uint8_t)((v >> 24) & 0xFF));
}

static void vm_assm_emit_u64(VMAssembleResult* res, uint64_t v)
{
    vm_assm_emit_u32(res, (uint32_t)(v & 0xFFFFFFFF));
    vm_assm_emit_u32(res, (uint32_t)(v >> 32));
}

/* Parse register token like "r0", "R12", "r255" or raw integer "0" */
static int vm_assm_parse_reg(const char* tok, uint16_t* out_reg)
{
    if (!tok || !*tok) return 0;
    if (tok[0] == 'r' || tok[0] == 'R') tok++;
    char* endp;
    long val = strtol(tok, &endp, 10);
    if (*endp != '\0' || val < 0 || val > 65535) return 0;
    *out_reg = (uint16_t)val;
    return 1;
}

/* Parse signed 64-bit integer token (decimal or hex) */
static int vm_assm_parse_int64(const char* tok, int64_t* out_val)
{
    if (!tok || !*tok) return 0;
    char* endp;
    int64_t val = (int64_t)strtoll(tok, &endp, 0);
    if (*endp != '\0') return 0;
    *out_val = val;
    return 1;
}

/* Parse double token */
static int vm_assm_parse_double(const char* tok, double* out_val)
{
    if (!tok || !*tok) return 0;
    char temp[64];
    strncpy(temp, tok, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';
    size_t len = strlen(temp);
    if (len > 0 && (temp[len - 1] == 'f' || temp[len - 1] == 'F')) {
        temp[len - 1] = '\0';
    }
    char* endp;
    double val = strtod(temp, &endp);
    if (*endp != '\0') return 0;
    *out_val = val;
    return 1;
}

/* Map string mnemonic to VMOpcode */
static int vm_assm_parse_opcode(const char* name, VMOpcode* out_op)
{
    static const struct { const char* name; VMOpcode op; } map[] = {
        { "NOP", OP_NOP }, { "MOVE", OP_MOVE },
        { "CONST_I8", OP_CONST_I8 }, { "CONST_I16", OP_CONST_I16 },
        { "CONST_I32", OP_CONST_I32 }, { "CONST_I64", OP_CONST_I64 },
        { "CONST_F32", OP_CONST_F32 }, { "CONST_F64", OP_CONST_F64 },
        { "ADD_I32", OP_ADD_I32 }, { "SUB_I32", OP_SUB_I32 }, { "MUL_I32", OP_MUL_I32 }, { "DIV_I32", OP_DIV_I32 }, { "REM_I32", OP_REM_I32 },
        { "ADD_I64", OP_ADD_I64 }, { "SUB_I64", OP_SUB_I64 }, { "MUL_I64", OP_MUL_I64 }, { "DIV_I64", OP_DIV_I64 }, { "REM_I64", OP_REM_I64 },
        { "DIV_U32", OP_DIV_U32 }, { "REM_U32", OP_REM_U32 }, { "DIV_U64", OP_DIV_U64 }, { "REM_U64", OP_REM_U64 },
        { "ADD_F32", OP_ADD_F32 }, { "SUB_F32", OP_SUB_F32 }, { "MUL_F32", OP_MUL_F32 }, { "DIV_F32", OP_DIV_F32 },
        { "ADD_F64", OP_ADD_F64 }, { "SUB_F64", OP_SUB_F64 }, { "MUL_F64", OP_MUL_F64 }, { "DIV_F64", OP_DIV_F64 },
        { "NEG_I32", OP_NEG_I32 }, { "NEG_I64", OP_NEG_I64 }, { "NEG_F32", OP_NEG_F32 }, { "NEG_F64", OP_NEG_F64 },
        { "NOT_I32", OP_NOT_I32 }, { "NOT_I64", OP_NOT_I64 },
        { "AND_I32", OP_AND_I32 }, { "OR_I32", OP_OR_I32 }, { "XOR_I32", OP_XOR_I32 },
        { "AND_I64", OP_AND_I64 }, { "OR_I64", OP_OR_I64 }, { "XOR_I64", OP_XOR_I64 },
        { "SHL_I32", OP_SHL_I32 }, { "SHR_I32", OP_SHR_I32 }, { "USHR_I32", OP_USHR_I32 },
        { "SHL_I64", OP_SHL_I64 }, { "SHR_I64", OP_SHR_I64 }, { "USHR_I64", OP_USHR_I64 },
        { "CMP_I32", OP_CMP_I32 }, { "CMP_I64", OP_CMP_I64 }, { "CMP_F32", OP_CMP_F32 }, { "CMP_F64", OP_CMP_F64 }, { "CMP_F32_GT", OP_CMP_F32_GT },
        { "I32_TO_I8", OP_I32_TO_I8 }, { "I32_TO_I16", OP_I32_TO_I16 }, { "I32_TO_I64", OP_I32_TO_I64 },
        { "I32_TO_F32", OP_I32_TO_F32 }, { "I32_TO_F64", OP_I32_TO_F64 },
        { "I64_TO_I32", OP_I64_TO_I32 }, { "I64_TO_F32", OP_I64_TO_F32 }, { "I64_TO_F64", OP_I64_TO_F64 },
        { "F32_TO_I32", OP_F32_TO_I32 }, { "F32_TO_I64", OP_F32_TO_I64 }, { "F32_TO_F64", OP_F32_TO_F64 },
        { "F64_TO_I32", OP_F64_TO_I32 }, { "F64_TO_I64", OP_F64_TO_I64 }, { "F64_TO_F32", OP_F64_TO_F32 },
        { "LOAD8", OP_LOAD8 }, { "LOAD8S", OP_LOAD8S }, { "LOAD16", OP_LOAD16 }, { "LOAD16S", OP_LOAD16S },
        { "LOAD32", OP_LOAD32 }, { "LOAD32S", OP_LOAD32S }, { "LOAD64", OP_LOAD64 }, { "LOAD_PTR", OP_LOAD_PTR },
        { "STORE8", OP_STORE8 }, { "STORE16", OP_STORE16 }, { "STORE32", OP_STORE32 }, { "STORE64", OP_STORE64 }, { "STORE_PTR", OP_STORE_PTR },
        { "LEA", OP_LEA },
        { "GOTO", OP_GOTO }, { "GOTO_16", OP_GOTO_16 }, { "GOTO_32", OP_GOTO_32 },
        { "IF_EQ", OP_IF_EQ }, { "IF_NE", OP_IF_NE }, { "IF_LT", OP_IF_LT }, { "IF_GE", OP_IF_GE }, { "IF_GT", OP_IF_GT }, { "IF_LE", OP_IF_LE },
        { "IF_EQZ", OP_IF_EQZ }, { "IF_NEZ", OP_IF_NEZ }, { "IF_LTZ", OP_IF_LTZ }, { "IF_GEZ", OP_IF_GEZ }, { "IF_GTZ", OP_IF_GTZ }, { "IF_LEZ", OP_IF_LEZ },
        { "CALL", OP_CALL }, { "CALL_VOID", OP_CALL_VOID },
        { "RETURN_VOID", OP_RETURN_VOID }, { "RETURN", OP_RETURN }
    };
    size_t count = sizeof(map) / sizeof(map[0]);
    for (size_t i = 0; i < count; i++) {
        if (vm_assm_stricmp(map[i].name, name) == 0) {
            *out_op = map[i].op;
            return 1;
        }
    }
    return 0;
}

/* Pre-process line: strip comments, labels, address prefix, punctuation */
static void vm_assm_clean_line(const char* raw_line, char* out_label, char* out_cmd)
{
    out_label[0] = '\0';
    out_cmd[0]   = '\0';

    char work[512];
    strncpy(work, raw_line, sizeof(work) - 1);
    work[sizeof(work) - 1] = '\0';

    /* Strip comments (#, ;, //) */
    char* comment = strchr(work, '#');
    if (comment) *comment = '\0';
    comment = strchr(work, ';');
    if (comment) *comment = '\0';
    comment = strstr(work, "//");
    if (comment) *comment = '\0';

    /* Trim leading spaces */
    char* cur = work;
    while (*cur && isspace((unsigned char)*cur)) cur++;

    /* Ignore assembler directives starting with '.' (e.g. .registers, .regs, .entry) */
    if (*cur == '.') {
        return;
    }

    /* Check for address prefix like "0004:" */
    char* colon = strchr(cur, ':');
    if (colon) {
        int is_addr = 1;
        for (char* p = cur; p < colon; p++) {
            if (!isxdigit((unsigned char)*p) && *p != 'x' && *p != 'X') {
                is_addr = 0;
                break;
            }
        }
        if (is_addr && (colon - cur) <= 8) {
            cur = colon + 1;
            while (*cur && isspace((unsigned char)*cur)) cur++;
            colon = strchr(cur, ':');
        }
    }

    /* Check for label definition "label_name:" */
    if (colon) {
        int is_label = 1;
        for (char* p = cur; p < colon; p++) {
            if (!isalnum((unsigned char)*p) && *p != '_') {
                is_label = 0;
                break;
            }
        }
        if (is_label && (colon - cur) > 0) {
            size_t llen = (size_t)(colon - cur);
            if (llen < 64) {
                memcpy(out_label, cur, llen);
                out_label[llen] = '\0';
            }
            cur = colon + 1;
            while (*cur && isspace((unsigned char)*cur)) cur++;
        }
    }

    /* Clean punctuation in command: replace commas, brackets, plus signs with spaces */
    char* dst = out_cmd;
    while (*cur) {
        char c = *cur++;
        if (c == ',' || c == '[' || c == ']' || c == '+') {
            *dst++ = ' ';
        } else {
            *dst++ = c;
        }
    }
    *dst = '\0';

    /* Trim trailing spaces */
    size_t len = strlen(out_cmd);
    while (len > 0 && isspace((unsigned char)out_cmd[len - 1])) {
        out_cmd[--len] = '\0';
    }
}

/* Tokenize clean command string by whitespace */
static int vm_assm_tokenize(char* cmd, char* tokens[], int max_tokens)
{
    int count = 0;
    char* p = strtok(cmd, " \t\r\n");
    while (p && count < max_tokens) {
        tokens[count++] = p;
        p = strtok(NULL, " \t\r\n");
    }
    return count;
}

/* Calculate length in bytes for an instruction */
static int vm_assm_calc_inst_size(VMOpcode op, int num_tokens, char* tokens[])
{
    switch (op) {
        case OP_NOP:          return 2;
        case OP_MOVE:         return 5;
        case OP_CONST_I8:     return 4;
        case OP_CONST_I16:    return 5;
        case OP_CONST_I32:    return 7;
        case OP_CONST_I64:    return 11;
        case OP_CONST_F32:    return 7;
        case OP_CONST_F64:    return 11;

        case OP_ADD_I32: case OP_SUB_I32: case OP_MUL_I32: case OP_DIV_I32: case OP_REM_I32:
        case OP_DIV_U32: case OP_REM_U32:
        case OP_ADD_I64: case OP_SUB_I64: case OP_MUL_I64: case OP_DIV_I64: case OP_REM_I64:
        case OP_DIV_U64: case OP_REM_U64:
        case OP_ADD_F32: case OP_SUB_F32: case OP_MUL_F32: case OP_DIV_F32:
        case OP_ADD_F64: case OP_SUB_F64: case OP_MUL_F64: case OP_DIV_F64:
        case OP_AND_I32: case OP_OR_I32:  case OP_XOR_I32:
        case OP_AND_I64: case OP_OR_I64:  case OP_XOR_I64:
        case OP_SHL_I32: case OP_SHR_I32: case OP_USHR_I32:
        case OP_SHL_I64: case OP_SHR_I64: case OP_USHR_I64:
        case OP_CMP_I32: case OP_CMP_I64: case OP_CMP_F32: case OP_CMP_F64: case OP_CMP_F32_GT:
            return 5;

        case OP_NEG_I32: case OP_NEG_I64: case OP_NEG_F32: case OP_NEG_F64:
        case OP_NOT_I32: case OP_NOT_I64:
        case OP_I32_TO_I8:  case OP_I32_TO_I16: case OP_I32_TO_I64:
        case OP_I32_TO_F32: case OP_I32_TO_F64: case OP_I64_TO_I32:
        case OP_I64_TO_F32: case OP_I64_TO_F64: case OP_F32_TO_I32:
        case OP_F32_TO_I64: case OP_F32_TO_F64: case OP_F64_TO_I32:
        case OP_F64_TO_I64: case OP_F64_TO_F32:
        case OP_LOAD8:  case OP_LOAD8S: case OP_LOAD16: case OP_LOAD16S:
        case OP_LOAD32: case OP_LOAD32S: case OP_LOAD64: case OP_LOAD_PTR:
        case OP_STORE8: case OP_STORE16: case OP_STORE32: case OP_STORE64: case OP_STORE_PTR:
            return 4;

        case OP_LEA:          return 8;
        case OP_GOTO:         return 3;
        case OP_GOTO_16:      return 4;
        case OP_GOTO_32:      return 6;

        case OP_IF_EQ: case OP_IF_NE: case OP_IF_LT:
        case OP_IF_GE: case OP_IF_GT: case OP_IF_LE:
            return 6;

        case OP_IF_EQZ: case OP_IF_NEZ: case OP_IF_LTZ:
        case OP_IF_GEZ: case OP_IF_GTZ: case OP_IF_LEZ:
            return 5;

        case OP_CALL: {
            /* tokens[0]=CALL, tokens[1]=dst, tokens[2]=id, tokens[3..N]=args (or tokens[3]=argc) */
            if (num_tokens < 3) return 0;
            int argc = num_tokens - 3;
            if (num_tokens >= 4) {
                int64_t explicit_argc = 0;
                if (vm_assm_parse_int64(tokens[3], &explicit_argc)) {
                    if (explicit_argc == num_tokens - 4) {
                        argc = (int)explicit_argc;
                    }
                }
            }
            return 8 + argc;
        }

        case OP_CALL_VOID: {
            /* tokens[0]=CALL_VOID, tokens[1]=id, tokens[2..N]=args (or tokens[2]=argc) */
            if (num_tokens < 2) return 0;
            int argc = num_tokens - 2;
            if (num_tokens >= 3) {
                int64_t explicit_argc = 0;
                if (vm_assm_parse_int64(tokens[2], &explicit_argc)) {
                    if (explicit_argc == num_tokens - 3) {
                        argc = (int)explicit_argc;
                    }
                }
            }
            return 7 + argc;
        }

        case OP_RETURN_VOID:  return 2;
        case OP_RETURN:       return 3;

        default:              return 0;
    }
}

/* Assemble source text into binary bytecode */
static VMAssembleResult vm_assemble(const char* source_text)
{
    VMAssembleResult res;
    memset(&res, 0, sizeof(res));
    res.success = 1;

    if (!source_text) {
        res.success = 0;
        snprintf(res.error_msg, sizeof(res.error_msg), "Null source text");
        return res;
    }

    VMAssmLabelTable labels;
    memset(&labels, 0, sizeof(labels));

    /* =========================================================================
     * PASS 1: Record labels & calculate instruction addresses
     * ====================================================================== */
    size_t current_pc = 0;
    int line_num = 0;

    const char* src_ptr = source_text;
    char line[512];

    while (*src_ptr) {
        line_num++;
        size_t len = 0;
        while (*src_ptr && *src_ptr != '\n' && *src_ptr != '\r' && len < sizeof(line) - 1) {
            line[len++] = *src_ptr++;
        }
        line[len] = '\0';
        if (*src_ptr == '\r') src_ptr++;
        if (*src_ptr == '\n') src_ptr++;

        char label[64];
        char cmd[512];
        vm_assm_clean_line(line, label, cmd);

        if (label[0] != '\0') {
            if (!vm_assm_add_label(&labels, label, current_pc)) {
                res.success = 0;
                res.error_line = line_num;
                snprintf(res.error_msg, sizeof(res.error_msg), "Duplicate or invalid label '%s'", label);
                return res;
            }
        }

        if (cmd[0] != '\0') {
            char cmd_copy[512];
            strcpy(cmd_copy, cmd);
            char* tokens[64];
            int ntok = vm_assm_tokenize(cmd_copy, tokens, 64);
            if (ntok > 0) {
                VMOpcode op;
                if (!vm_assm_parse_opcode(tokens[0], &op)) {
                    res.success = 0;
                    res.error_line = line_num;
                    snprintf(res.error_msg, sizeof(res.error_msg), "Unknown opcode '%s'", tokens[0]);
                    return res;
                }
                int isize = vm_assm_calc_inst_size(op, ntok, tokens);
                if (isize <= 0) {
                    res.success = 0;
                    res.error_line = line_num;
                    snprintf(res.error_msg, sizeof(res.error_msg), "Invalid arguments for '%s'", tokens[0]);
                    return res;
                }
                current_pc += (size_t)isize;
            }
        }
    }

    /* =========================================================================
     * PASS 2: Encode instructions into binary bytecode
     * ====================================================================== */
    src_ptr = source_text;
    line_num = 0;
    current_pc = 0;

    while (*src_ptr) {
        line_num++;
        size_t len = 0;
        while (*src_ptr && *src_ptr != '\n' && *src_ptr != '\r' && len < sizeof(line) - 1) {
            line[len++] = *src_ptr++;
        }
        line[len] = '\0';
        if (*src_ptr == '\r') src_ptr++;
        if (*src_ptr == '\n') src_ptr++;

        char label[64];
        char cmd[512];
        vm_assm_clean_line(line, label, cmd);

        if (cmd[0] == '\0') continue;

        char cmd_copy[512];
        strcpy(cmd_copy, cmd);
        char* tokens[64];
        int ntok = vm_assm_tokenize(cmd_copy, tokens, 64);
        if (ntok <= 0) continue;

        VMOpcode op;
        if (!vm_assm_parse_opcode(tokens[0], &op)) {
            res.success = 0;
            res.error_line = line_num;
            snprintf(res.error_msg, sizeof(res.error_msg), "Unknown opcode '%s'", tokens[0]);
            if (res.bytecode) free(res.bytecode);
            res.bytecode = NULL;
            return res;
        }

        /* Emit opcode u16 LE */
        vm_assm_emit_u16(&res, (uint16_t)op);

        int inst_size = vm_assm_calc_inst_size(op, ntok, tokens);

        switch (op) {
            case OP_NOP:
            case OP_RETURN_VOID:
                break;

            case OP_MOVE: {
                uint16_t dst = 0, src = 0;
                if (ntok < 3 || !vm_assm_parse_reg(tokens[1], &dst) || !vm_assm_parse_reg(tokens[2], &src)) {
                    goto parse_err;
                }
                vm_assm_emit_u8(&res, (uint8_t)dst);
                vm_assm_emit_u16(&res, src);
                break;
            }

            case OP_CONST_I8: {
                uint16_t dst = 0; int64_t val = 0;
                if (ntok < 3 || !vm_assm_parse_reg(tokens[1], &dst) || !vm_assm_parse_int64(tokens[2], &val)) {
                    goto parse_err;
                }
                vm_assm_emit_u8(&res, (uint8_t)dst);
                vm_assm_emit_u8(&res, (uint8_t)(int8_t)val);
                break;
            }

            case OP_CONST_I16: {
                uint16_t dst = 0; int64_t val = 0;
                if (ntok < 3 || !vm_assm_parse_reg(tokens[1], &dst) || !vm_assm_parse_int64(tokens[2], &val)) {
                    goto parse_err;
                }
                vm_assm_emit_u8(&res, (uint8_t)dst);
                vm_assm_emit_u16(&res, (uint16_t)(int16_t)val);
                break;
            }

            case OP_CONST_I32: {
                uint16_t dst = 0; int64_t val = 0;
                if (ntok < 3 || !vm_assm_parse_reg(tokens[1], &dst) || !vm_assm_parse_int64(tokens[2], &val)) {
                    goto parse_err;
                }
                vm_assm_emit_u8(&res, (uint8_t)dst);
                vm_assm_emit_u32(&res, (uint32_t)(int32_t)val);
                break;
            }

            case OP_CONST_I64: {
                uint16_t dst = 0; int64_t val = 0;
                if (ntok < 3 || !vm_assm_parse_reg(tokens[1], &dst) || !vm_assm_parse_int64(tokens[2], &val)) {
                    goto parse_err;
                }
                vm_assm_emit_u8(&res, (uint8_t)dst);
                vm_assm_emit_u64(&res, (uint64_t)val);
                break;
            }

            case OP_CONST_F32: {
                uint16_t dst = 0; double val = 0.0;
                if (ntok < 3 || !vm_assm_parse_reg(tokens[1], &dst) || !vm_assm_parse_double(tokens[2], &val)) {
                    goto parse_err;
                }
                float f = (float)val;
                uint32_t bits;
                memcpy(&bits, &f, sizeof(float));
                vm_assm_emit_u8(&res, (uint8_t)dst);
                vm_assm_emit_u32(&res, bits);
                break;
            }

            case OP_CONST_F64: {
                uint16_t dst = 0; double val = 0.0;
                if (ntok < 3 || !vm_assm_parse_reg(tokens[1], &dst) || !vm_assm_parse_double(tokens[2], &val)) {
                    goto parse_err;
                }
                uint64_t bits;
                memcpy(&bits, &val, sizeof(double));
                vm_assm_emit_u8(&res, (uint8_t)dst);
                vm_assm_emit_u64(&res, bits);
                break;
            }

            case OP_ADD_I32: case OP_SUB_I32: case OP_MUL_I32: case OP_DIV_I32: case OP_REM_I32:
            case OP_DIV_U32: case OP_REM_U32:
            case OP_ADD_I64: case OP_SUB_I64: case OP_MUL_I64: case OP_DIV_I64: case OP_REM_I64:
            case OP_DIV_U64: case OP_REM_U64:
            case OP_ADD_F32: case OP_SUB_F32: case OP_MUL_F32: case OP_DIV_F32:
            case OP_ADD_F64: case OP_SUB_F64: case OP_MUL_F64: case OP_DIV_F64:
            case OP_AND_I32: case OP_OR_I32:  case OP_XOR_I32:
            case OP_AND_I64: case OP_OR_I64:  case OP_XOR_I64:
            case OP_SHL_I32: case OP_SHR_I32: case OP_USHR_I32:
            case OP_SHL_I64: case OP_SHR_I64: case OP_USHR_I64:
            case OP_CMP_I32: case OP_CMP_I64: case OP_CMP_F32: case OP_CMP_F64: case OP_CMP_F32_GT: {
                uint16_t dst = 0, lhs = 0, rhs = 0;
                if (ntok < 4 || !vm_assm_parse_reg(tokens[1], &dst) ||
                    !vm_assm_parse_reg(tokens[2], &lhs) || !vm_assm_parse_reg(tokens[3], &rhs)) {
                    goto parse_err;
                }
                vm_assm_emit_u8(&res, (uint8_t)dst);
                vm_assm_emit_u8(&res, (uint8_t)lhs);
                vm_assm_emit_u8(&res, (uint8_t)rhs);
                break;
            }

            case OP_NEG_I32: case OP_NEG_I64: case OP_NEG_F32: case OP_NEG_F64:
            case OP_NOT_I32: case OP_NOT_I64:
            case OP_I32_TO_I8:  case OP_I32_TO_I16: case OP_I32_TO_I64:
            case OP_I32_TO_F32: case OP_I32_TO_F64: case OP_I64_TO_I32:
            case OP_I64_TO_F32: case OP_I64_TO_F64: case OP_F32_TO_I32:
            case OP_F32_TO_I64: case OP_F32_TO_F64: case OP_F64_TO_I32:
            case OP_F64_TO_I64: case OP_F64_TO_F32:
            case OP_LOAD8:  case OP_LOAD8S: case OP_LOAD16: case OP_LOAD16S:
            case OP_LOAD32: case OP_LOAD32S: case OP_LOAD64: case OP_LOAD_PTR:
            case OP_STORE8: case OP_STORE16: case OP_STORE32: case OP_STORE64: case OP_STORE_PTR: {
                uint16_t reg1 = 0, reg2 = 0;
                if (ntok < 3 || !vm_assm_parse_reg(tokens[1], &reg1) || !vm_assm_parse_reg(tokens[2], &reg2)) {
                    goto parse_err;
                }
                vm_assm_emit_u8(&res, (uint8_t)reg1);
                vm_assm_emit_u8(&res, (uint8_t)reg2);
                break;
            }

            case OP_LEA: {
                uint16_t dst = 0, base = 0; int64_t off = 0;
                if (ntok < 4 || !vm_assm_parse_reg(tokens[1], &dst) ||
                    !vm_assm_parse_reg(tokens[2], &base) || !vm_assm_parse_int64(tokens[3], &off)) {
                    goto parse_err;
                }
                vm_assm_emit_u8(&res, (uint8_t)dst);
                vm_assm_emit_u8(&res, (uint8_t)base);
                vm_assm_emit_u32(&res, (uint32_t)(int32_t)off);
                break;
            }

            case OP_GOTO: case OP_GOTO_16: case OP_GOTO_32: {
                if (ntok < 2) goto parse_err;
                int64_t offset = 0;
                size_t target_pc = 0;
                if (vm_assm_find_label(&labels, tokens[1], &target_pc)) {
                    size_t next_pc = current_pc + (size_t)inst_size;
                    offset = (int64_t)target_pc - (int64_t)next_pc;
                } else if (!vm_assm_parse_int64(tokens[1], &offset)) {
                    res.success = 0;
                    res.error_line = line_num;
                    snprintf(res.error_msg, sizeof(res.error_msg), "Undefined label or invalid offset '%s'", tokens[1]);
                    if (res.bytecode) { free(res.bytecode); }
                    res.bytecode = NULL;
                    return res;
                }
                if (op == OP_GOTO)          vm_assm_emit_u8(&res, (uint8_t)(int8_t)offset);
                else if (op == OP_GOTO_16)  vm_assm_emit_u16(&res, (uint16_t)(int16_t)offset);
                else                        vm_assm_emit_u32(&res, (uint32_t)(int32_t)offset);
                break;
            }

            case OP_IF_EQ: case OP_IF_NE: case OP_IF_LT:
            case OP_IF_GE: case OP_IF_GT: case OP_IF_LE: {
                uint16_t a = 0, b = 0;
                if (ntok < 4 || !vm_assm_parse_reg(tokens[1], &a) || !vm_assm_parse_reg(tokens[2], &b)) {
                    goto parse_err;
                }
                int64_t offset = 0;
                size_t target_pc = 0;
                if (vm_assm_find_label(&labels, tokens[3], &target_pc)) {
                    size_t next_pc = current_pc + (size_t)inst_size;
                    offset = (int64_t)target_pc - (int64_t)next_pc;
                } else if (!vm_assm_parse_int64(tokens[3], &offset)) {
                    res.success = 0;
                    res.error_line = line_num;
                    snprintf(res.error_msg, sizeof(res.error_msg), "Undefined label or invalid offset '%s'", tokens[3]);
                    if (res.bytecode) { free(res.bytecode); }
                    res.bytecode = NULL;
                    return res;
                }
                vm_assm_emit_u8(&res, (uint8_t)a);
                vm_assm_emit_u8(&res, (uint8_t)b);
                vm_assm_emit_u16(&res, (uint16_t)(int16_t)offset);
                break;
            }

            case OP_IF_EQZ: case OP_IF_NEZ: case OP_IF_LTZ:
            case OP_IF_GEZ: case OP_IF_GTZ: case OP_IF_LEZ: {
                uint16_t a = 0;
                if (ntok < 3 || !vm_assm_parse_reg(tokens[1], &a)) {
                    goto parse_err;
                }
                int64_t offset = 0;
                size_t target_pc = 0;
                if (vm_assm_find_label(&labels, tokens[2], &target_pc)) {
                    size_t next_pc = current_pc + (size_t)inst_size;
                    offset = (int64_t)target_pc - (int64_t)next_pc;
                } else if (!vm_assm_parse_int64(tokens[2], &offset)) {
                    res.success = 0;
                    res.error_line = line_num;
                    snprintf(res.error_msg, sizeof(res.error_msg), "Undefined label or invalid offset '%s'", tokens[2]);
                    if (res.bytecode) { free(res.bytecode); }
                    res.bytecode = NULL;
                    return res;
                }
                vm_assm_emit_u8(&res, (uint8_t)a);
                vm_assm_emit_u16(&res, (uint16_t)(int16_t)offset);
                break;
            }

            case OP_CALL: {
                uint16_t dst = 0; int64_t id = 0;
                if (ntok < 3 || !vm_assm_parse_reg(tokens[1], &dst) || !vm_assm_parse_int64(tokens[2], &id)) {
                    goto parse_err;
                }
                int start_arg_idx = 3;
                int argc = ntok - 3;
                if (ntok >= 4) {
                    int64_t explicit_argc = 0;
                    if (vm_assm_parse_int64(tokens[3], &explicit_argc)) {
                        if (explicit_argc == ntok - 4) {
                            argc = (int)explicit_argc;
                            start_arg_idx = 4;
                        }
                    }
                }
                vm_assm_emit_u8(&res, (uint8_t)dst);
                vm_assm_emit_u32(&res, (uint32_t)id);
                vm_assm_emit_u8(&res, (uint8_t)argc);
                for (int i = 0; i < argc; i++) {
                    uint16_t arg_reg = 0;
                    if (!vm_assm_parse_reg(tokens[start_arg_idx + i], &arg_reg)) {
                        goto parse_err;
                    }
                    vm_assm_emit_u8(&res, (uint8_t)arg_reg);
                }
                break;
            }

            case OP_CALL_VOID: {
                int64_t id = 0;
                if (ntok < 2 || !vm_assm_parse_int64(tokens[1], &id)) {
                    goto parse_err;
                }
                int start_arg_idx = 2;
                int argc = ntok - 2;
                if (ntok >= 3) {
                    int64_t explicit_argc = 0;
                    if (vm_assm_parse_int64(tokens[2], &explicit_argc)) {
                        if (explicit_argc == ntok - 3) {
                            argc = (int)explicit_argc;
                            start_arg_idx = 3;
                        }
                    }
                }
                vm_assm_emit_u32(&res, (uint32_t)id);
                vm_assm_emit_u8(&res, (uint8_t)argc);
                for (int i = 0; i < argc; i++) {
                    uint16_t arg_reg = 0;
                    if (!vm_assm_parse_reg(tokens[start_arg_idx + i], &arg_reg)) {
                        goto parse_err;
                    }
                    vm_assm_emit_u8(&res, (uint8_t)arg_reg);
                }
                break;
            }

            case OP_RETURN: {
                uint16_t src = 0;
                if (ntok < 2 || !vm_assm_parse_reg(tokens[1], &src)) {
                    goto parse_err;
                }
                vm_assm_emit_u8(&res, (uint8_t)src);
                break;
            }

            default:
                goto parse_err;
        }

        current_pc += (size_t)inst_size;
        continue;

    parse_err:
        res.success = 0;
        res.error_line = line_num;
        snprintf(res.error_msg, sizeof(res.error_msg), "Syntax error parsing line: '%s'", line);
        if (res.bytecode) free(res.bytecode);
        res.bytecode = NULL;
        return res;
    }

    return res;
}

/* Free assemble result bytecode */
static void vm_assemble_free(VMAssembleResult* res)
{
    if (res && res->bytecode) {
        free(res->bytecode);
        res->bytecode = NULL;
        res->size = 0;
        res->capacity = 0;
    }
}

/* Assemble to caller-provided memory buffer */
static VMError vm_assemble_to_buffer(const char* source_text, uint8_t* out_buf, size_t max_size,
                                     size_t* out_size, char* err_buf, size_t err_buf_size)
{
    VMAssembleResult res = vm_assemble(source_text);
    if (!res.success) {
        if (err_buf && err_buf_size > 0) {
            snprintf(err_buf, err_buf_size, "Line %d: %s", res.error_line, res.error_msg);
        }
        vm_assemble_free(&res);
        return VM_ERR_INVALID_OPCODE;
    }
    if (res.size > max_size) {
        if (err_buf && err_buf_size > 0) {
            snprintf(err_buf, err_buf_size, "Bytecode size %u exceeds buffer max %u", (unsigned)res.size, (unsigned)max_size);
        }
        vm_assemble_free(&res);
        return VM_ERR_OUT_OF_BOUNDS;
    }
    memcpy(out_buf, res.bytecode, res.size);
    if (out_size) *out_size = res.size;
    vm_assemble_free(&res);
    return VM_OK;
}

#ifdef __cplusplus
}
#endif

#ifdef _MSC_VER
#  pragma warning(pop)
#elif defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif

#endif /* VM_ASSEMBLER_H */
