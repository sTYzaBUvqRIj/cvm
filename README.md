# CVM — C/C++ Register VM

A small, portable, register-based virtual machine written in C99, designed to
execute bytecode generated from C/C++-level semantics.

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

---

## Overview

CVM is a minimal bytecode interpreter with a clean, embeddable API. It is designed
to be dropped into any C or C++ project as two files (`vm.h` and `vm.c`) and used
immediately — no build system integration, no external dependencies, no configuration headers.

The register file is an array of 64-bit unions, giving bytecode direct access to
every primitive C type (`int8_t` through `double` and `void*`) without any boxing
or marshalling overhead. Native C functions can be registered at runtime, letting
the VM call back into the host application for I/O, system calls, or anything else
not expressible in bytecode.

---

## Features

- **Two-file core implementation** — everything in `vm.h` + `vm.c`
- **Register-based** — no operand stack; all values live in a flat register file
- **Untyped 64-bit registers** — each register is a union of all C primitive types
- **~160 opcodes** covering:
  - Integer arithmetic and logic (i32 and i64, signed and unsigned)
  - Floating-point arithmetic (f32 and f64, IEEE-754)
  - Bitwise operations and shifts (arithmetic and logical)
  - Three-way comparisons (`-1 / 0 / +1`, including ordered/unordered float variants)
  - **Unsigned comparisons** (`CMP_U32`, `CMP_U64`) and **unsigned branches** (`IF_ULT/UGE/UGT/ULE`)
  - All type conversions between i8, i16, i32, i64, f32, f64
  - Memory loads and stores (8/16/32/64-bit, signed and unsigned, with/without immediate offsets)
  - **Load/Store with immediate offset** (`LOAD*_OFF`, `STORE*_OFF`) — 8 bytes, struct field access
  - Pointer loads, pointer stores, LEA (immediate offset) and **LEA_REG** (register offset)
  - Conditional branches — 12 signed variants + 4 unsigned variants; vs-register and vs-zero
  - Unconditional branches (3 range sizes: i8, i16, i32 offset)
  - Native function calls with up to 64 arguments
  - Bytecode subroutines (`OP_CALL_BC` / `OP_RET`) with register frame save/restore
  - **SELECT** — branchless conditional (`dst = cond ? a : b`)
  - **Bit manipulation** — CLZ/CTZ/POPCNT (i32 and i64), ROTL/ROTR (i32 and i64)
  - **Integer ABS/MIN/MAX** — signed and unsigned variants for i32 and i64
  - **MULH** — high half of 64-bit/128-bit multiply (signed and unsigned, i32 and i64)
  - **BOOL** — normalize any integer to 0 or 1
  - **Float intrinsics** — ABS, SQRT, FLOOR, CEIL, TRUNC, ROUND (f32 and f64)
  - **Float binary** — MIN, MAX, COPYSIGN (f32 and f64)
  - **MEMCPY / MEMSET** — bulk memory operations driven by register operands
  - **SWITCH** — O(1) table dispatch with default case
- **Minimal call-frame architecture** — compact 16-byte `VMFrame`; shared register storage via windowing; no per-frame register copies
- **Two-pass Assembler (`vm_assembler.h`)** — converts text assembly (`.cvma`) to binary bytecode (`.cvmb`)
- **Disassembler (`vm_disassembler.h`)** — decodes binary bytecode into human-readable text assembly
- **Host integration** — pre-load registers with host pointers; read results back
- **File format** — `.cvmb` binary format (16-byte header + raw bytecode)
- **Bytecode builder** — `vm_builder.h` provides a full emit API with a lightweight test framework
- **Standard library** — `vm_loader.h` provides standard native functions and `.cvmb` I/O helpers
- **CLI tools** — `vm_run` (executes `.cvmb` files) and `cvma2cvmb` (compiles `.cvma` to `.cvmb`)
- **C99 compatible** — compiles cleanly with `-std=c99 -Wall -Wextra -Wpedantic`
- **Optional debug tracing** — compile with `-DVM_DEBUG`; enable per-context at runtime
- **Built-in profiler** — per-opcode execution counters; `--profile` report in `vm_run`

---

## File Structure

