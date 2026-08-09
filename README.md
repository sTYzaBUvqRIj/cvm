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
- **100 opcodes** covering:
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
│   ├── error_handling.c  All VM error codes intentionally triggered
│   ├── loops.c           Counting, sum, GCD, nested, do-while
│   ├── fibonacci.c       Iterative Fibonacci (i32 and i64)
│   ├── factorial.c       Factorial (i32 and i64)
│   ├── complex.c         Calculator, prime, sort, matrix, strlen
│   ├── file_execution.c  .cvmb write/read/execute round-trip demo
│   ├── assembler_disassembler_demo.c  Assembly & disassembly round-trip test
│   └── unsigned_ops.c    DIV_U32, REM_U32, DIV_U64, REM_U64, CMP_F32_GT
└── example2/         Text Assembly (.cvma) Example Suite & Compiler
    ├── Makefile          Builds cvma2cvmb and compiles all .cvma -> .cvmb
    ├── README.md         Documentation for example2 suite
    ├── cvma2cvmb.c       CLI assembly compiler source (.cvma -> .cvmb)
    ├── basic.cvma        Basic assembly registers, moves, constants
    ├── arithmetic.cvma   Integer & float arithmetic
    ├── control_flow.cvma Branches, loops, and labels
    ├── native_calls.cvma Native function calls (abs, max, print)
    ├── bitwise_shifts.cvma Bitwise operations and shifts
    ├── conversions.cvma  Int to float conversions
    ├── floating_point.cvma Double precision float math
    ├── comparison.cvma   Three-way comparisons (-1, 0, +1)
    ├── fibonacci.cvma    Fibonacci calculation loop
    ├── factorial.cvma    Factorial calculation loop
    ├── complex_algo.cvma Euclidean GCD algorithm in assembly
    └── unsigned_ops.cvma Unsigned division/remainder and CMP_F32_GT in assembly
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

- [ ] **Call stack / subroutines** — add `OP_CALL_BC` and `OP_RET` to call into
      other bytecode functions (separate from native calls), enabling recursion and
      multi-function programs
- [ ] **Stack frames** — associated register save/restore for `OP_CALL_BC` so
      callee registers don't clobber caller state
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
- [ ] **Debugger interface** — a step/breakpoint callback hook in `VMContext`
- [ ] **Profiler** — per-opcode execution counters and a report mode in `vm_run`

### Performance & Testing

- [ ] **Computed goto dispatch** — replace `switch` dispatcher with `goto`-label
      table (`&&label`) for ~10–30% throughput improvement on tight loops
- [ ] **Fuzz testing** — libFuzzer / AFL integration for bytecode parsing and execution
- [ ] **Cross-platform CI** — automated build and test on Linux, macOS, and Windows
