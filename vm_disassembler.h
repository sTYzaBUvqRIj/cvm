/*
 * Copyright (C) 2026 CVM Contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * vm_disassembler.h  —  Bytecode Disassembler for C/C++ Register VM
 *
 * Header-only library for disassembling CVM binary bytecode into
 * human-readable assembly text format.
 */

#ifndef VM_DISASSEMBLER_H
#define VM_DISASSEMBLER_H

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
#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Options for disassembly formatting. */
typedef struct {
    int show_addresses; /* 1: include "0000: " offset prefix */
    int show_hex_bytes; /* 1: include raw hex bytes */
} VMDisasmOptions;

/* Return string mnemonic for a VMOpcode. */
static const char* vm_opcode_name(VMOpcode op)
{
    switch (op) {
        case OP_NOP:          return "NOP";
        case OP_MOVE:         return "MOVE";
        case OP_CONST_I8:     return "CONST_I8";
        case OP_CONST_I16:    return "CONST_I16";
        case OP_CONST_I32:    return "CONST_I32";
        case OP_CONST_I64:    return "CONST_I64";
        case OP_CONST_F32:    return "CONST_F32";
        case OP_CONST_F64:    return "CONST_F64";
        case OP_ADD_I32:      return "ADD_I32";
        case OP_SUB_I32:      return "SUB_I32";
        case OP_MUL_I32:      return "MUL_I32";
        case OP_DIV_I32:      return "DIV_I32";
        case OP_REM_I32:      return "REM_I32";
        case OP_ADD_I64:      return "ADD_I64";
        case OP_SUB_I64:      return "SUB_I64";
        case OP_MUL_I64:      return "MUL_I64";
        case OP_DIV_I64:      return "DIV_I64";
        case OP_REM_I64:      return "REM_I64";
        case OP_DIV_U32:      return "DIV_U32";
        case OP_REM_U32:      return "REM_U32";
        case OP_DIV_U64:      return "DIV_U64";
        case OP_REM_U64:      return "REM_U64";
        case OP_ADD_F32:      return "ADD_F32";
        case OP_SUB_F32:      return "SUB_F32";
        case OP_MUL_F32:      return "MUL_F32";
        case OP_DIV_F32:      return "DIV_F32";
        case OP_ADD_F64:      return "ADD_F64";
        case OP_SUB_F64:      return "SUB_F64";
        case OP_MUL_F64:      return "MUL_F64";
        case OP_DIV_F64:      return "DIV_F64";
        case OP_NEG_I32:      return "NEG_I32";
        case OP_NEG_I64:      return "NEG_I64";
        case OP_NEG_F32:      return "NEG_F32";
        case OP_NEG_F64:      return "NEG_F64";
        case OP_NOT_I32:      return "NOT_I32";
        case OP_NOT_I64:      return "NOT_I64";
        case OP_AND_I32:      return "AND_I32";
        case OP_OR_I32:       return "OR_I32";
        case OP_XOR_I32:      return "XOR_I32";
        case OP_AND_I64:      return "AND_I64";
        case OP_OR_I64:       return "OR_I64";
        case OP_XOR_I64:      return "XOR_I64";
        case OP_SHL_I32:      return "SHL_I32";
        case OP_SHR_I32:      return "SHR_I32";
        case OP_USHR_I32:     return "USHR_I32";
        case OP_SHL_I64:      return "SHL_I64";
        case OP_SHR_I64:      return "SHR_I64";
        case OP_USHR_I64:     return "USHR_I64";
        case OP_CMP_I32:      return "CMP_I32";
        case OP_CMP_I64:      return "CMP_I64";
        case OP_CMP_F32:      return "CMP_F32";
        case OP_CMP_F64:      return "CMP_F64";
        case OP_CMP_F32_GT:   return "CMP_F32_GT";
        case OP_I32_TO_I8:    return "I32_TO_I8";
        case OP_I32_TO_I16:   return "I32_TO_I16";
        case OP_I32_TO_I64:   return "I32_TO_I64";
        case OP_I32_TO_F32:   return "I32_TO_F32";
        case OP_I32_TO_F64:   return "I32_TO_F64";
        case OP_I64_TO_I32:   return "I64_TO_I32";
        case OP_I64_TO_F32:   return "I64_TO_F32";
        case OP_I64_TO_F64:   return "I64_TO_F64";
        case OP_F32_TO_I32:   return "F32_TO_I32";
        case OP_F32_TO_I64:   return "F32_TO_I64";
        case OP_F32_TO_F64:   return "F32_TO_F64";
        case OP_F64_TO_I32:   return "F64_TO_I32";
        case OP_F64_TO_I64:   return "F64_TO_I64";
        case OP_F64_TO_F32:   return "F64_TO_F32";
        case OP_LOAD8:        return "LOAD8";
        case OP_LOAD8S:       return "LOAD8S";
        case OP_LOAD16:       return "LOAD16";
        case OP_LOAD16S:      return "LOAD16S";
        case OP_LOAD32:       return "LOAD32";
        case OP_LOAD32S:      return "LOAD32S";
        case OP_LOAD64:       return "LOAD64";
        case OP_LOAD_PTR:     return "LOAD_PTR";
        case OP_STORE8:       return "STORE8";
        case OP_STORE16:      return "STORE16";
        case OP_STORE32:      return "STORE32";
        case OP_STORE64:      return "STORE64";
        case OP_STORE_PTR:    return "STORE_PTR";
        case OP_LEA:          return "LEA";
        case OP_GOTO:         return "GOTO";
        case OP_GOTO_16:      return "GOTO_16";
        case OP_GOTO_32:      return "GOTO_32";
        case OP_IF_EQ:        return "IF_EQ";
        case OP_IF_NE:        return "IF_NE";
        case OP_IF_LT:        return "IF_LT";
        case OP_IF_GE:        return "IF_GE";
        case OP_IF_GT:        return "IF_GT";
        case OP_IF_LE:        return "IF_LE";
        case OP_IF_EQZ:       return "IF_EQZ";
        case OP_IF_NEZ:       return "IF_NEZ";
        case OP_IF_LTZ:       return "IF_LTZ";
        case OP_IF_GEZ:       return "IF_GEZ";
        case OP_IF_GTZ:       return "IF_GTZ";
        case OP_IF_LEZ:       return "IF_LEZ";
        case OP_CALL:         return "CALL";
        case OP_CALL_VOID:    return "CALL_VOID";
        case OP_RETURN_VOID:  return "RETURN_VOID";
        case OP_RETURN:       return "RETURN";
        case OP_CALL_BC:      return "CALL_BC";
        case OP_RET:          return "RET";
        default:              return "UNKNOWN";
    }
}

