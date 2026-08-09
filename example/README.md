# VM Example Suite

A comprehensive set of standalone C programs demonstrating every opcode and
feature of the C/C++ Register VM.  Each program is an independent executable
that builds bytecode at runtime, executes it, and reports `[PASS]` / `[FAIL]`
for every test case.

---

## Directory layout

```
example/
├── vm_builder.h        shared bytecode-construction helpers + test framework
├── Makefile            build system
├── README.md           this file
│
├── basic.c             VM init, NOP, MOVE, CONST, RETURN
├── arithmetic.c        integer arithmetic (i32 + i64), NEG
├── floating_point.c    float arithmetic (f32 + f64), NEG, NaN/Inf
├── comparison.c        CMP_I32/I64/F32/F64, NaN semantics
├── branching.c         all IF_* opcodes; GOTO; forward/backward branches
├── bitwise.c           AND/OR/XOR/NOT for i32 and i64
├── shifts.c            SHL/SHR/USHR for i32 and i64
├── conversions.c       all 14 type conversion opcodes
├── memory.c            LOAD*/STORE* for 8/16/32/64/PTR widths
├── pointers.c          LEA, LOAD_PTR, STORE_PTR, pointer arithmetic
├── native_functions.c  CALL / CALL_VOID with multiple signatures
├── error_handling.c    every VMError code intentionally triggered
├── loops.c             counting, sum, GCD, nested loops, do-while
├── fibonacci.c         iterative fib (i32 + i64), first-N sequence
├── factorial.c         factorial loop (i32 + i64), edge cases
├── complex.c           calculator, prime test, sort, matrix, statistics
├── file_execution.c    .cvmb write/read/execute round-trip demo
├── assembler_disassembler_demo.c assembly parsing & disassembler round-trip test
└── unsigned_ops.c      DIV_U32, REM_U32, DIV_U64, REM_U64, CMP_F32_GT tests
```

---

## How to compile

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

## How to run

```bash
# Run every example sequentially:
make run

# Run one example:
make basic          # builds + runs basic
./comparison        # just run (if already built)
```

Expected output from every program ends with:

```
========================================
Results: N / N passed  (all tests passed)
========================================
```

---

## Bytecode format

Every instruction begins with a **16-bit little-endian opcode**, followed by
zero or more operand bytes.  All multi-byte operands are little-endian.

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

### Branch offsets

All branch offsets are **signed** and **relative to the next instruction**
(the byte immediately after the last operand byte of the branch).

```
target_pc = next_pc + offset
```

- `offset = 0` → no-op (fall through)
- `offset < 0` → loop back
- `offset > 0` → skip forward

---

## Bytecode construction in examples

All examples use the helpers in `vm_builder.h`:

```c
Bytecode bc;
bc_init(&bc);

// emit instructions:
emit_const_i32(&bc, 0, 42);  // r0 = 42
emit_return(&bc, 0);          // return r0

// run with a 4-register file:
VMContext ctx;
vm_init(&ctx);
VMError err = bc_run(&ctx, &bc, 4);

// check result:
check_i32("const and return", ctx.result.i32, 42);
```

### Forward branch pattern

```c
emit_const_i32(&bc, 0, 0);              // r0 = 0
uint32_t skip = emit_if_nez(&bc, 0);    // if r0 != 0, jump
emit_const_i32(&bc, 1, 100);            // (taken path)
uint32_t end  = emit_goto_16_fwd(&bc);
bc_patch_here(&bc, skip);               // skip lands here
emit_const_i32(&bc, 1, 200);            // (not-taken / else)
bc_patch_here(&bc, end);
emit_return(&bc, 1);
```

### Backward loop pattern

```c
uint32_t loop_top = bc.size;
emit_add_i32(&bc, 2, 2, 0);           // sum += counter
emit_add_i32(&bc, 0, 0, 3);           // counter++
uint32_t p = emit_if_le(&bc, 0, 1);   // if counter <= limit, loop
bc_patch_back(&bc, p, loop_top);       // jump back
emit_return(&bc, 2);                   // return sum
```

