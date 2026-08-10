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
- **102 opcodes** covering:
  - Integer arithmetic and logic (i32 and i64, signed and unsigned)
  - Floating-point arithmetic (f32 and f64, IEEE-754)
  - Bitwise operations and shifts (arithmetic and logical)
  - Three-way comparisons (`-1 / 0 / +1`, including ordered/unordered float variants)
  - All type conversions between i8, i16, i32, i64, f32, f64
  - Memory loads and stores (8 / 16 / 32 / 64-bit, signed and unsigned)
  - Pointer loads, pointer stores, and LEA (load effective address)
  - Conditional branches (12 variants: register-pair and vs-zero)
  - Unconditional branches (3 range sizes: i8, i16, i32 offset)
  - Native function calls with up to 64 arguments
  - Bytecode subroutines (`OP_CALL_BC` / `OP_RET`) & recursion with register frame save/restore
- **Two-pass Assembler (`vm_assembler.h`)** — converts text assembly (`.cvma`) to binary bytecode (`.cvmb`)
- **Disassembler (`vm_disassembler.h`)** — decodes binary bytecode into human-readable text assembly
- **Host integration** — pre-load registers with host pointers; read results back
- **File format** — `.cvmb` binary format (16-byte header + raw bytecode)
- **Bytecode builder** — `vm_builder.h` provides a simple emit API for writing programs in C
- **Standard library** — `vm_loader.h` provides 12 ready-to-use native functions
- **CLI tools** — `vm_run` (executes `.cvmb` files) and `cvma2cvmb` (compiles `.cvma` to `.cvmb`)
- **C99 compatible** — compiles cleanly with `-std=c99 -Wall -Wextra -Wpedantic`
- **Optional debug tracing** — compile with `-DVM_DEBUG`; enable per-context at runtime

---

## File Structure

```
cvm/
├── vm.h              Public API — opcodes, types, VMContext, vm_execute()
├── vm.c              VM implementation — dispatch loop
├── vm_builder.h      Bytecode construction helpers (emit_* functions)
├── vm_loader.h       .cvmb file format + standard native function library
├── vm_assembler.h    Two-pass assembler (.cvma assembly text -> bytecode)
├── vm_disassembler.h Bytecode disassembler (bytecode -> assembly text)
├── DOCUMENTATION.md  Complete opcode reference
├── README.md         This file
├── LICENSE           GNU General Public License v3.0
├── example/          C API Example Suite
│   ├── Makefile          Build system for all examples
│   ├── README.md         Example-specific documentation
│   ├── vm_run.c          CLI runner source
│   ├── basic.c           NOP, MOVE, CONST_*, RETURN
│   ├── arithmetic.c      ADD/SUB/MUL/DIV/REM — i32 and i64
│   ├── floating_point.c  ADD/SUB/MUL/DIV — f32 and f64, NaN/Inf
│   ├── comparison.c      CMP_I32/I64/F32/F64
│   ├── branching.c       IF_EQ/NE/LT/GE/GT/LE, IF_*Z, GOTO
│   ├── bitwise.c         AND/OR/XOR/NOT — i32 and i64
│   ├── shifts.c          SHL/SHR/USHR — i32 and i64
│   ├── conversions.c     All 14 type conversion opcodes
│   ├── memory.c          LOAD*/STORE* — all widths, signed/unsigned
│   ├── pointers.c        LEA, LOAD_PTR, STORE_PTR, pointer chains
│   ├── native_functions.c CALL, CALL_VOID, native registration
├── example/                    # C API test & example programs (25 standalone files)
│   ├── vm_builder.h            # Dynamic bytecode emitter & test macro helpers
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
│   ├── debugger_profiler.c     # Debugger hooks, breakpoints, & profiler report
│   ├── subroutines.c           # Subroutines, recursion, & call stack frames
│   └── hash_algorithms.c       # DJB2, FNV-1a 32-bit & FNV-1a 64-bit hashing
├── example2/                   # Text Assembly (.cvma) test suite (19 files)
│   ├── cvma2cvmb.c             # Text assembly compiler tool (.cvma -> .cvmb)
│   ├── basic.cvma              # Basic assembly instructions
│   ├── arithmetic.cvma         # Assembly arithmetic ops
│   ├── control_flow.cvma       # Branching & label resolution
│   ├── native_calls.cvma       # Native host function calling
│   ├── bitwise_shifts.cvma     # Bitwise and shift assembly ops
│   ├── conversions.cvma        # Type conversion assembly ops
│   ├── floating_point.cvma     # Float assembly ops
│   ├── comparison.cvma         # Comparison assembly ops
│   ├── fibonacci.cvma          # Fibonacci algorithm assembly
│   ├── factorial.cvma          # Factorial algorithm assembly
│   ├── complex_algo.cvma       # Euclidean GCD algorithm
│   ├── unsigned_ops.cvma       # Unsigned ops & CMP_F32_GT assembly
│   ├── collatz.cvma            # Collatz 3n+1 stopping time assembly
│   ├── binary_search.cvma      # Binary search algorithm assembly
│   ├── subroutines.cvma        # Recursive assembly subroutines
│   ├── matrix_operations.cvma  # Subroutines, LEA, dot product & matrix trace
│   ├── sorting_and_stats.cvma  # Conversions, float division & statistical mean
│   ├── pseudo_random_hash.cvma # PRNG LCG algorithm, bitwise hash & shifts
│   └── hash_algorithms.cvma    # DJB2, FNV-1a 32-bit & FNV-1a 64-bit assembly
```

