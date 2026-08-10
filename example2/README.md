# Example 2 — CVM Assembly (.cvma) to Bytecode (.cvmb) Workflow

This directory demonstrates writing programs in **CVM Assembly text format (`.cvma`)**, assembling them into **CVM Binary Bytecode (`.cvmb`)** using `cvma2cvmb`, and running them with the `vm_run` command-line tool.

---

## Directory Overview

```
example2/
├── cvma2cvmb.c            CLI compiler/assembler tool (.cvma -> .cvmb)
├── basic.cvma             Basic constants, register moves, and return
├── arithmetic.cvma        Integer and floating-point arithmetic
├── control_flow.cvma      Branches, loops, and label definitions
├── native_calls.cvma      Native function calls (abs, max, print)
├── bitwise_shifts.cvma    Bitwise operations (AND, OR) and bit shifts (SHL, SHR)
├── conversions.cvma       Type conversions between int32 and double
├── floating_point.cvma    Double precision float calculations
├── comparison.cvma        Three-way comparisons (-1, 0, +1)
├── fibonacci.cvma         Iterative Fibonacci calculation loop
├── factorial.cvma         Factorial calculation loop
├── complex_algo.cvma      Euclidean GCD algorithm in assembly
├── unsigned_ops.cvma      Unsigned division/remainder and CMP_F32_GT
├── collatz.cvma           Collatz (3n+1) sequence stopping time algorithm
├── binary_search.cvma     Binary search over sorted array elements
├── subroutines.cvma        Recursive assembly subroutines (OP_CALL_BC / OP_RET)
├── matrix_operations.cvma  Subroutines, LEA pointer arithmetic, dot product & matrix trace
├── sorting_and_stats.cvma  Type conversions, double float division & statistical mean
├── pseudo_random_hash.cvma PRNG LCG algorithm, bitwise hash pipeline & bit shifts
├── hash_algorithms.cvma    DJB2, FNV-1a 32-bit, and FNV-1a 64-bit assembly subroutines
├── Makefile               Automates compilation, assembly, and execution
└── README.md              This documentation file
```

---

## How to Build and Run

### 1. Build Assembler & Assemble All `.cvma` Files

```sh
cd example2/
make
```

This compiles `cvma2cvmb` and assembles every `.cvma` file into a corresponding `.cvmb` binary file.

### 2. Assemble and Run All Bytecode Files

```sh
make run
```

This assembles all 19 `.cvma` files, compiles the `vm_run` CLI runner, and executes each generated `.cvmb` bytecode file in the VM.

### 3. Assemble a Single File Manually

```sh
./cvma2cvmb fibonacci.cvma -o fibonacci.cvmb --verbose
```

### 4. Execute a Single `.cvmb` File

```sh
../example/vm_run fibonacci.cvmb
```

---

## The CVM Assembly (`.cvma`) Syntax

- **Opcodes**: Case-insensitive instruction mnemonics (e.g. `CONST_I32`, `ADD_I32`, `IF_LT`, `CALL`, `RETURN`).
- **Registers**: Specified as `r0`..`r255` or `R0`..`R255`.
- **Labels**: Defined with a colon suffix (`label_name:`).
- **Branch Targets**: Can be specified by label name (`IF_LT r0, r1, loop_start`) or signed byte offset (`GOTO -12`).
- **Native Calls**: Written as `CALL dst, func_id, arg0, arg1...` or `CALL_VOID func_id, arg0...`
- **Comments**: Lines starting with `#`, `;`, or `//`.
- **Directives**: `.registers N` or `.regs N` to specify register count.

---

## Sample `.cvma` Code

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