### Memory / pointer pattern

```c
int32_t host_value = 0;
VMRegister regs[4];
memset(regs, 0, sizeof(regs));
regs[0].ptr = &host_value;  // r0 = pointer to host_value

bc_init(&bc);
emit_const_i32(&bc, 1, 99); // r1 = 99
emit_store32(&bc, 0, 1);    // *r0 = r1
emit_load32s(&bc, 2, 0);    // r2 = *r0 (sign-extended)
emit_return(&bc, 2);

VMError err = bc_run_regs(&ctx, &bc, regs, 4);
// host_value == 99, ctx.result.i32 == 99
```

---

## How to add a new example

1. Create `example/your_example.c`:
   ```c
   #include "vm_builder.h"

   static void test_something(void) {
       VMContext ctx; vm_init(&ctx);
       Bytecode  bc;  bc_init(&bc);
       // ... emit instructions ...
       VMError err = bc_run(&ctx, &bc, 8);
       check_err ("no error",  err,              VM_OK);
       check_i32 ("result",    ctx.result.i32,   expected);
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

## Opcode coverage table

| Opcode(s) | Example file | Notes |
|-----------|-------------|-------|
| `OP_NOP` | `basic.c` | ✓ |
| `OP_MOVE` | `basic.c` | ✓ wide src (u16) tested |
| `OP_CONST_I8` | `basic.c` | ✓ positive and negative |
| `OP_CONST_I16` | `basic.c` | ✓ |
| `OP_CONST_I32` | `basic.c`, all | ✓ |
| `OP_CONST_I64` | `basic.c`, `fibonacci.c` | ✓ INT64_MAX/MIN |
| `OP_CONST_F32` | `basic.c`, `floating_point.c` | ✓ |
| `OP_CONST_F64` | `basic.c`, `floating_point.c` | ✓ |
| `OP_ADD_I32` | `arithmetic.c` | ✓ |
| `OP_SUB_I32` | `arithmetic.c` | ✓ |
| `OP_MUL_I32` | `arithmetic.c` | ✓ |
| `OP_DIV_I32` | `arithmetic.c`, `error_handling.c` | ✓ div-zero tested |
| `OP_REM_I32` | `arithmetic.c`, `error_handling.c` | ✓ |
| `OP_ADD_I64` | `arithmetic.c`, `fibonacci.c` | ✓ |
| `OP_SUB_I64` | `arithmetic.c` | ✓ |
| `OP_MUL_I64` | `arithmetic.c`, `factorial.c` | ✓ |
| `OP_DIV_I64` | `arithmetic.c`, `error_handling.c` | ✓ |
| `OP_REM_I64` | `arithmetic.c`, `loops.c` | ✓ |
| `OP_DIV_U32` | `unsigned_ops.c` | ✓ |
| `OP_REM_U32` | `unsigned_ops.c` | ✓ |
| `OP_DIV_U64` | `unsigned_ops.c` | ✓ |
| `OP_REM_U64` | `unsigned_ops.c` | ✓ |
| `OP_ADD_F32` | `floating_point.c` | ✓ |
| `OP_SUB_F32` | `floating_point.c` | ✓ |
| `OP_MUL_F32` | `floating_point.c` | ✓ |
| `OP_DIV_F32` | `floating_point.c` | ✓ |
| `OP_ADD_F64` | `floating_point.c` | ✓ |
| `OP_SUB_F64` | `floating_point.c` | ✓ |
| `OP_MUL_F64` | `floating_point.c` | ✓ |
| `OP_DIV_F64` | `floating_point.c` | ✓ |
| `OP_NEG_I32` | `arithmetic.c` | ✓ |
| `OP_NEG_I64` | `arithmetic.c` | ✓ |
| `OP_NEG_F32` | `floating_point.c` | ✓ |
| `OP_NEG_F64` | `floating_point.c` | ✓ |
| `OP_NOT_I32` | `bitwise.c` | ✓ |
| `OP_NOT_I64` | `bitwise.c` | ✓ |
| `OP_AND_I32` | `bitwise.c` | ✓ masking, flags |
| `OP_OR_I32` | `bitwise.c` | ✓ bit setting |
| `OP_XOR_I32` | `bitwise.c` | ✓ toggle |
| `OP_AND_I64` | `bitwise.c` | ✓ |
| `OP_OR_I64` | `bitwise.c` | ✓ |
| `OP_XOR_I64` | `bitwise.c` | ✓ |
| `OP_SHL_I32` | `shifts.c` | ✓ |
| `OP_SHR_I32` | `shifts.c` | ✓ arithmetic / sign-preserving |
| `OP_USHR_I32` | `shifts.c` | ✓ logical / zero-filling |
| `OP_SHL_I64` | `shifts.c` | ✓ |
| `OP_SHR_I64` | `shifts.c` | ✓ |
| `OP_USHR_I64` | `shifts.c` | ✓ |
| `OP_CMP_I32` | `comparison.c` | ✓ <, =, >, negative, zero |
| `OP_CMP_I64` | `comparison.c` | ✓ INT64_MAX |
| `OP_CMP_F32` | `comparison.c`, `unsigned_ops.c` | ✓ NaN→-1, ±Inf |
| `OP_CMP_F64` | `comparison.c` | ✓ NaN→-1, ±Inf |
| `OP_CMP_F32_GT` | `unsigned_ops.c` | ✓ NaN→+2 |
| `OP_I32_TO_I8` | `conversions.c` | ✓ truncation + sign-extend |
| `OP_I32_TO_I16` | `conversions.c` | ✓ |
| `OP_I32_TO_I64` | `conversions.c` | ✓ sign extension |
| `OP_I32_TO_F32` | `conversions.c` | ✓ |
| `OP_I32_TO_F64` | `conversions.c` | ✓ |
| `OP_I64_TO_I32` | `conversions.c` | ✓ truncation |
| `OP_I64_TO_F32` | `conversions.c` | ✓ |
| `OP_I64_TO_F64` | `conversions.c` | ✓ |
| `OP_F32_TO_I32` | `conversions.c` | ✓ truncate toward zero |
| `OP_F32_TO_I64` | `conversions.c` | ✓ |
| `OP_F32_TO_F64` | `conversions.c` | ✓ |
| `OP_F64_TO_I32` | `conversions.c` | ✓ |
| `OP_F64_TO_I64` | `conversions.c` | ✓ |
| `OP_F64_TO_F32` | `conversions.c` | ✓ |
| `OP_LOAD8` | `memory.c` | ✓ zero-extend |
| `OP_LOAD8S` | `memory.c` | ✓ sign-extend |
| `OP_LOAD16` | `memory.c` | ✓ |
| `OP_LOAD16S` | `memory.c` | ✓ |
| `OP_LOAD32` | `memory.c` | ✓ |
| `OP_LOAD32S` | `memory.c`, `pointers.c` | ✓ |
| `OP_LOAD64` | `memory.c` | ✓ |
| `OP_LOAD_PTR` | `memory.c`, `pointers.c` | ✓ pointer chain |
| `OP_STORE8` | `memory.c`, `pointers.c` | ✓ |
| `OP_STORE16` | `memory.c` | ✓ |
| `OP_STORE32` | `memory.c`, `pointers.c`, `complex.c` | ✓ |
| `OP_STORE64` | `memory.c` | ✓ |
| `OP_STORE_PTR` | `memory.c`, `pointers.c` | ✓ |
| `OP_LEA` | `pointers.c`, `memory.c`, `complex.c` | ✓ positive, negative, zero offset |
| `OP_GOTO` | `branching.c` | ✓ i8 offset |
| `OP_GOTO_16` | `branching.c`, all loops | ✓ forward and backward |
| `OP_GOTO_32` | `branching.c` | ✓ |
| `OP_IF_EQ` | `branching.c` | ✓ taken + not-taken |
| `OP_IF_NE` | `branching.c` | ✓ |
| `OP_IF_LT` | `branching.c` | ✓ |
| `OP_IF_GE` | `branching.c` | ✓ |
| `OP_IF_GT` | `branching.c` | ✓ |
| `OP_IF_LE` | `branching.c`, `loops.c` | ✓ |
| `OP_IF_EQZ` | `branching.c` | ✓ |
| `OP_IF_NEZ` | `branching.c`, `loops.c` | ✓ |
| `OP_IF_LTZ` | `branching.c` | ✓ |
| `OP_IF_GEZ` | `branching.c` | ✓ |
| `OP_IF_GTZ` | `branching.c` | ✓ |
| `OP_IF_LEZ` | `branching.c` | ✓ |
| `OP_CALL` | `native_functions.c`, `fibonacci.c` | ✓ 0–4 args, f64 return |
| `OP_CALL_VOID` | `native_functions.c`, `loops.c` | ✓ result in ctx |
| `OP_RETURN_VOID` | `basic.c`, `error_handling.c` | ✓ |
| `OP_RETURN` | `basic.c`, all | ✓ |

**All 100 implemented opcodes are covered.**

---

## Error code coverage

| Error | Triggered in |
|-------|-------------|
| `VM_OK` (0) | every successful test |
| `VM_ERR_INVALID_OPCODE` (1) | `error_handling.c` |
| `VM_ERR_OUT_OF_BOUNDS` (2) | `error_handling.c` — truncated bytecode + bad branch |
| `VM_ERR_DIV_ZERO` (3) | `error_handling.c`, `arithmetic.c` |
| `VM_ERR_INVALID_REGISTER` (4) | `error_handling.c` |
| `VM_ERR_BAD_FUNCTION` (5) | `error_handling.c` — id out of range + null slot |
| `VM_ERR_BAD_ARGC` (6) | `error_handling.c` |

---

## VM API quick reference

```c
void    vm_init(VMContext* ctx);
VMError vm_register_function(VMContext* ctx, uint32_t id, VMNativeFn fn);
VMError vm_execute(VMContext* ctx, VMRegister* regs, uint32_t reg_count,
                   uint32_t pc, const uint8_t* bytecode, uint32_t bytecode_size);