```
cvm/
├── vm.h                        # Public API — opcodes, types, VMContext, vm_execute()
├── vm.c                        # VM implementation — dispatch loop
├── vm_builder.h                # Bytecode construction helpers (emit_* functions + test framework)
├── vm_loader.h                 # .cvmb file format, standard native function library
├── vm_assembler.h              # Two-pass assembler (.cvma text → bytecode)
├── vm_disassembler.h           # Bytecode disassembler (bytecode → assembly text)
├── DOCUMENTATION.md            # Complete opcode & API reference
├── README.md                   # This file
├── LICENSE                     # GNU General Public License v3.0
├── example/                    # C API test & example programs (27 standalone C files)
│   ├── Makefile                # Build system for all C examples
│   ├── README.md               # C example suite documentation
│   ├── vm_run.c                # CLI runner source
│   ├── basic.c                 # Constants, register moves, NOP, return
│   ├── arithmetic.c            # Integer arithmetic (i32/i64) & negation
│   ├── floating_point.c        # Float arithmetic (f32/f64) & IEEE edge cases
│   ├── comparison.c            # Three-way comparisons (signed & float)
│   ├── branching.c             # Conditional/unconditional jumps & loops
│   ├── bitwise.c               # Bitwise AND/OR/XOR/NOT operations
│   ├── shifts.c                # SHL/SHR/USHR shift operations
│   ├── conversions.c           # All 14 scalar type conversions
│   ├── memory.c                # LOAD*/STORE* operations (all widths)
│   ├── pointers.c              # LEA & pointer dereference patterns
│   ├── native_functions.c      # Native C host callback dispatch
│   ├── error_handling.c        # Trapping & error code tests
│   ├── loops.c                 # Structured loop patterns & algorithms
│   ├── fibonacci.c             # Fibonacci sequence generators
│   ├── factorial.c             # Factorial calculations
│   ├── complex.c               # Calculator, sorting, matrix, stats
│   ├── file_execution.c        # Binary serialization & loading
│   ├── assembler_disassembler_demo.c # Assembler/disassembler round-trip
│   ├── unsigned_ops.c          # Unsigned division/remainder & CMP_F32_GT
│   ├── single_step.c           # Single-stepping & VMContext state control
│   ├── coroutines.c            # Generator/Coroutine pattern via single-step
│   ├── benchmark.c             # High-throughput opcode execution benchmark
│   ├── debugger_profiler.c     # Debugger hooks, breakpoints & profiler report
│   ├── subroutines.c           # Subroutines, recursion & call stack frames
│   ├── hash_algorithms.c       # DJB2, FNV-1a 32-bit & FNV-1a 64-bit hashing
│   └── extended_ops.c          # Extended opcodes — bit ops, float intrinsics, SWITCH (116 tests)
└── example2/                   # Text Assembly (.cvma) programs (24 files)
    ├── Makefile                # Automates compilation, assembly, and execution
    ├── README.md               # Assembly workflow documentation
    ├── cvma2cvmb.c             # Text assembly compiler tool (.cvma → .cvmb)
    ├── basic.cvma              # Basic constants, register moves, and return
    ├── arithmetic.cvma         # Integer and floating-point arithmetic
    ├── control_flow.cvma       # Branches, loops, and label resolution
    ├── native_calls.cvma       # Native host function calling
    ├── bitwise_shifts.cvma     # Bitwise operations (AND/OR/XOR) and bit shifts
    ├── conversions.cvma        # Type conversions between int32 and double
    ├── floating_point.cvma     # Double precision float calculations
    ├── comparison.cvma         # Three-way comparisons (-1, 0, +1)
    ├── fibonacci.cvma          # Iterative Fibonacci calculation loop
    ├── factorial.cvma          # Factorial calculation loop
    ├── complex_algo.cvma       # Euclidean GCD algorithm in assembly
    ├── unsigned_ops.cvma       # Unsigned division/remainder and CMP_F32_GT
    ├── collatz.cvma            # Collatz (3n+1) sequence stopping time algorithm
    ├── binary_search.cvma      # Binary search over sorted array elements
    ├── subroutines.cvma        # Recursive assembly subroutines (CALL_BC / RET)
    ├── matrix_operations.cvma  # Subroutines, LEA, dot product & matrix trace
    ├── sorting_and_stats.cvma  # Type conversions, double division & statistical mean
    ├── pseudo_random_hash.cvma # PRNG LCG algorithm, bitwise hash pipeline & bit shifts
    ├── hash_algorithms.cvma    # DJB2, FNV-1a 32-bit & FNV-1a 64-bit assembly subroutines
    ├── bit_ops.cvma            # CLZ/CTZ/POPCNT, ROTL/ROTR, ABS, BOOL (i32 & i64)
    ├── float_intrinsics.cvma   # ABS/SQRT/FLOOR/CEIL/TRUNC/ROUND, MIN/MAX/COPYSIGN, SELECT
    ├── int_extended.cvma       # MIN/MAX signed/unsigned, MULH, CMP_U32/U64, IF_ULT/UGE/UGT/ULE
    ├── switch_demo.cvma        # SWITCH dispatch — day-of-week, score-grade, season tables
    └── extended_algorithms.cvma # Complex programs combining many extended opcodes
```

