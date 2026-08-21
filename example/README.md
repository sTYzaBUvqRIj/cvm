# CVM Example Suite (`example/`)

A comprehensive set of standalone C programs demonstrating every opcode and
feature of the C/C++ Register VM. Each program is an independent executable
that builds bytecode at runtime, executes it, and reports `[PASS]` / `[FAIL]`
for every test case.

---

## Directory Layout

```
example/
├── vm_run.c                     CLI runner — executes .cvmb files from the command line
├── Makefile                     Build system for all examples
├── README.md                    This file
│
│── Core Language
├── basic.c                      VM init, NOP, MOVE, CONST, RETURN
├── arithmetic.c                 Integer arithmetic (i32 + i64), NEG
├── floating_point.c             Float arithmetic (f32 + f64), NEG, NaN/Inf edge cases
├── comparison.c                 CMP_I32/I64/F32/F64, NaN semantics
├── branching.c                  All IF_* opcodes; GOTO; forward/backward branches
├── bitwise.c                    AND/OR/XOR/NOT for i32 and i64
├── shifts.c                     SHL/SHR/USHR for i32 and i64
├── conversions.c                All 14 type conversion opcodes
├── unsigned_ops.c               DIV_U32/U64, REM_U32/U64, CMP_F32_GT
│
│── Memory & Pointers
├── memory.c                     LOAD*/STORE* for 8/16/32/64/PTR widths
├── pointers.c                   LEA, LOAD_PTR, STORE_PTR, pointer arithmetic
│
│── Functions & Calls
├── native_functions.c           CALL / CALL_VOID with multiple signatures
├── subroutines.c                Bytecode subroutines, recursion & call stack frames (CALL_BC / RET)
│
│── Algorithms
├── loops.c                      Counting, sum, GCD, nested loops, do-while patterns
├── fibonacci.c                  Iterative fib (i32 + i64), first-N sequence
├── factorial.c                  Factorial loop (i32 + i64), edge cases
├── complex.c                    Calculator, prime test, sort, matrix, statistics
├── hash_algorithms.c            DJB2, FNV-1a 32-bit & FNV-1a 64-bit hashing algorithms
│
│── Extended Opcodes
├── extended_ops.c               116-test suite:
│                                  - CMP_U32/U64, IF_ULT/UGE/UGT/ULE (unsigned compare & branch)
│                                  - SELECT (branchless conditional)
│                                  - CLZ/CTZ/POPCNT (i32 & i64)
│                                  - ROTL/ROTR (i32 & i64)
│                                  - ABS/MIN/MAX_I32/I64 (signed & unsigned)
│                                  - MULH_I32/U32/I64/U64 (high-half multiply)
│                                  - BOOL_I32/I64 (normalize to 0/1)
│                                  - ABS/SQRT/FLOOR/CEIL/TRUNC/ROUND_F32/F64
│                                  - MIN/MAX/COPYSIGN_F32/F64
│                                  - LOAD*/STORE* with immediate offset (all widths)
│                                  - LEA_REG (variable-index pointer arithmetic)
│                                  - MEMCPY / MEMSET
│                                  - SWITCH (O(1) dispatch table with overflow guards)
│                                  - Dirty high-bits register hygiene & opcode tracing
│
│── Tooling & Internals
├── error_handling.c             Every VMError code intentionally triggered
├── single_step.c                VMContext pc, flags & single-step execution control
├── coroutines.c                 Generator/coroutine pattern via single-stepping
├── debugger_profiler.c          Debugger hooks, breakpoints & per-opcode profiler
├── benchmark.c                  Performance benchmark — 1M Collatz runs
├── file_execution.c             .cvmb write/read/execute round-trip demo
└── assembler_disassembler_demo.c  Assembly parsing & disassembler round-trip test
```

---

## How to Compile

### All examples at once (Linux / macOS / MSYS2)

```bash
cd example/
make
```

### All examples at once (Windows — PowerShell / MinGW)

```powershell
cd example
make
# or individually:
gcc -std=c99 -Wall -I.. -o basic.exe basic.c ../vm.c -lm
```

