# Example 2 — CVM Assembly (`.cvma`) Programs

This directory contains programs written in **CVM Assembly text format (`.cvma`)**.
Each file is assembled into **CVM Binary Bytecode (`.cvmb`)** using `cvma2cvmb`, and
executed with the `vm_run` command-line tool.

---

## Directory Structure

```
example2/
├── cvma2cvmb.c              CLI compiler/assembler tool (.cvma → .cvmb)
├── Makefile                 Automates compilation, assembly, and execution
├── README.md                This file
│
│── Core Language
├── basic.cvma               Constants, register moves, and return
├── arithmetic.cvma          Integer and floating-point arithmetic
├── control_flow.cvma        Branches, loops, and label definitions
├── native_calls.cvma        Native host function calls (abs, max, print)
├── bitwise_shifts.cvma      Bitwise operations (AND/OR/XOR) and bit shifts
├── conversions.cvma         Type conversions between int32 and double
├── floating_point.cvma      Double precision float calculations
├── comparison.cvma          Three-way comparisons (-1, 0, +1)
├── unsigned_ops.cvma        Unsigned division/remainder and CMP_F32_GT
│
│── Algorithms
├── fibonacci.cvma           Iterative Fibonacci calculation (CALL_BC + loop)
├── factorial.cvma           Factorial loop using CALL_BC subroutine
├── complex_algo.cvma        Euclidean GCD algorithm
├── collatz.cvma             Collatz (3n+1) sequence stopping time
├── binary_search.cvma       Binary search over sorted array elements
├── subroutines.cvma         Recursive assembly subroutines (CALL_BC / RET)
│
│── Data Structures & Memory
├── matrix_operations.cvma   Subroutines, LEA, dot product & 2×2 matrix trace
├── sorting_and_stats.cvma   Type conversions, double division & statistical mean
│
│── Hashing & Bit Algorithms
├── pseudo_random_hash.cvma  PRNG LCG, bitwise hash pipeline & bit shifts
├── hash_algorithms.cvma     DJB2, FNV-1a 32-bit & FNV-1a 64-bit subroutines
│
│── Extended Opcodes (new)
├── bit_ops.cvma             CLZ/CTZ/POPCNT, ROTL/ROTR, ABS, BOOL (i32 & i64)
├── float_intrinsics.cvma    ABS/SQRT/FLOOR/CEIL/TRUNC/ROUND, MIN/MAX/COPYSIGN, SELECT (f32 & f64)
├── int_extended.cvma        MIN/MAX signed & unsigned, MULH, CMP_U32/U64, IF_ULT/UGE/UGT/ULE
├── switch_demo.cvma         SWITCH dispatch tables — day-of-week, grade, season lookups
└── extended_algorithms.cvma Complex programs: popcount-sum, next-pow2 via CLZ, abs-diff, float pipeline
```

---

## How to Build and Run

### 1. Build Assembler & Assemble All `.cvma` Files

```sh
cd example2/
make
```

This compiles `cvma2cvmb` and assembles every `.cvma` file into a corresponding `.cvmb` binary.

### 2. Assemble and Execute All Files

```sh
make run
```

Assembles all `.cvma` files and runs each `.cvmb` through `vm_run`.

### 3. Assemble a Single File

```sh
./cvma2cvmb fibonacci.cvma -o fibonacci.cvmb --verbose
```

### 4. Run a Single `.cvmb` File

```sh
../example/vm_run fibonacci.cvmb
```

---

## CVM Assembly (`.cvma`) Syntax

| Element | Syntax | Example |
|---------|--------|---------|
| Registers | `r0`..`r255` (case-insensitive) | `r0`, `R15` |
| Integer constants | Signed decimal | `CONST_I32 r0, -42` |
| Float constants | Decimal | `CONST_F64 r1, 3.14159` |
| Labels | Identifier followed by `:` | `loop_start:` |
| Branch to label | Label name as operand | `IF_LT r0, r1, loop_start` |
| Unconditional jump | `GOTO` / `GOTO_16` / `GOTO_32` | `GOTO_16 main_entry` |
| Native call | `CALL dst, id, argc, r0..` | `CALL r0, 0, 2, r1, r2` |
| BC subroutine call | `CALL_BC dst, label, argc, r0..` | `CALL_BC r0, my_fn, 2, r1, r2` |
| Return from sub | `RET src` | `RET r0` |
| Register count directive | `.registers N` or `.regs N` | `.registers 16` |
| Comment | `#`, `;`, or `//` to end of line | `# this is a comment` |