---

## Quick Start

### Embedding the VM

Copy `vm.h` and `vm.c` into your project. No other files are required.

```c
#include "vm.h"

int main(void) {
    /* 1. Build bytecode manually or with vm_builder.h */
    static const uint8_t code[] = {
        /* CONST_I32 r0, 42 */
        0x04, 0x00,  /* opcode OP_CONST_I32 */
        0x00,        /* dst = r0 */
        0x2A, 0x00, 0x00, 0x00,  /* val = 42 (LE) */
        /* RETURN r0 */
        0x5E, 0x00,  /* opcode OP_RETURN */
        0x00,        /* src = r0 */
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

### Assembling and Disassembling Code

Using `vm_assembler.h` and `vm_disassembler.h`:

```c
#include "vm_assembler.h"
#include "vm_disassembler.h"

int main(void) {
    const char* asm_source =
        "CONST_I32 r0, 100\n"
        "CONST_I32 r1, 200\n"
        "ADD_I32   r2, r0, r1\n"
        "RETURN    r2\n";

    /* Assemble text -> bytecode */
    VMAssembleResult res = vm_assemble(asm_source);
    if (res.success) {
        printf("Assembled %u bytes successfully.\n", (unsigned)res.size);

        /* Disassemble bytecode -> string */
        char* text = vm_disassemble(res.bytecode, res.size);
        printf("Disassembly:\n%s\n", text);
        free(text);
    }
    vm_assemble_free(&res);
    return 0;
}
```

---

### Assembling `.cvma` Text Files via CLI (`cvma2cvmb`)

```sh
cd example2/
make                      # Compiles cvma2cvmb and assembles all .cvma files
./cvma2cvmb fibonacci.cvma -o fibonacci.cvmb -v
../example/vm_run fibonacci.cvmb
```

---

## API Summary

### Core (`vm.h`)

| Function | Description |
|----------|-------------|
| `vm_init(ctx)` | Zero-initialise a `VMContext` before use |
| `vm_register_function(ctx, id, fn)` | Register a native C function at `id` (0–255) |
| `vm_execute(ctx, regs, reg_count, pc, code, size)` | Execute bytecode; returns `VMError` |

### Assembler (`vm_assembler.h`)

| Function | Description |
|----------|-------------|
| `vm_assemble(source_text)` | Assemble text source code to binary bytecode result |
| `vm_assemble_free(res)` | Free bytecode buffer inside result |
| `vm_assemble_to_buffer(...)` | Assemble directly into caller-allocated buffer |

### Disassembler (`vm_disassembler.h`)

| Function | Description |
|----------|-------------|
| `vm_disassemble_instruction(...)` | Disassemble a single instruction at `pc` |
| `vm_disassemble(code, size)` | Disassemble entire bytecode buffer into allocated string |
| `vm_disassemble_file(stream, code, size)` | Disassemble and print entire bytecode to `FILE*` |

### Bytecode Builder (`vm_builder.h`)

| Helper | Description |
|--------|-------------|
| `bc_init(bc)` | Initialise a `Bytecode` buffer |
| `emit_nop(bc)` | Emit `OP_NOP` |
| `emit_const_i32(bc, dst, val)` | Load 32-bit integer constant |
| `emit_add_i32(bc, dst, lhs, rhs)` | Integer add |
| `emit_if_lt(bc, a, b)` | Conditional branch (returns patch site) |
| `bc_patch_here(bc, site)` | Resolve a forward branch to current position |
| `bc_patch_back(bc, site, target)` | Resolve a backward branch |
| `bc_run(ctx, bc, reg_count)` | Execute the built bytecode |
| `bc_run_regs(ctx, bc, regs, reg_count)` | Execute with a pre-populated register file |

### File Format (`vm_loader.h`)

| Function | Description |
|----------|-------------|
| `cvmb_write(path, code, size, reg_count, entry)` | Write a `.cvmb` file |
| `cvmb_read(path, &hdr)` | Load a `.cvmb` file into a heap buffer |
| `cvmb_free(code)` | Free a buffer returned by `cvmb_read` |
| `vm_register_stdlib(ctx)` | Register the 12 standard native functions |

---

## Error Codes

| Code | Constant | Cause |
|------|----------|-------|
| 0 | `VM_OK` | Success |
| 1 | `VM_ERR_INVALID_OPCODE` | Unknown 16-bit opcode |
| 2 | `VM_ERR_OUT_OF_BOUNDS` | PC or branch target outside bytecode buffer |
| 3 | `VM_ERR_DIV_ZERO` | Integer division or remainder by zero |
| 4 | `VM_ERR_INVALID_REGISTER` | Register index >= reg_count |
| 5 | `VM_ERR_BAD_FUNCTION` | Unregistered or null native function id |
| 6 | `VM_ERR_BAD_ARGC` | More than 64 arguments passed to CALL |

---

## License

This project is licensed under the **GNU General Public License v3.0** (GPLv3).
See the [`LICENSE`](LICENSE) file for details.

Copyright (C) 2026 CVM Contributors.

---

## TODO

### Core VM

- [x] **Call stack / subroutines** — `OP_CALL_BC` and `OP_RET` for calling bytecode subroutines and supporting recursion
- [x] **Stack frames** — zero-allocation register frame save/restore (`VMFrame`, `call_stack`) for `OP_CALL_BC` so callee registers don't clobber caller state
- [x] **Unsigned integer support** — `DIV_U32`, `REM_U32`, `DIV_U64`, `REM_U64`
      for unsigned division semantics
- [x] **32-bit float comparisons** — `CMP_F32_GT` ordered variant to distinguish
      unordered-less from ordered-less
- [ ] **Wide register indices** — currently CALL arg lists use u8 register indices;
      consider u16 to support the full 0–255 range consistently

### Bytecode & Tooling

- [x] **Assembler** — two-pass assembler (`vm_assembler.h`) and CLI compiler (`cvma2cvmb`)
- [x] **Disassembler** — disassembler library (`vm_disassembler.h`) for decoding bytecode
- [ ] **Constant pool** — a dedicated section in the `.cvmb` format for storing
      string literals, float constants, and large integer constants referenced by
      index, reducing code size
- [ ] **Symbol table** — optional section in `.cvmb` for debug info (function names,
      source line mappings)
- [ ] **Magic version validation** — `cvmb_read` currently accepts any version byte;
      add rejection of unknown versions
- [ ] **`vm_run` — argument passing** — allow passing initial register values via
      command-line arguments (e.g. `vm_run program.cvmb --r0=42`)
- [ ] **`vm_run` — exit code** — propagate `ctx.result.i32` as the process exit code
- [x] **Debugger interface** — step/breakpoint callback hooks (`VMDebugHook`, `breakpoints`) in `VMContext`
- [x] **Profiler** — per-opcode execution counters and `--profile` report mode in `vm_run`

### Future Roadmap & Advanced Features

- [ ] **JIT Compilation Engine** — Just-In-Time native code compiler (x86_64 / AArch64) translating VM bytecode into native machine instructions for near-native performance
- [ ] **Garbage Collection System** — Mark-and-sweep or generational GC engine managing dynamic heap object allocations
- [ ] **Multi-threading / Async Fiber Support** — Lightweight cooperative fibers, async execution flags, and channel-based inter-fiber communication
- [ ] **Debug Adapter Protocol (DAP) / GDB Remote Bridge** — Standard DAP server implementation enabling full visual interactive debugging in VS Code / IDEs
- [ ] **Source Maps & Debug Symbol Tables (`.cvmdb`)** — Source file line-number mappings and variable name lookup tables for high-level debuggers
- [ ] **Foreign Function Interface (FFI)** — Dynamic shared library (`.so` / `.dll`) loader and C ABI marshalling engine for calling arbitrary host C libraries without recompilation
- [ ] **SIMD Vector Extensions** — 128-bit packed vector registers (`VEC128`) and parallel float/int vector arithmetic opcodes

### Performance & Testing

- [ ] **Computed goto dispatch** — replace `switch` dispatcher with `goto`-label
      table (`&&label`) for ~10–30% throughput improvement on tight loops
- [ ] **Fuzz testing** — libFuzzer / AFL integration for bytecode parsing and execution
- [ ] **Cross-platform CI** — automated build and test on Linux, macOS, and Windows