### With debug tracing (prints every executed opcode to stderr)

```bash
make VM_DEBUG=1
```

---

## How to Run

```bash
# Run every example sequentially:
make run

# Run one example:
make basic          # builds + runs basic
./comparison        # just run (if already built)

# Run vm_run CLI tool on a .cvmb file:
./vm_run ../example2/fibonacci.cvmb
```

Expected output from every program ends with:

```
========================================
Results: N / N passed  (all tests passed)
========================================
```

---

## Bytecode Format

Every instruction begins with a **16-bit little-endian opcode**, followed by
zero or more operand bytes. All multi-byte operands are little-endian.

| Notation | Width | Meaning |
|----------|-------|---------|
| `u8` | 1 byte | unsigned byte operand |
| `i8` | 1 byte | signed byte operand |
| `u16` | 2 bytes LE | unsigned 16-bit |
| `i16` | 2 bytes LE | signed 16-bit |
| `u32` | 4 bytes LE | unsigned 32-bit |
| `i32` | 4 bytes LE | signed 32-bit |
| `u64` | 8 bytes LE | unsigned 64-bit |
| `i64` | 8 bytes LE | signed 64-bit |
| `dst/lhs/rhs/src/addr/base/val/amt` | `u8` | register index |

### Branch Offsets

All branch offsets are **signed** and **relative to the next instruction**
(the byte immediately after the last operand byte of the branch instruction).

```
target_pc = next_pc + offset
```

- `offset = 0` → no-op (fall through)
- `offset < 0` → loop back
- `offset > 0` → skip forward

---

## Bytecode Construction Patterns

All examples use helpers from `vm_builder.h`:

```c
VMContext ctx;
Bytecode  bc;
vm_init(&ctx);
bc_init(&bc);

/* Emit instructions */
emit_const_i32(&bc, 0, 42);  /* r0 = 42 */
emit_return   (&bc, 0);       /* return r0 */

/* Run with a 4-register file */
VMError err = bc_run(&ctx, &bc, 4);

/* Check result */
check_i32("const and return", ctx.result.i32, 42);
```

### Forward Branch Pattern

```c
uint32_t skip = emit_if_nez(&bc, 0);    /* if r0 != 0, jump forward */
emit_const_i32(&bc, 1, 100);            /* not-taken path */
uint32_t end  = emit_goto_16_fwd(&bc);
bc_patch_here(&bc, skip);               /* skip lands here */
emit_const_i32(&bc, 1, 200);            /* taken path */
bc_patch_here(&bc, end);
emit_return(&bc, 1);
```

### Backward Loop Pattern

```c
uint32_t loop_top = bc.size;
emit_add_i32(&bc, 2, 2, 0);            /* sum += counter */
emit_add_i32(&bc, 0, 0, 3);            /* counter++ */
uint32_t p = emit_if_le(&bc, 0, 1);    /* if counter <= limit, loop */
bc_patch_back(&bc, p, loop_top);        /* jump back */
emit_return(&bc, 2);
```

### Memory / Pointer Pattern

```c
int32_t host_value = 0;
VMRegister regs[4];
memset(regs, 0, sizeof(regs));
regs[0].ptr = &host_value;             /* r0 = pointer to host variable */

bc_init(&bc);
emit_const_i32(&bc, 1, 99);            /* r1 = 99 */
emit_store32  (&bc, 0, 1);             /* *r0 = r1 */
emit_load32s  (&bc, 2, 0);             /* r2 = *r0 (sign-extended) */
emit_return   (&bc, 2);

VMError err = bc_run_regs(&ctx, &bc, regs, 4);
/* host_value == 99, ctx.result.i32 == 99 */
```

### Load/Store with Immediate Offset Pattern

```c
struct { int32_t x; int32_t y; } pt = {10, 20};
VMRegister regs[8];
memset(regs, 0, sizeof(regs));
regs[0].ptr = &pt;

emit_load32s_off(&bc, 1, 0, 0);        /* r1 = pt.x  (offset=0) */
emit_load32s_off(&bc, 2, 0, 4);        /* r2 = pt.y  (offset=4) */
emit_const_i32  (&bc, 3, 99);
emit_store32_off(&bc, 0, 3, 4);        /* pt.y = 99 */
```