### New Extended Opcodes in Assembly

```assembly
# Bit manipulation
CLZ_I32    r1, r0          # r1 = count leading zeros of r0 (32-bit)
CTZ_I64    r1, r0          # r1 = count trailing zeros of r0 (64-bit)
POPCNT_I32 r1, r0          # r1 = population count of r0
ROTL_I32   r2, r0, r1      # r2 = rotate r0 left by r1 bits (32-bit)
ROTR_I64   r2, r0, r1      # r2 = rotate r0 right by r1 bits (64-bit)

# Integer ABS / MIN / MAX
ABS_I32    r1, r0          # r1 = |r0|  (signed 32-bit)
MIN_I32    r2, r0, r1      # r2 = signed min(r0, r1)
MAX_U64    r2, r0, r1      # r2 = unsigned max(r0, r1)  (64-bit)

# High-half multiply
MULH_I32   r2, r0, r1      # r2 = high 32 bits of (signed) r0 * r1
MULH_U64   r2, r0, r1      # r2 = high 64 bits of (unsigned) r0 * r1

# Normalize to boolean
BOOL_I32   r1, r0          # r1 = (r0 != 0) ? 1 : 0

# Branchless select
SELECT     r3, r0, r1, r2  # r3 = (r2 != 0) ? r0 : r1

# Float intrinsics
ABS_F32    r1, r0          # r1 = |r0| (float)
SQRT_F64   r1, r0          # r1 = sqrt(r0) (double)
FLOOR_F32  r1, r0          # r1 = floor(r0) (float)
CEIL_F64   r1, r0          # r1 = ceil(r0) (double)
TRUNC_F32  r1, r0          # r1 = trunc(r0) toward zero
ROUND_F64  r1, r0          # r1 = round(r0) to nearest
MIN_F32    r2, r0, r1      # r2 = min(r0, r1)
COPYSIGN_F64 r2, r0, r1   # r2 = |r0| with sign of r1

# Unsigned comparison & branches
CMP_U32    r2, r0, r1      # r2 = unsigned cmp(r0,r1): -1/0/+1
IF_ULT     r0, r1, label   # branch if r0 < r1 (unsigned)
IF_UGE     r0, r1, label   # branch if r0 >= r1 (unsigned)

# SWITCH dispatch
SWITCH r0, 3, default_lbl, case0_lbl, case1_lbl, case2_lbl
```

---

## Sample Program

```assembly
# Compute sum 1..100 = 5050
.registers 6

CONST_I32  r0, 1          # i = 1
CONST_I32  r1, 100        # limit = 100
CONST_I32  r2, 0          # sum = 0
CONST_I32  r3, 1          # step = 1

loop_start:
ADD_I32    r2, r2, r0     # sum += i
ADD_I32    r0, r0, r3     # i++
IF_LE      r0, r1, loop_start

RETURN     r2             # Returns 5050
```

---

## Expected Return Values

| File | Description | `result.i32` |
|------|-------------|:---:|
| `basic.cvma` | Constants & moves | 42 |
| `arithmetic.cvma` | Integer arithmetic | 55 |
| `control_flow.cvma` | Loops & branches | 5050 |
| `fibonacci.cvma` | Fibonacci(15) | 610 |
| `factorial.cvma` | 10! | 3628800 |
| `complex_algo.cvma` | GCD(48, 18) | 6 |
| `collatz.cvma` | Collatz steps for 27 | 111 |
| `binary_search.cvma` | Index of target | varies |
| `hash_algorithms.cvma` | DJB2("Hello") | 2107146364 |
| `matrix_operations.cvma` | Trace(C) for 2×2 product | 69 |
| `bit_ops.cvma` | Bit-op checksum | 1073743513 |
| `float_intrinsics.cvma` | Float intrinsic checksum | 324 |
| `int_extended.cvma` | Int extended checksum | 47 |
| `switch_demo.cvma` | SWITCH table checksum | 96 |
| `extended_algorithms.cvma` | Algorithm checksum | 83 |