---

## Quick Start

### Embedding the VM

Copy `vm.h` and `vm.c` into your project. No other files are required.

```c
#include "vm.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    /* 1. Build bytecode manually or with vm_builder.h */
    static const uint8_t code[] = {
        /* CONST_I32 r0, 42 */
        0x04, 0x00,              /* opcode OP_CONST_I32 (little-endian u16) */
        0x00,                    /* dst = r0 */
        0x2A, 0x00, 0x00, 0x00, /* val = 42 (little-endian i32) */
        /* RETURN r0 */
        0x5E, 0x00,              /* opcode OP_RETURN */
        0x00,                    /* src = r0 */
    };

    /* 2. Set up context and register file */
    VMContext  ctx;
    VMRegister regs[4];
    vm_init(&ctx);
    memset(regs, 0, sizeof(regs));

    /* 3. Execute */
    VMError err = vm_execute(&ctx, regs, 4, 0, code, sizeof(code));

    /* 4. Read result */
    if (err == VM_OK)
        printf("result = %d\n", ctx.result.i32);  /* prints: result = 42 */

    return 0;
}
```

Compile:
```sh
gcc -std=c99 -o myprogram myprogram.c vm.c
```

---

### Using the Bytecode Builder (`vm_builder.h`)

```c
#include "vm_builder.h"

int main(void) {
    VMContext ctx;
    Bytecode  bc;

    vm_init(&ctx);
    bc_init(&bc);

    /* (a + b) * c  where a=10, b=20, c=3  →  90 */
    emit_const_i32(&bc, 0, 10);
    emit_const_i32(&bc, 1, 20);
    emit_add_i32  (&bc, 2, 0, 1);   /* r2 = r0 + r1 = 30 */
    emit_const_i32(&bc, 3, 3);
    emit_mul_i32  (&bc, 4, 2, 3);   /* r4 = r2 * r3 = 90 */
    emit_return   (&bc, 4);

    bc_run(&ctx, &bc, 8);
    printf("result = %d\n", ctx.result.i32);  /* 90 */
    return 0;
}
```

---

### Assembling and Disassembling Code

```c
#include "vm_assembler.h"
#include "vm_disassembler.h"

int main(void) {
    const char* asm_source =
        "CONST_I32 r0, 100\n"
        "CONST_I32 r1, 200\n"
        "ADD_I32   r2, r0, r1\n"
        "RETURN    r2\n";

    /* Assemble text → bytecode */
    VMAssembleResult res = vm_assemble(asm_source);
    if (res.success) {
        printf("Assembled %u bytes.\n", (unsigned)res.size);

        /* Disassemble bytecode → string */
        char* text = vm_disassemble(res.bytecode, res.size);
        printf("Disassembly:\n%s\n", text);
        free(text);
    }
    vm_assemble_free(&res);
    return 0;
}
```

---

### Assembling `.cvma` Text Files via CLI

```sh
cd example2/
make                                       # Compile cvma2cvmb & assemble all .cvma files
./cvma2cvmb fibonacci.cvma -o fibonacci.cvmb --verbose
../example/vm_run fibonacci.cvmb
```

---

## API Summary

### Core (`vm.h`)

| Function | Description |
|----------|-------------|
| `vm_init(ctx)` | Zero-initialise a `VMContext` before use |
| `vm_destroy(ctx)` | Release any resources held by `VMContext` |
| `vm_register_function(ctx, id, fn)` | Register a native C function at slot `id` (0–255) |
| `vm_execute(ctx, regs, reg_count, pc, code, size)` | Execute bytecode; returns `VMError` |