### SWITCH Pattern

```c
uint32_t sw     = emit_switch_header(&bc, 0, 3);   /* reg=r0, 3 cases */
uint32_t sw_end = bc.size;                          /* PC after header  */

uint32_t pc0 = bc.size; emit_const_i32(&bc, 1, 10); uint32_t j0 = emit_goto_16_fwd(&bc);
uint32_t pc1 = bc.size; emit_const_i32(&bc, 1, 20); uint32_t j1 = emit_goto_16_fwd(&bc);
uint32_t pc2 = bc.size; emit_const_i32(&bc, 1, 30); uint32_t j2 = emit_goto_16_fwd(&bc);
uint32_t pcd = bc.size; emit_const_i32(&bc, 1, 99); /* default */

bc_patch_here(&bc, j0); bc_patch_here(&bc, j1); bc_patch_here(&bc, j2);
emit_return(&bc, 1);

bc_patch_switch(&bc, sw, 0, pcd, sw_end);           /* slot 0 = default */
bc_patch_switch(&bc, sw, 1, pc0, sw_end);            /* slot 1 = case 0  */
bc_patch_switch(&bc, sw, 2, pc1, sw_end);            /* slot 2 = case 1  */
bc_patch_switch(&bc, sw, 3, pc2, sw_end);            /* slot 3 = case 2  */
```

---

## How to Add a New Example

1. Create `example/your_example.c`:
   ```c
   #include "../vm_builder.h"

   static void test_something(void) {
       VMContext ctx; vm_init(&ctx);
       Bytecode  bc;  bc_init(&bc);
       /* ... emit instructions ... */
       VMError err = bc_run(&ctx, &bc, 8);
       check_err("no error", err,           VM_OK);
       check_i32("result",   ctx.result.i32, expected);
       vm_destroy(&ctx);
   }

   int main(void) {
       printf("=== Your Example ===\n");
       test_something();
       print_summary();
       return g_fail > 0 ? 1 : 0;
   }
   ```
2. Add `your_example` to the `EXAMPLES` list in `Makefile`.
3. Run `make your_example` to build and test.

---

## Opcode Coverage Table