/* Internal helpers to read little-endian integers */
static uint16_t vm_disasm_u16(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t vm_disasm_u32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint64_t vm_disasm_u64(const uint8_t* p) {
    return (uint64_t)vm_disasm_u32(p) | ((uint64_t)vm_disasm_u32(p + 4) << 32);
}

/*
 * Disassemble a single instruction at byte offset 'pc' in 'code'.
 * Accepts custom formatting options via 'opts'.
 * Returns bytes consumed (> 0) on success, or <= 0 on error / OOB.
 */
static int vm_disassemble_instruction_ext(const uint8_t* code, size_t size, size_t pc,
                                          char* out_buf, size_t buf_size,
                                          const VMDisasmOptions* opts)
{
    if (!code || pc + 2 > size || !out_buf || buf_size == 0) {
        return 0;
    }

    uint16_t op_val = vm_disasm_u16(code + pc);
    VMOpcode op = (VMOpcode)op_val;
    const char* op_str = vm_opcode_name(op);

    char inst_text[256];
    inst_text[0] = '\0';

    int inst_len = 0;

    switch (op) {
        case OP_NOP: {
            snprintf(inst_text, sizeof(inst_text), "NOP");
            inst_len = 2;
            break;
        }
        case OP_MOVE: {
            if (pc + 5 > size) return 0;
            uint8_t dst = code[pc + 2];
            uint16_t src = vm_disasm_u16(code + pc + 3);
            snprintf(inst_text, sizeof(inst_text), "MOVE r%u, r%u", dst, src);
            inst_len = 5;
            break;
        }
        case OP_CONST_I8: {
            if (pc + 4 > size) return 0;
            uint8_t dst = code[pc + 2];
            int8_t val = (int8_t)code[pc + 3];
            snprintf(inst_text, sizeof(inst_text), "CONST_I8 r%u, %d", dst, (int)val);
            inst_len = 4;
            break;
        }
        case OP_CONST_I16: {
            if (pc + 5 > size) return 0;
            uint8_t dst = code[pc + 2];
            int16_t val = (int16_t)vm_disasm_u16(code + pc + 3);
            snprintf(inst_text, sizeof(inst_text), "CONST_I16 r%u, %d", dst, (int)val);
            inst_len = 5;
            break;
        }
        case OP_CONST_I32: {
            if (pc + 7 > size) return 0;
            uint8_t dst = code[pc + 2];
            int32_t val = (int32_t)vm_disasm_u32(code + pc + 3);
            snprintf(inst_text, sizeof(inst_text), "CONST_I32 r%u, %d", dst, (int)val);
            inst_len = 7;
            break;
        }
        case OP_CONST_I64: {
            if (pc + 11 > size) return 0;
            uint8_t dst = code[pc + 2];
            int64_t val = (int64_t)vm_disasm_u64(code + pc + 3);
            snprintf(inst_text, sizeof(inst_text), "CONST_I64 r%u, %" PRId64, dst, val);
            inst_len = 11;
            break;
        }
        case OP_CONST_F32: {
            if (pc + 7 > size) return 0;
            uint8_t dst = code[pc + 2];
            uint32_t bits = vm_disasm_u32(code + pc + 3);
            float f;
            memcpy(&f, &bits, sizeof(float));
            snprintf(inst_text, sizeof(inst_text), "CONST_F32 r%u, %.9g", dst, (double)f);
            inst_len = 7;
            break;
        }
        case OP_CONST_F64: {
            if (pc + 11 > size) return 0;
            uint8_t dst = code[pc + 2];
            uint64_t bits = vm_disasm_u64(code + pc + 3);
            double d;
            memcpy(&d, &bits, sizeof(double));
            snprintf(inst_text, sizeof(inst_text), "CONST_F64 r%u, %.17g", dst, d);
            inst_len = 11;
            break;
        }
        /* 3-operand register instructions: [op:u16][dst:u8][lhs:u8][rhs:u8] */
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
            if (pc + 5 > size) return 0;
            uint8_t dst = code[pc + 2];
            uint8_t lhs = code[pc + 3];
            uint8_t rhs = code[pc + 4];
            snprintf(inst_text, sizeof(inst_text), "%s r%u, r%u, r%u", op_str, dst, lhs, rhs);
            inst_len = 5;
            break;
        }
        /* Unary & Type conversions: [op:u16][dst:u8][src:u8] */
        case OP_NEG_I32: case OP_NEG_I64: case OP_NEG_F32: case OP_NEG_F64:
        case OP_NOT_I32: case OP_NOT_I64:
        case OP_I32_TO_I8:  case OP_I32_TO_I16: case OP_I32_TO_I64:
        case OP_I32_TO_F32: case OP_I32_TO_F64: case OP_I64_TO_I32:
        case OP_I64_TO_F32: case OP_I64_TO_F64: case OP_F32_TO_I32:
        case OP_F32_TO_I64: case OP_F32_TO_F64: case OP_F64_TO_I32:
        case OP_F64_TO_I64: case OP_F64_TO_F32: {
            if (pc + 4 > size) return 0;
            uint8_t dst = code[pc + 2];
            uint8_t src = code[pc + 3];
            snprintf(inst_text, sizeof(inst_text), "%s r%u, r%u", op_str, dst, src);
            inst_len = 4;
            break;
        }
        /* Memory loads: [op:u16][dst:u8][addr:u8] */
        case OP_LOAD8:  case OP_LOAD8S: case OP_LOAD16: case OP_LOAD16S:
        case OP_LOAD32: case OP_LOAD32S: case OP_LOAD64: case OP_LOAD_PTR: {
            if (pc + 4 > size) return 0;
            uint8_t dst  = code[pc + 2];
            uint8_t addr = code[pc + 3];
            snprintf(inst_text, sizeof(inst_text), "%s r%u, [r%u]", op_str, dst, addr);
            inst_len = 4;
            break;
        }
        /* Memory stores: [op:u16][addr:u8][src:u8] */
        case OP_STORE8:  case OP_STORE16: case OP_STORE32:
        case OP_STORE64: case OP_STORE_PTR: {
            if (pc + 4 > size) return 0;
            uint8_t addr = code[pc + 2];
            uint8_t src  = code[pc + 3];
            snprintf(inst_text, sizeof(inst_text), "%s [r%u], r%u", op_str, addr, src);
            inst_len = 4;
            break;
        }
        /* Pointer arithmetic: LEA [dst:u8][base:u8][offset:i32] */
        case OP_LEA: {
            if (pc + 8 > size) return 0;
            uint8_t dst  = code[pc + 2];
            uint8_t base = code[pc + 3];
            int32_t off  = (int32_t)vm_disasm_u32(code + pc + 4);
            if (off >= 0) {
                snprintf(inst_text, sizeof(inst_text), "LEA r%u, [r%u + %d]", dst, base, (int)off);
            } else {
                snprintf(inst_text, sizeof(inst_text), "LEA r%u, [r%u - %d]", dst, base, (int)(-off));
            }
            inst_len = 8;
            break;
        }
        /* Unconditional branches */
        case OP_GOTO: {
            if (pc + 3 > size) return 0;
            int8_t off = (int8_t)code[pc + 2];
            snprintf(inst_text, sizeof(inst_text), "GOTO %d", (int)off);
            inst_len = 3;
            break;
        }
        case OP_GOTO_16: {
            if (pc + 4 > size) return 0;
            int16_t off = (int16_t)vm_disasm_u16(code + pc + 2);
            snprintf(inst_text, sizeof(inst_text), "GOTO_16 %d", (int)off);
            inst_len = 4;
            break;
        }
        case OP_GOTO_32: {
            if (pc + 6 > size) return 0;
            int32_t off = (int32_t)vm_disasm_u32(code + pc + 2);
            snprintf(inst_text, sizeof(inst_text), "GOTO_32 %d", (int)off);
            inst_len = 6;
            break;
        }
        /* Conditional branches (pair): [op:u16][A:u8][B:u8][offset:i16] */
        case OP_IF_EQ: case OP_IF_NE: case OP_IF_LT:
        case OP_IF_GE: case OP_IF_GT: case OP_IF_LE: {
            if (pc + 6 > size) return 0;
            uint8_t a   = code[pc + 2];
            uint8_t b   = code[pc + 3];
            int16_t off = (int16_t)vm_disasm_u16(code + pc + 4);
            snprintf(inst_text, sizeof(inst_text), "%s r%u, r%u, %d", op_str, a, b, (int)off);
            inst_len = 6;
            break;
        }
        /* Conditional branches (vs zero): [op:u16][A:u8][offset:i16] */
        case OP_IF_EQZ: case OP_IF_NEZ: case OP_IF_LTZ:
        case OP_IF_GEZ: case OP_IF_GTZ: case OP_IF_LEZ: {
            if (pc + 5 > size) return 0;
            uint8_t a   = code[pc + 2];
            int16_t off = (int16_t)vm_disasm_u16(code + pc + 3);
            snprintf(inst_text, sizeof(inst_text), "%s r%u, %d", op_str, a, (int)off);
            inst_len = 5;
            break;
        }
        /* CALL: [op:u16][dst:u8][id:u32][argc:u8][r0..rN:u8] */
        case OP_CALL: {
            if (pc + 8 > size) return 0;
            uint8_t dst  = code[pc + 2];
            uint32_t id  = vm_disasm_u32(code + pc + 3);
            uint8_t argc = code[pc + 7];
            if (pc + 8 + argc > size) return 0;

            int written = snprintf(inst_text, sizeof(inst_text), "CALL r%u, %u", dst, id);
            for (uint8_t i = 0; i < argc && written < (int)sizeof(inst_text) - 10; i++) {
                written += snprintf(inst_text + written, sizeof(inst_text) - (size_t)written, ", r%u", code[pc + 8 + i]);
            }
            inst_len = 8 + argc;
            break;
        }
        /* CALL_VOID: [op:u16][id:u32][argc:u8][r0..rN:u8] */
        case OP_CALL_VOID: {
            if (pc + 7 > size) return 0;
            uint32_t id  = vm_disasm_u32(code + pc + 2);
            uint8_t argc = code[pc + 6];
            if (pc + 7 + argc > size) return 0;

            int written = snprintf(inst_text, sizeof(inst_text), "CALL_VOID %u", id);
            for (uint8_t i = 0; i < argc && written < (int)sizeof(inst_text) - 10; i++) {
                written += snprintf(inst_text + written, sizeof(inst_text) - (size_t)written, ", r%u", code[pc + 7 + i]);
            }
            inst_len = 7 + argc;
            break;
        }
        /* OP_CALL_BC: [op:u16][dst:u8][target:u32][argc:u8][r0..rN:u8] */
        case OP_CALL_BC: {
            if (pc + 8 > size) return 0;
            uint8_t  dst  = code[pc + 2];
            uint32_t target_pc = vm_disasm_u32(code + pc + 3);
            uint8_t  argc = code[pc + 7];
            if (pc + 8 + argc > size) return 0;

            int written = snprintf(inst_text, sizeof(inst_text), "CALL_BC r%u, 0x%04X", dst, target_pc);
            for (uint8_t i = 0; i < argc && written < (int)sizeof(inst_text) - 10; i++) {
                written += snprintf(inst_text + written, sizeof(inst_text) - (size_t)written, ", r%u", code[pc + 8 + i]);
            }
            inst_len = 8 + argc;
            break;
        }
        /* OP_RET */
        case OP_RET: {
            if (pc + 3 > size) return 0;
            uint8_t src = code[pc + 2];
            if (src == 0xFF) {
                snprintf(inst_text, sizeof(inst_text), "RET VOID");
            } else {
                snprintf(inst_text, sizeof(inst_text), "RET r%u", src);
            }
            inst_len = 3;
            break;
        }
        /* Return */
        case OP_RETURN_VOID: {
            snprintf(inst_text, sizeof(inst_text), "RETURN_VOID");
            inst_len = 2;
            break;
        }
        case OP_RETURN: {
            if (pc + 3 > size) return 0;
            uint8_t src = code[pc + 2];
            snprintf(inst_text, sizeof(inst_text), "RETURN r%u", src);
            inst_len = 3;
            break;
        }
        default: {
            snprintf(inst_text, sizeof(inst_text), "UNKNOWN (0x%04X)", (unsigned)op_val);
            inst_len = 2;
            break;
        }
    }

    /* Format line into out_buf */
    size_t pos = 0;
    out_buf[0] = '\0';

    if (opts && opts->show_addresses) {
        pos += (size_t)snprintf(out_buf + pos, buf_size - pos, "%04X: ", (unsigned)pc);
    }

    if (opts && opts->show_hex_bytes) {
        char hex_str[64] = "";
        size_t hpos = 0;
        for (int i = 0; i < inst_len && i < 8; i++) {
            hpos += (size_t)snprintf(hex_str + hpos, sizeof(hex_str) - hpos, "%02X ", code[pc + i]);
        }
        if (inst_len > 8) {
            snprintf(hex_str + hpos, sizeof(hex_str) - hpos, ".. ");
        }
        pos += (size_t)snprintf(out_buf + pos, buf_size - pos, "%-20s ", hex_str);
    }

    snprintf(out_buf + pos, buf_size - pos, "%s", inst_text);

    return inst_len;
}