### Bytecode Builder (`vm_builder.h`)

| Helper | Description |
|--------|-------------|
| `bc_init(bc)` | Initialise a `Bytecode` buffer |
| `bc_free(bc)` | Release the buffer |
| `emit_nop(bc)` | Emit `OP_NOP` |
| `emit_const_i32(bc, dst, val)` | Load 32-bit integer constant |
| `emit_const_i64(bc, dst, val)` | Load 64-bit integer constant |
| `emit_const_f32/f64(bc, dst, val)` | Load float constant |
| `emit_add_i32(bc, dst, lhs, rhs)` | Integer add (and all arithmetic/logic variants) |
| `emit_if_lt(bc, a, b)` | Conditional branch — returns forward-patch site |
| `emit_if_ult(bc, a, b)` | Unsigned conditional branch |
| `emit_select(bc, dst, a, b, cond)` | Branchless conditional select |
| `emit_clz_i32/i64(bc, dst, src)` | Count leading zeros |
| `emit_popcnt_i32/i64(bc, dst, src)` | Population count |
| `emit_rotl_i32/i64(bc, dst, val, amt)` | Rotate left |
| `emit_min_i32/max_i32(bc, dst, a, b)` | Integer min/max (signed/unsigned variants) |
| `emit_mulh_i32/i64(bc, dst, a, b)` | High-half multiply |
| `emit_abs_f32/f64(bc, dst, src)` | Float absolute value |
| `emit_sqrt_f32/f64(bc, dst, src)` | Float square root |
| `emit_floor/ceil/trunc/round_f32(bc, dst, src)` | Float rounding |
| `emit_min/max/copysign_f32(bc, dst, a, b)` | Float binary intrinsics |
| `emit_load32s_off(bc, dst, base, offset)` | Load with immediate offset |
| `emit_store32_off(bc, addr, src, offset)` | Store with immediate offset |
| `emit_lea_reg(bc, dst, base, idx)` | Variable-index pointer arithmetic |
| `emit_memcpy/memset(bc, dst, src, len)` | Bulk memory operations |
| `emit_switch_header(bc, reg, count)` | Emit SWITCH header, returns patch site |
| `bc_patch_here(bc, site)` | Resolve a forward branch to current position |
| `bc_patch_back(bc, site, target)` | Resolve a backward branch |
| `bc_patch_switch(bc, slots, idx, target, switch_end)` | Patch a SWITCH table entry |
| `bc_run(ctx, bc, reg_count)` | Execute the built bytecode |
| `bc_run_regs(ctx, bc, regs, reg_count)` | Execute with a pre-populated register file |

### Assembler (`vm_assembler.h`)

| Function | Description |
|----------|-------------|
| `vm_assemble(source_text)` | Assemble text source code to binary bytecode result |
| `vm_assemble_free(res)` | Free bytecode buffer inside result |
| `vm_assemble_to_buffer(...)` | Assemble directly into caller-allocated buffer |

### Disassembler (`vm_disassembler.h`)

| Function | Description |
|----------|-------------|
| `vm_opcode_name(op)` | Return the mnemonic string for an opcode |
| `vm_disassemble_instruction(buf, size, pc, out, out_size)` | Disassemble a single instruction |
| `vm_disassemble(code, size)` | Disassemble entire bytecode buffer into an allocated string |
| `vm_disassemble_file(stream, code, size)` | Disassemble and print entire bytecode to `FILE*` |

### File Format (`vm_loader.h`)

| Function | Description |
|----------|-------------|
| `cvmb_write(path, code, size, reg_count, entry)` | Write a `.cvmb` file |
| `cvmb_read(path, &hdr)` | Load a `.cvmb` file into a heap buffer |
| `cvmb_free(code)` | Free a buffer returned by `cvmb_read` |
| `vm_register_stdlib(ctx)` | Register the standard native functions |

---

## Error Codes

| Code | Constant | Cause |
|------|----------|-------|
| 0 | `VM_OK` | Success / clean exit |
| 1 | `VM_ERR_INVALID_OPCODE` | Unknown 16-bit opcode encountered |
| 2 | `VM_ERR_OUT_OF_BOUNDS` | PC or branch target outside bytecode buffer |
| 3 | `VM_ERR_DIV_ZERO` | Integer division or remainder by zero |
| 4 | `VM_ERR_INVALID_REGISTER` | Register index >= reg_count |
| 5 | `VM_ERR_BAD_FUNCTION` | Unregistered or null native function id |
| 6 | `VM_ERR_BAD_ARGC` | More than 64 arguments passed to CALL |
| 7 | `VM_ERR_STACK_OVERFLOW` | Recursion depth exceeded maximum call depth |