| Opcode(s) | Example file | Notes |
|-----------|-------------|-------|
| `OP_NOP` | `basic.c` | ✓ |
| `OP_MOVE` | `basic.c` | ✓ wide src (u16) tested |
| `OP_CONST_I8/I16/I32/I64` | `basic.c`, all | ✓ INT64_MAX/MIN |
| `OP_CONST_F32/F64` | `basic.c`, `floating_point.c` | ✓ |
| `OP_ADD/SUB/MUL/DIV/REM_I32` | `arithmetic.c`, `error_handling.c` | ✓ div-zero tested |
| `OP_ADD/SUB/MUL/DIV/REM_I64` | `arithmetic.c`, `fibonacci.c` | ✓ |
| `OP_DIV/REM_U32` | `unsigned_ops.c` | ✓ |
| `OP_DIV/REM_U64` | `unsigned_ops.c` | ✓ |
| `OP_ADD/SUB/MUL/DIV_F32` | `floating_point.c` | ✓ |
| `OP_ADD/SUB/MUL/DIV_F64` | `floating_point.c` | ✓ |
| `OP_NEG_I32/I64` | `arithmetic.c` | ✓ |
| `OP_NEG_F32/F64` | `floating_point.c` | ✓ |
| `OP_NOT/AND/OR/XOR_I32` | `bitwise.c` | ✓ masking, flags, toggle |
| `OP_NOT/AND/OR/XOR_I64` | `bitwise.c` | ✓ |
| `OP_SHL/SHR/USHR_I32` | `shifts.c` | ✓ arithmetic vs logical |
| `OP_SHL/SHR/USHR_I64` | `shifts.c` | ✓ |
| `OP_CMP_I32/I64` | `comparison.c` | ✓ <, =, >, negative, zero |
| `OP_CMP_F32/F64` | `comparison.c` | ✓ NaN→-1, ±Inf |
| `OP_CMP_F32_GT` | `unsigned_ops.c` | ✓ NaN→+2 |
| `OP_CMP_U32/U64` | `extended_ops.c` | ✓ unsigned three-way compare |
| `OP_I32_TO_I8/I16/I64` | `conversions.c` | ✓ sign-extend / truncate |
| `OP_I32_TO_F32/F64` | `conversions.c` | ✓ |
| `OP_I64_TO_I32/F32/F64` | `conversions.c` | ✓ |
| `OP_F32_TO_I32/I64/F64` | `conversions.c` | ✓ truncate toward zero |
| `OP_F64_TO_I32/I64/F32` | `conversions.c` | ✓ |
| `OP_LOAD8/8S/16/16S/32/32S/64/PTR` | `memory.c` | ✓ zero/sign extend |
| `OP_STORE8/16/32/64/PTR` | `memory.c`, `pointers.c` | ✓ |
| `OP_LOAD8_OFF / LOAD8S_OFF` | `extended_ops.c` | ✓ immediate offset |
| `OP_LOAD32S_OFF` | `extended_ops.c` | ✓ struct field read |
| `OP_STORE32_OFF` | `extended_ops.c` | ✓ struct field write |
| `OP_LEA` | `pointers.c`, `memory.c` | ✓ pos/neg/zero offset |
| `OP_LEA_REG` | `extended_ops.c` | ✓ variable-index array access |
| `OP_MEMCPY / OP_MEMSET` | `extended_ops.c` | ✓ partial copy, fill |
| `OP_GOTO / GOTO_16 / GOTO_32` | `branching.c` | ✓ all sizes |
| `OP_IF_EQ/NE/LT/GE/GT/LE` | `branching.c` | ✓ taken + not-taken |
| `OP_IF_EQZ/NEZ/LTZ/GEZ/GTZ/LEZ` | `branching.c` | ✓ |
| `OP_IF_ULT/UGE/UGT/ULE` | `extended_ops.c` | ✓ unsigned branches |
| `OP_SELECT` | `extended_ops.c` | ✓ cond=0/1/nonzero |
| `OP_CLZ_I32/I64` | `extended_ops.c` | ✓ 0-input = width |
| `OP_CTZ_I32/I64` | `extended_ops.c` | ✓ |
| `OP_POPCNT_I32/I64` | `extended_ops.c` | ✓ all-zeros, all-ones |
| `OP_ROTL_I32/ROTR_I32` | `extended_ops.c` | ✓ wrap-around |
| `OP_ROTL_I64/ROTR_I64` | `extended_ops.c` | ✓ |
| `OP_ABS_I32/I64` | `extended_ops.c` | ✓ |
| `OP_MIN/MAX_I32` | `extended_ops.c` | ✓ signed |
| `OP_MIN/MAX_U32` | `extended_ops.c` | ✓ unsigned |
| `OP_MIN/MAX_I64` | `extended_ops.c` | ✓ INT64_MIN/MAX |
| `OP_MULH_I32/U32/I64/U64` | `extended_ops.c` | ✓ high-half precision |
| `OP_BOOL_I32/I64` | `extended_ops.c` | ✓ 0→0, any→1 |
| `OP_ABS/SQRT/FLOOR/CEIL/TRUNC/ROUND_F32` | `extended_ops.c` | ✓ |
| `OP_ABS/SQRT/FLOOR/CEIL/TRUNC/ROUND_F64` | `extended_ops.c` | ✓ |
| `OP_MIN/MAX/COPYSIGN_F32` | `extended_ops.c` | ✓ |
| `OP_MIN/MAX/COPYSIGN_F64` | `extended_ops.c` | ✓ |
| `OP_CALL / OP_CALL_VOID` | `native_functions.c` | ✓ 0–4 args, f64 return |
| `OP_RETURN_VOID / OP_RETURN` | `basic.c`, all | ✓ |
| `OP_CALL_BC / OP_RET` | `subroutines.c` | ✓ recursion, stack frames |
| `OP_SWITCH` | `extended_ops.c` | ✓ in-bounds, out-of-bounds, negative key |