```

### Native function signature

```c
typedef VMError (*VMNativeFn)(VMContext* ctx, uint32_t argc,
                              VMRegister* args, VMRegister* out_result);
```

Return `VM_OK` on success.  Any other error aborts execution and is returned
by `vm_execute()`.

### VMRegister union

```c
typedef union {
    int8_t i8;  uint8_t u8;  int16_t i16;  uint16_t u16;
    int32_t i32; uint32_t u32; int64_t i64; uint64_t u64;
    float f32;   double f64;   void* ptr;
} VMRegister;
```

Registers are untyped at the VM level.  Read the member that matches the
instruction that wrote the register.

---

## Notes on semantics

| Topic | Behaviour |
|-------|-----------|
| Integer constants | Sign-extended into full 64-bit register |
| `DIV_I32 / REM_I32` | Trap `VM_ERR_DIV_ZERO` on divisor = 0; `INT32_MIN / -1` returns `INT32_MIN` |
| `DIV_F32 / DIV_F64` | IEEE-754 — produces `Inf` or `NaN`, never a VM error |
| `CMP_F32 / CMP_F64` | Returns `-1` when either operand is NaN |
| `SHR_I32 / SHR_I64` | Arithmetic (sign-preserving) right shift |
| `USHR_I32 / USHR_I64` | Logical (zero-filling) right shift |
| `I32_TO_I8` | Truncates then sign-extends into full 64-bit register |
| `LOAD8S / LOAD16S / LOAD32S` | Sign-extend into full 64-bit register |
| `LOAD8 / LOAD16 / LOAD32` | Zero-extend into full 64-bit register |
| Branch offset | Signed, relative to next instruction (byte after last operand byte) |