---

## License

This project is licensed under the **GNU General Public License v3.0** (GPLv3).
See the [`LICENSE`](LICENSE) file for details.

Copyright (C) 2026 CVM Contributors.

---

## TODO

### Core VM

- [x] **Call stack / subroutines** — `OP_CALL_BC` and `OP_RET` for calling bytecode subroutines with recursion support
- [x] **Minimal call frames** — compact 16-byte `VMFrame`; shared register storage via windowing (no per-frame register copies)
- [x] **Unsigned integer support** — `DIV_U32`, `REM_U32`, `DIV_U64`, `REM_U64`
- [x] **Unsigned comparisons & branches** — `CMP_U32/U64`, `IF_ULT/UGE/UGT/ULE`
- [x] **32-bit float comparisons** — `CMP_F32_GT` ordered variant
- [x] **SELECT** — branchless conditional select opcode
- [x] **Bit manipulation** — CLZ/CTZ/POPCNT, ROTL/ROTR for i32 and i64
- [x] **Integer ABS/MIN/MAX** — signed and unsigned variants for i32 and i64
- [x] **MULH** — high-half multiply for i32 and i64 (signed and unsigned)
- [x] **BOOL** — normalize integer to 0/1
- [x] **Float intrinsics** — ABS, SQRT, FLOOR, CEIL, TRUNC, ROUND, MIN, MAX, COPYSIGN (f32 and f64)
- [x] **Load/Store with offset** — `LOAD*_OFF` / `STORE*_OFF` with 32-bit immediate offset
- [x] **LEA_REG** — variable-index pointer arithmetic
- [x] **MEMCPY / MEMSET** — bulk memory operations
- [x] **SWITCH** — O(1) table dispatch opcode with default case
- [ ] **Wide register indices** — CALL arg lists use u8 register indices; consider u16 for full 0–255 range

### Bytecode & Tooling

- [x] **Assembler** — two-pass assembler (`vm_assembler.h`) and CLI compiler (`cvma2cvmb`)
- [x] **Disassembler** — disassembler library (`vm_disassembler.h`) for decoding bytecode
- [x] **Debugger interface** — step/breakpoint callback hooks (`VMDebugHook`, `breakpoints`) in `VMContext`
- [x] **Profiler** — per-opcode execution counters and `--profile` report mode in `vm_run`
- [ ] **Constant pool** — dedicated section in `.cvmb` for string literals and large constants
- [ ] **Symbol table** — optional `.cvmb` section for debug info (function names, source line mappings)
- [ ] **Magic version validation** — `cvmb_read` should reject unknown version bytes
- [ ] **`vm_run` — argument passing** — allow passing initial register values via CLI (e.g. `--r0=42`)
- [ ] **`vm_run` — exit code** — propagate `ctx.result.i32` as the process exit code

### Future Roadmap & Advanced Features

- [ ] **JIT Compilation Engine** — translate VM bytecode into native machine instructions (x86-64 / AArch64)
- [ ] **Garbage Collection System** — mark-and-sweep or generational GC for dynamic heap objects
- [ ] **Multi-threading / Async Fiber Support** — lightweight cooperative fibers and channel-based communication
- [ ] **Debug Adapter Protocol (DAP)** — standard DAP server for full visual interactive debugging in VS Code / IDEs
- [ ] **Source Maps & Debug Symbols (`.cvmdb`)** — source line mappings and variable name lookup for debuggers
- [ ] **Foreign Function Interface (FFI)** — dynamic `.so`/`.dll` loader and C ABI marshalling
- [ ] **SIMD Vector Extensions** — 128-bit packed vector registers and parallel arithmetic opcodes

### Performance & Testing

- [ ] **Computed goto dispatch** — replace `switch` with `&&label` table for ~10–30% throughput improvement
- [ ] **Fuzz testing** — libFuzzer / AFL integration for bytecode parsing and execution
- [ ] **Cross-platform CI** — automated build and test on Linux, macOS, and Windows