---

## Error Code Coverage

| Error | Triggered in |
|-------|-------------|
| `VM_OK` (0) | every successful test |
| `VM_ERR_INVALID_OPCODE` (1) | `error_handling.c` |
| `VM_ERR_OUT_OF_BOUNDS` (2) | `error_handling.c` — truncated bytecode + bad branch |
| `VM_ERR_DIV_ZERO` (3) | `error_handling.c`, `arithmetic.c` |
| `VM_ERR_INVALID_REGISTER` (4) | `error_handling.c` |
| `VM_ERR_BAD_FUNCTION` (5) | `error_handling.c` — id out of range + null slot |
| `VM_ERR_BAD_ARGC` (6) | `error_handling.c` |
| `VM_ERR_STACK_OVERFLOW` (7) | `subroutines.c` — recursion depth > 128 |

---

## VM API Quick Reference

```c
void    vm_init(VMContext* ctx);
void    vm_destroy(VMContext* ctx);
VMError vm_register_function(VMContext* ctx, uint32_t id, VMNativeFn fn);
VMError vm_execute(VMContext* ctx, VMRegister* regs, uint32_t reg_count,
                   uint32_t pc, const uint8_t* bytecode, uint32_t size);
```

### Native Function Signature

```c
typedef VMError (*VMNativeFn)(VMContext* ctx, uint32_t argc,
                              VMRegister* args, VMRegister* out_result);
```

Return `VM_OK` on success. Any other error aborts execution and is returned
by `vm_execute()`.

### VMRegister Union

```c
typedef union {
    int8_t i8;   uint8_t u8;   int16_t i16;  uint16_t u16;
    int32_t i32; uint32_t u32; int64_t i64;  uint64_t u64;
    float f32;   double f64;   void* ptr;
} VMRegister;
```

Registers are untyped at the VM level. Read the member that matches the
instruction that wrote the register.

---

## Semantic Notes

| Topic | Behaviour |
|-------|-----------|
| Integer constants | Sign-extended into full 64-bit register |
| `DIV_I32 / REM_I32` | Traps `VM_ERR_DIV_ZERO` on divisor = 0; `INT32_MIN / -1` returns `INT32_MIN` |
| `DIV_F32 / DIV_F64` | IEEE-754 — produces `Inf` or `NaN`, never a VM error |
| `CMP_F32 / CMP_F64` | Returns `-1` when either operand is NaN |
| `CMP_F32_GT` | Returns `+2` when either operand is NaN (ordered-greater semantics) |
| `SHR_I32 / SHR_I64` | Arithmetic (sign-preserving) right shift |
| `USHR_I32 / USHR_I64` | Logical (zero-filling) right shift |
| `ROTL/ROTR_I32` | Rotation amount masked to low 5 bits |
| `ROTL/ROTR_I64` | Rotation amount masked to low 6 bits |
| `CLZ/CTZ on 0` | Returns type width (32 or 64) — no undefined behaviour |
| `BOOL_I32` | Checks `src.i32 != 0`; writes `1` or `0` to `dst.i32` |
| `BOOL_I64` | Checks `src.i64 != 0`; writes `1` or `0` to `dst.i32` |
| `SELECT` | `dst = (cond.i32 != 0) ? a : b` — copies full 64-bit register |
| `SWITCH` | `reg.i32 < 0` or `>= count` → default case; never traps |
| `MEMCPY` | Uses `memmove` — handles overlapping regions safely |
| `MEMSET` | Uses `val.i32 & 0xFF` as the byte fill value |
| `LOAD8S / LOAD16S / LOAD32S` | Sign-extend into full 64-bit register |
| `LOAD8 / LOAD16 / LOAD32` | Zero-extend into full 64-bit register |
| Branch offset | Signed, relative to next instruction |