/*
 * Disassemble a single instruction with default options (includes address prefix).
 */
static int vm_disassemble_instruction(const uint8_t* code, size_t size, size_t pc,
                                       char* out_buf, size_t buf_size)
{
    VMDisasmOptions opts;
    opts.show_addresses = 1;
    opts.show_hex_bytes = 0;
    return vm_disassemble_instruction_ext(code, size, pc, out_buf, buf_size, &opts);
}

/*
 * Disassemble the entire bytecode buffer into a dynamically allocated string.
 * Caller must free() the returned string. Returns NULL on allocation failure.
 */
static char* vm_disassemble(const uint8_t* code, size_t size)
{
    if (!code || size == 0) return NULL;

    size_t cap = size * 32 + 256;
    char* buf = (char*)malloc(cap);
    if (!buf) return NULL;

    buf[0] = '\0';
    size_t buf_len = 0;

    VMDisasmOptions opts;
    opts.show_addresses = 1;
    opts.show_hex_bytes = 0;

    size_t pc = 0;
    while (pc < size) {
        char line[256];
        int len = vm_disassemble_instruction_ext(code, size, pc, line, sizeof(line), &opts);
        if (len <= 0) {
            snprintf(line, sizeof(line), "%04X: <invalid opcode at offset %u>", (unsigned)pc, (unsigned)pc);
            len = 2;
        }

        size_t line_len = strlen(line);
        if (buf_len + line_len + 2 >= cap) {
            cap *= 2;
            char* new_buf = (char*)realloc(buf, cap);
            if (!new_buf) {
                free(buf);
                return NULL;
            }
            buf = new_buf;
        }

        memcpy(buf + buf_len, line, line_len);
        buf_len += line_len;
        buf[buf_len++] = '\n';
        buf[buf_len] = '\0';

        pc += (size_t)len;
    }

    return buf;
}

/*
 * Disassemble the entire bytecode buffer and print to stream (e.g. stdout).
 */
static void vm_disassemble_file(FILE* stream, const uint8_t* code, size_t size)
{
    if (!stream || !code || size == 0) return;

    VMDisasmOptions opts;
    opts.show_addresses = 1;
    opts.show_hex_bytes = 0;

    size_t pc = 0;
    while (pc < size) {
        char line[256];
        int len = vm_disassemble_instruction_ext(code, size, pc, line, sizeof(line), &opts);
        if (len <= 0) {
            fprintf(stream, "%04X: <invalid opcode at offset %u>\n", (unsigned)pc, (unsigned)pc);
            pc += 2;
            continue;
        }
        fprintf(stream, "%s\n", line);
        pc += (size_t)len;
    }
}

#ifdef __cplusplus
}
#endif

#ifdef _MSC_VER
#  pragma warning(pop)
#elif defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif

#endif /* VM_DISASSEMBLER_H */
