# C/C++ Register VM — Opcode Reference

Complete reference for every instruction in the VM bytecode instruction set.

---

## Table of Contents

1. [VM Architecture](#vm-architecture)
2. [Bytecode Encoding](#bytecode-encoding)
3. [Register File](#register-file)
4. [Error Codes](#error-codes)
5. [Instruction Reference](#instruction-reference)
   - [Miscellaneous](#miscellaneous)
   - [Data Movement](#data-movement)
   - [Integer Constants](#integer-constants)
   - [Float Constants](#float-constants)
   - [Integer Arithmetic — i32](#integer-arithmetic--i32)
   - [Integer Arithmetic — i64](#integer-arithmetic--i64)
   - [Unsigned Integer Arithmetic](#unsigned-integer-arithmetic)
   - [Float Arithmetic — f32](#float-arithmetic--f32)
   - [Float Arithmetic — f64](#float-arithmetic--f64)
   - [Unary Operations](#unary-operations)
   - [Bitwise — i32](#bitwise--i32)
   - [Bitwise — i64](#bitwise--i64)
   - [Shifts — i32](#shifts--i32)
   - [Shifts — i64](#shifts--i64)
   - [Comparisons](#comparisons)
   - [Type Conversions](#type-conversions)
   - [Memory Loads](#memory-loads)
   - [Memory Stores](#memory-stores)
   - [Pointer Arithmetic](#pointer-arithmetic)
   - [Unconditional Branches](#unconditional-branches)
   - [Conditional Branches — Register Pair](#conditional-branches--register-pair)
   - [Conditional Branches — vs Zero](#conditional-branches--vs-zero)
   - [Native Function Calls](#native-function-calls)
   - [Return](#return)
6. [Opcode Encoding Summary](#opcode-encoding-summary)
7. [Public API](#public-api)

---

## VM Architecture

The VM is a **register-based** virtual machine. There is no implicit operand stack —
all operands and results live in an explicitly indexed register file provided by the
host at execution time.

```
+--------------+     vm_execute()      +-------------------+
|  Bytecode    | --------------------> |  Dispatch Loop    |
|  Buffer      |                       |  (switch / pc++)  |
+--------------+                       +--------+----------+
                                                |  reads/writes
                                       +--------v----------+
                                       |  Register File    |
                                       |  regs[0..N-1]     |
                                       +-------------------+
                                                |  calls into
                                       +--------v----------+
                                       |  Native Function  |
                                       |  Table (256 max)  |
                                       +-------------------+
```

Key properties:

| Property | Value |
|----------|-------|
| Register width | 64 bits (union of all primitive types) |
| Max registers | 256 (u8 index in most instructions; MOVE allows u16 src) |
| Max native functions | 256 |
| Max call arguments | 64 |
| Endianness | Little-endian throughout |
| Integer overflow | Wraps (two's complement, C semantics) |
| Float behaviour | IEEE-754 (NaN and Inf propagate naturally) |
| Integer div/0 | Traps with `VM_ERR_DIV_ZERO` |
| Float div/0 | Returns `Inf` or `NaN` (no trap) |

---

## Bytecode Encoding

Every instruction begins with a **16-bit little-endian opcode** immediately followed
by zero or more operand bytes. There is no padding or alignment requirement.

```
+----------+----------+---------------------------+
| opcode   | opcode   |  operand bytes ...        |
| low byte | high byte|  (instruction-specific)   |
+----------+----------+---------------------------+
  byte 0     byte 1     bytes 2, 3, ...
```

**Operand type notation used throughout this document:**

| Notation | Width | Description |
|----------|-------|-------------|
| `u8`   | 1 byte    | Unsigned 8-bit integer |
| `i8`   | 1 byte    | Signed 8-bit integer |
| `u16`  | 2 bytes LE | Unsigned 16-bit integer |
| `i16`  | 2 bytes LE | Signed 16-bit integer |
| `u32`  | 4 bytes LE | Unsigned 32-bit integer |
| `i32`  | 4 bytes LE | Signed 32-bit integer |
| `u64`  | 8 bytes LE | Unsigned 64-bit integer |
| `i64`  | 8 bytes LE | Signed 64-bit integer |
| `dst`  | u8 | Destination register index |
| `src`  | u8 | Source register index |
| `lhs`  | u8 | Left-hand operand register index |
| `rhs`  | u8 | Right-hand operand register index |
| `addr` | u8 | Register holding a memory address (`.ptr` member) |
| `base` | u8 | Register holding a base pointer (`.ptr` member) |

**Branch offset semantics:**

All branch displacements are **signed** and **relative to the first byte of the
instruction immediately after the branch** (i.e. the "next" instruction):

```
target_pc = next_instruction_pc + offset

offset = 0   -> no-op (fall through)
offset > 0   -> skip forward
offset < 0   -> loop back
```

---

## Register File

Registers are **untyped 64-bit unions**. The VM never tracks what type is stored in
a register — the bytecode program is responsible for using the correct member.

```c
typedef union {
    int8_t   i8;    /* signed   8-bit integer */
    uint8_t  u8;    /* unsigned 8-bit integer */
    int16_t  i16;   /* signed  16-bit integer */
    uint16_t u16;   /* unsigned 16-bit integer */
    int32_t  i32;   /* signed  32-bit integer */
    uint32_t u32;   /* unsigned 32-bit integer */
    int64_t  i64;   /* signed  64-bit integer */
    uint64_t u64;   /* unsigned 64-bit integer */
    float    f32;   /* 32-bit IEEE-754 float  */
    double   f64;   /* 64-bit IEEE-754 double */
    void*    ptr;   /* host pointer           */
} VMRegister;
```

All registers begin at whatever value the host initialises them to (typically zero
via `memset`). The host can pre-load registers with pointer values before calling
`vm_execute()` to pass host objects into the bytecode.

> **Important:** Integer constant instructions (`CONST_I8`, `CONST_I16`, `CONST_I32`)
> **sign-extend** the immediate value into all 64 bits of the register. Reading `.i64`
> on a register set by `CONST_I32(-1)` yields `-1LL`, not `0x00000000FFFFFFFF`.

---

## Error Codes

| Constant | Value | Meaning |
|----------|-------|---------|
| `VM_OK` | 0 | Execution completed successfully |
| `VM_ERR_INVALID_OPCODE` | 1 | The 16-bit opcode at the current `pc` is not recognised |
| `VM_ERR_OUT_OF_BOUNDS` | 2 | `pc` advanced past the end of the bytecode buffer, or a branch target is outside `[0, bytecode_size)` |
| `VM_ERR_DIV_ZERO` | 3 | Integer `DIV` or `REM` with a zero divisor |
| `VM_ERR_INVALID_REGISTER` | 4 | An instruction references a register index `>= reg_count` |
| `VM_ERR_BAD_FUNCTION` | 5 | `CALL`/`CALL_VOID` with an unregistered function id, a null slot, or `id >= 256` |
| `VM_ERR_BAD_ARGC` | 6 | `CALL`/`CALL_VOID` with `argc > VM_MAX_CALL_ARGC` (64) |

> **Note:** Float division by zero is **not** an error — it produces `Inf` or `NaN`
> per IEEE-754. Only integer division by zero traps.

---

## Instruction Reference

---

### Miscellaneous

---

#### `OP_NOP` — No Operation

```
Encoding:  [0x00 0x00]
Size:      2 bytes
```

Does nothing. Advances the program counter by 2. Useful for alignment, placeholder
slots during code generation, or patching out instructions without relocating jumps.

---

### Data Movement

---

#### `OP_MOVE` — Copy Register

```
Encoding:  [op:u16][dst:u8][src:u16]
Size:      5 bytes
```

Copies the full 64-bit value of register `src` into register `dst`. The `src`
operand is **16 bits wide**, allowing all registers 0–255 as both source and
destination. The entire union is copied bit-for-bit; no type conversion occurs.

```
regs[dst] = regs[src]   (all 64 bits)
```

```c
// Copy r5 -> r0
emit_move(&bc, 0, 5);

// Swap r0 and r1 (using r2 as temp)
emit_move(&bc, 2, 0);  // r2 = r0
emit_move(&bc, 0, 1);  // r0 = r1
emit_move(&bc, 1, 2);  // r1 = r2
```

---

### Integer Constants

All integer constant instructions sign-extend the immediate value to fill the entire
64-bit register. A subsequent read of any member (`.i8`, `.i32`, `.i64`, `.u64`)
reflects the sign-extended value.

---

#### `OP_CONST_I8` — Load Signed 8-bit Constant

```
Encoding:  [op:u16][dst:u8][val:i8]
Size:      4 bytes
```

Sign-extends `val` (-128..127) to 64 bits and stores it in `regs[dst]`.

```
regs[dst].i64 = (int64_t)(int8_t)val
```

---

#### `OP_CONST_I16` — Load Signed 16-bit Constant

```
Encoding:  [op:u16][dst:u8][val:i16]
Size:      5 bytes
```

```
regs[dst].i64 = (int64_t)(int16_t)val
```

---

#### `OP_CONST_I32` — Load Signed 32-bit Constant

```
Encoding:  [op:u16][dst:u8][val:i32]
Size:      7 bytes
```

The most commonly used constant loader. Sign-extends to 64 bits.

```
regs[dst].i64 = (int64_t)(int32_t)val
```

```c
emit_const_i32(&bc, 0, 0);
emit_const_i32(&bc, 1, 2147483647);  // INT32_MAX
emit_const_i32(&bc, 2, -1);          // r2.i64 == -1LL
```

> **Note:** To load `0xFFFFFFFF` as unsigned (not sign-extended), use `CONST_I64`.

---

#### `OP_CONST_I64` — Load Signed 64-bit Constant

```
Encoding:  [op:u16][dst:u8][val:i64]
Size:      11 bytes
```

Stores the full 64-bit value directly.

```
regs[dst].i64 = val
```

---

### Float Constants

Float constants are encoded as their IEEE-754 bit patterns in little-endian order.
The `vm_builder.h` helpers accept host `float`/`double` values and extract bits
automatically.

---

#### `OP_CONST_F32` — Load 32-bit Float Constant

```
Encoding:  [op:u16][dst:u8][bits:u32]
Size:      7 bytes
```

Writes the 32-bit IEEE-754 bit pattern directly. Reading `regs[dst].f32` yields the
original float.

```
memcpy(&regs[dst].f32, &bits, 4)
```

---

#### `OP_CONST_F64` — Load 64-bit Float Constant

```
Encoding:  [op:u16][dst:u8][bits:u64]
Size:      11 bytes
```

```
memcpy(&regs[dst].f64, &bits, 8)
```

---

### Integer Arithmetic — i32

All i32 arithmetic reads operands as `int32_t` and writes results as `int32_t`.
Overflow wraps silently (two's-complement). Encoding for all binary ops:
`[op:u16][dst:u8][lhs:u8][rhs:u8]` — **5 bytes**.

---

#### `OP_ADD_I32` — Integer Addition (32-bit)

```
regs[dst].i32 = regs[lhs].i32 + regs[rhs].i32
```

---

#### `OP_SUB_I32` — Integer Subtraction (32-bit)

```
regs[dst].i32 = regs[lhs].i32 - regs[rhs].i32
```

---

#### `OP_MUL_I32` — Integer Multiplication (32-bit)

```
regs[dst].i32 = regs[lhs].i32 * regs[rhs].i32
```

Overflow wraps (low 32 bits of full product).

---

#### `OP_DIV_I32` — Integer Division (32-bit)

```
regs[dst].i32 = regs[lhs].i32 / regs[rhs].i32   (truncate toward zero)
```

> **Caution:** Traps `VM_ERR_DIV_ZERO` if `regs[rhs].i32 == 0`.
> `INT32_MIN / -1` is handled safely and returns `INT32_MIN`.

---

#### `OP_REM_I32` — Integer Remainder (32-bit)

```
regs[dst].i32 = regs[lhs].i32 % regs[rhs].i32
```

Result has the same sign as the dividend (C `%` semantics):
`-7 % 3 = -1`, `7 % -3 = 1`.

> **Caution:** Traps `VM_ERR_DIV_ZERO` if `regs[rhs].i32 == 0`.

---

### Integer Arithmetic — i64

Identical semantics to i32 variants but operates on 64-bit `i64`. Encoding:
`[op:u16][dst:u8][lhs:u8][rhs:u8]` — **5 bytes** each.

#### `OP_ADD_I64`
```
regs[dst].i64 = regs[lhs].i64 + regs[rhs].i64
```

#### `OP_SUB_I64`
```
regs[dst].i64 = regs[lhs].i64 - regs[rhs].i64
```

#### `OP_MUL_I64`
```
regs[dst].i64 = regs[lhs].i64 * regs[rhs].i64
```

#### `OP_DIV_I64`
```
regs[dst].i64 = regs[lhs].i64 / regs[rhs].i64
```
> **Caution:** Traps `VM_ERR_DIV_ZERO` if divisor is 0. `INT64_MIN / -1` returns `INT64_MIN`.

#### `OP_REM_I64`
```
regs[dst].i64 = regs[lhs].i64 % regs[rhs].i64
```
> **Caution:** Traps `VM_ERR_DIV_ZERO` if divisor is 0.

---

### Unsigned Integer Arithmetic

Division and remainder operations interpreting registers as **unsigned integers** (`u32` / `u64`).
Encoding: `[op:u16][dst:u8][lhs:u8][rhs:u8]` — **5 bytes** each.

#### `OP_DIV_U32` — Unsigned 32-bit Division
```
regs[dst].u32 = regs[lhs].u32 / regs[rhs].u32
```
> **Caution:** Traps `VM_ERR_DIV_ZERO` if divisor is 0. Does not overflow (unsigned division).

#### `OP_REM_U32` — Unsigned 32-bit Remainder
```
regs[dst].u32 = regs[lhs].u32 % regs[rhs].u32
```
> **Caution:** Traps `VM_ERR_DIV_ZERO` if divisor is 0.

#### `OP_DIV_U64` — Unsigned 64-bit Division
```
regs[dst].u64 = regs[lhs].u64 / regs[rhs].u64
```
> **Caution:** Traps `VM_ERR_DIV_ZERO` if divisor is 0.

#### `OP_REM_U64` — Unsigned 64-bit Remainder
```
regs[dst].u64 = regs[lhs].u64 % regs[rhs].u64
```
> **Caution:** Traps `VM_ERR_DIV_ZERO` if divisor is 0.

---

### Float Arithmetic — f32

Reads operands as `float`, writes result as `float`. NaN and Inf propagate per
IEEE-754. Division by zero produces `+/-Inf` or `NaN` — **not** a VM error.
Encoding: `[op:u16][dst:u8][lhs:u8][rhs:u8]` — **5 bytes** each.

#### `OP_ADD_F32`
```
regs[dst].f32 = regs[lhs].f32 + regs[rhs].f32
```

#### `OP_SUB_F32`
```
regs[dst].f32 = regs[lhs].f32 - regs[rhs].f32
```

#### `OP_MUL_F32`
```
regs[dst].f32 = regs[lhs].f32 * regs[rhs].f32
```

#### `OP_DIV_F32`
```
regs[dst].f32 = regs[lhs].f32 / regs[rhs].f32
```

| Dividend | Divisor | Result |
|----------|---------|--------|
| non-zero | `0.0f`  | `+/-Inf` |
| `0.0f`   | `0.0f`  | `NaN` |
| `NaN`    | anything | `NaN` |

---

### Float Arithmetic — f64

Identical semantics to f32 but operates on `double`. Encoding:
`[op:u16][dst:u8][lhs:u8][rhs:u8]` — **5 bytes** each.

#### `OP_ADD_F64`
```
regs[dst].f64 = regs[lhs].f64 + regs[rhs].f64
```

#### `OP_SUB_F64`
```
regs[dst].f64 = regs[lhs].f64 - regs[rhs].f64
```

#### `OP_MUL_F64`
```
regs[dst].f64 = regs[lhs].f64 * regs[rhs].f64
```

#### `OP_DIV_F64`
```
regs[dst].f64 = regs[lhs].f64 / regs[rhs].f64
```

---

### Unary Operations

Encoding for all unary ops: `[op:u16][dst:u8][src:u8]` — **4 bytes**.
`dst` and `src` may be the same register.

---

#### `OP_NEG_I32` — Arithmetic Negate (32-bit)

```
regs[dst].i32 = -regs[src].i32
```

`NEG_I32(INT32_MIN)` wraps to `INT32_MIN` (two's-complement).

---

#### `OP_NEG_I64` — Arithmetic Negate (64-bit)

```
regs[dst].i64 = -regs[src].i64
```

---

#### `OP_NEG_F32` — Float Negate (32-bit)

```
regs[dst].f32 = -regs[src].f32
```

Flips the IEEE-754 sign bit. `NEG_F32(NaN)` produces a NaN with opposite sign bit
(still NaN). `NEG_F32(0.0f)` produces `-0.0f`.

---

#### `OP_NEG_F64` — Float Negate (64-bit)

```
regs[dst].f64 = -regs[src].f64
```

---

#### `OP_NOT_I32` — Bitwise Complement (32-bit)

```
regs[dst].i32 = ~regs[src].i32
```

Flips all 32 bits. `NOT_I32(0) = -1`, `NOT_I32(-1) = 0`.

---

#### `OP_NOT_I64` — Bitwise Complement (64-bit)

```
regs[dst].i64 = ~regs[src].i64
```

---

### Bitwise — i32

Encoding: `[op:u16][dst:u8][lhs:u8][rhs:u8]` — **5 bytes**.
Operands read as `uint32_t`; result stored in the `i32`/`u32` slot (same bits).

---

#### `OP_AND_I32` — Bitwise AND (32-bit)

```
regs[dst].u32 = regs[lhs].u32 & regs[rhs].u32
```

**Common uses:** masking bits, checking flags, extracting fields.

```c
// Extract low byte of r0 -> r1  (r2 = 0xFF)
emit_and_i32(&bc, 1, 0, 2);

// Check if bit 4 is set
emit_const_i32(&bc, 2, 0x10);
emit_and_i32(&bc, 3, 0, 2);   // r3 != 0 iff bit 4 is set
```

---

#### `OP_OR_I32` — Bitwise OR (32-bit)

```
regs[dst].u32 = regs[lhs].u32 | regs[rhs].u32
```

**Common uses:** setting flags, combining bit fields.

---

#### `OP_XOR_I32` — Bitwise XOR (32-bit)

```
regs[dst].u32 = regs[lhs].u32 ^ regs[rhs].u32
```

**Common uses:** toggling bits, equality check (`a ^ a = 0`).

```c
// Zero a register: XOR with itself
emit_xor_i32(&bc, 0, 0, 0);  // r0 = 0
```

---

### Bitwise — i64

Identical semantics to i32 variants but operates on 64 bits.
Encoding: `[op:u16][dst:u8][lhs:u8][rhs:u8]` — **5 bytes** each.

#### `OP_AND_I64`
```
regs[dst].u64 = regs[lhs].u64 & regs[rhs].u64
```

#### `OP_OR_I64`
```
regs[dst].u64 = regs[lhs].u64 | regs[rhs].u64
```

#### `OP_XOR_I64`
```
regs[dst].u64 = regs[lhs].u64 ^ regs[rhs].u64
```

---

### Shifts — i32

```
Encoding:  [op:u16][dst:u8][val:u8][amt:u8]
Size:      5 bytes
```

The shift amount is taken from `regs[amt].i32` and **masked to the low 5 bits**
(`& 31`) before use, making amounts 0–31 always valid. The `val` register holds the
value to shift.

---

#### `OP_SHL_I32` — Left Shift (32-bit)

```
regs[dst].u32 = (uint32_t)regs[val].u32 << (regs[amt].i32 & 31)
```

Vacated low bits are filled with zeros. Equivalent to multiplying by a power of 2
for non-negative values.

```c
// r2 = r0 << r1  (r1 holds the shift amount)
emit_shl_i32(&bc, 2, 0, 1);

// Multiply r0 by 8 via shift (r3 = 3)
emit_const_i32(&bc, 3, 3);
emit_shl_i32(&bc, 2, 0, 3);
```

---

#### `OP_SHR_I32` — Arithmetic Right Shift (32-bit)

```
regs[dst].i32 = regs[val].i32 >> (regs[amt].i32 & 31)
```

**Arithmetic** (signed) right shift: the sign bit is replicated into vacated high bits.
Negative values remain negative.

| Input | Amount | Result |
|-------|--------|--------|
| `-8`  | 1      | `-4`   |
| `-1`  | 31     | `-1`   |
| `256` | 4      | `16`   |

---

#### `OP_USHR_I32` — Logical Right Shift (32-bit)

```
regs[dst].u32 = (uint32_t)regs[val].u32 >> (regs[amt].i32 & 31)
```

**Logical** (unsigned) right shift: high bits are filled with zeros regardless of sign.

| Input (i32) | Amount | Result (u32) |
|-------------|--------|--------------|
| `-1` (0xFFFFFFFF) | 1 | `0x7FFFFFFF` |
| `-256` (0xFFFFFF00) | 4 | `0x0FFFFFF0` |
| `256` | 4 | `16` |

> **Tip:** Use `SHR_I32` for signed right shift and `USHR_I32` for unsigned right
> shift. They differ only for negative inputs.

---

### Shifts — i64

Same semantics as i32 shifts, but the shift amount is masked to **6 bits** (`& 63`)
and the value is 64 bits wide.
Encoding: `[op:u16][dst:u8][val:u8][amt:u8]` — **5 bytes** each.

#### `OP_SHL_I64`
```
regs[dst].u64 = (uint64_t)regs[val].u64 << (regs[amt].i32 & 63)
```

#### `OP_SHR_I64` — Arithmetic right shift (64-bit)
```
regs[dst].i64 = regs[val].i64 >> (regs[amt].i32 & 63)
```

#### `OP_USHR_I64` — Logical right shift (64-bit)
```
regs[dst].u64 = (uint64_t)regs[val].u64 >> (regs[amt].i32 & 63)
```

---

### Comparisons

```
Encoding:  [op:u16][dst:u8][lhs:u8][rhs:u8]
Size:      5 bytes
```

All comparison instructions produce a **three-way result** stored as `int32_t` in
`regs[dst]`:

| Condition | Result |
|-----------|--------|
| `lhs < rhs` | `-1` |
| `lhs == rhs` | `0` |
| `lhs > rhs` | `+1` |

The result can be inspected with the `IF_LTZ` / `IF_EQZ` / `IF_GTZ` branch family,
or used as an integer in subsequent arithmetic.

---

#### `OP_CMP_I32` — Compare Signed 32-bit Integers

```
regs[dst].i32 = (regs[lhs].i32 < regs[rhs].i32) ? -1
              : (regs[lhs].i32 > regs[rhs].i32) ? +1 : 0
```

---

#### `OP_CMP_I64` — Compare Signed 64-bit Integers

```
regs[dst].i32 = (regs[lhs].i64 < regs[rhs].i64) ? -1
              : (regs[lhs].i64 > regs[rhs].i64) ? +1 : 0
```

---

#### `OP_CMP_F32` — Compare 32-bit Floats

```
regs[dst].i32 = (either operand is NaN)         ? -1
              : (regs[lhs].f32 < regs[rhs].f32) ? -1
              : (regs[lhs].f32 > regs[rhs].f32) ? +1 : 0
```

> **Important:** If **either** operand is NaN, the result is always `-1`. This
> provides a consistent, non-signalling NaN comparison result.

| lhs | rhs | Result |
|-----|-----|--------|
| `1.0f` | `2.0f` | `-1` |
| `3.0f` | `3.0f` | `0` |
| `5.0f` | `2.0f` | `+1` |
| `NaN` | `1.0f` | `-1` |
| `1.0f` | `NaN` | `-1` |
| `+Inf` | `1.0f` | `+1` |
| `-Inf` | `1.0f` | `-1` |

---

#### `OP_CMP_F64` — Compare 64-bit Floats

```
regs[dst].i32 = (either is NaN)                 ? -1
              : (regs[lhs].f64 < regs[rhs].f64) ? -1
              : (regs[lhs].f64 > regs[rhs].f64) ? +1 : 0
```

Same NaN semantics as `CMP_F32`.

---

#### `OP_CMP_F32_GT` — Compare 32-bit Floats (Ordered-Greater / NaN → +2)

```
regs[dst].i32 = (either operand is NaN)         ? +2
              : (regs[lhs].f32 < regs[rhs].f32) ? -1
              : (regs[lhs].f32 > regs[rhs].f32) ? +1 : 0
```

> **Important:** Unlike `CMP_F32` which returns `-1` on NaN (unordered-less, matching Java `fcmpl`),
> `CMP_F32_GT` returns **`+2`** on NaN (unordered-greater, matching Java `fcmpg`).
> This variant allows complete and unambiguous compilation of floating-point conditional jumps when NaN operands may be present.

| lhs | rhs | Result |
|-----|-----|--------|
| `1.0f` | `2.0f` | `-1` |
| `3.0f` | `3.0f` | `0` |
| `5.0f` | `2.0f` | `+1` |
| `NaN` | `1.0f` | `+2` |
| `1.0f` | `NaN` | `+2` |

---

### Type Conversions

```
Encoding:  [op:u16][dst:u8][src:u8]
Size:      4 bytes
```

`dst` and `src` may be the same register (in-place conversion).

---

#### `OP_I32_TO_I8` — Narrow i32 to i8

```
regs[dst].i64 = (int64_t)(int8_t)(int32_t)regs[src].i32
```

Truncates to the low 8 bits, then sign-extends to 64 bits. Values outside -128..127 wrap.

| Input | Result |
|-------|--------|
| `127` | `127` |
| `128` | `-128` (wraps) |
| `300` | `44` (300 & 0xFF = 44, no sign flip) |
| `-1` | `-1` |

---

#### `OP_I32_TO_I16` — Narrow i32 to i16

```
regs[dst].i64 = (int64_t)(int16_t)(int32_t)regs[src].i32
```

Truncates to 16 bits, sign-extends. Values outside -32768..32767 wrap.

---

#### `OP_I32_TO_I64` — Sign-extend i32 to i64

```
regs[dst].i64 = (int64_t)(int32_t)regs[src].i32
```

Widens with sign extension. Most common way to promote a 32-bit integer for
64-bit arithmetic.

```c
emit_i32_to_i64(&bc, 1, 0);  // r1 = (i64)r0
emit_add_i64(&bc, 2, 1, 3);
```

---

#### `OP_I32_TO_F32` — Convert i32 to f32

```
regs[dst].f32 = (float)(int32_t)regs[src].i32
```

May lose precision for large values (floats have only 24-bit mantissa).

---

#### `OP_I32_TO_F64` — Convert i32 to f64

```
regs[dst].f64 = (double)(int32_t)regs[src].i32
```

Always exact — all int32 values are representable as double (53-bit mantissa).

---

#### `OP_I64_TO_I32` — Truncate i64 to i32

```
regs[dst].i32 = (int32_t)(int64_t)regs[src].i64
```

Discards high 32 bits. Result is sign-extended into the full register.
`INT64_MAX` (0x7FFFFFFFFFFFFFFF) becomes `-1` as i32.

---

#### `OP_I64_TO_F32` — Convert i64 to f32

```
regs[dst].f32 = (float)(int64_t)regs[src].i64
```

May lose precision for large values.

---

#### `OP_I64_TO_F64` — Convert i64 to f64

```
regs[dst].f64 = (double)(int64_t)regs[src].i64
```

Exact for values within 2^53.

---

#### `OP_F32_TO_I32` — Truncate f32 to i32

```
regs[dst].i32 = (int32_t)(float)regs[src].f32
```

Truncates toward zero (C cast semantics). `3.9f -> 3`, `-3.9f -> -3`.
Result is undefined for NaN, Inf, or out-of-range values.

---

#### `OP_F32_TO_I64` — Truncate f32 to i64

```
regs[dst].i64 = (int64_t)(float)regs[src].f32
```

---

#### `OP_F32_TO_F64` — Widen f32 to f64

```
regs[dst].f64 = (double)(float)regs[src].f32
```

Always exact (f64 is a superset of f32 range and precision).

---

#### `OP_F64_TO_I32` — Truncate f64 to i32

```
regs[dst].i32 = (int32_t)(double)regs[src].f64
```

---

#### `OP_F64_TO_I64` — Truncate f64 to i64

```
regs[dst].i64 = (int64_t)(double)regs[src].f64
```

---

#### `OP_F64_TO_F32` — Narrow f64 to f32

```
regs[dst].f32 = (float)(double)regs[src].f64
```

Rounds to nearest representable float. May lose precision.

---

### Memory Loads

```
Encoding:  [op:u16][dst:u8][addr:u8]
Size:      4 bytes
```

`regs[addr].ptr` must be a valid host pointer. The VM does **not** bounds-check host
memory — the host is responsible for pointer validity.

---

#### `OP_LOAD8` — Load Unsigned Byte

```
regs[dst].u64 = *(uint8_t*)regs[addr].ptr
```

Zero-extends to 64 bits. Result is in `[0, 255]`.

---

#### `OP_LOAD8S` — Load Signed Byte

```
regs[dst].i64 = *(int8_t*)regs[addr].ptr
```

Sign-extends to 64 bits. Result is in `[-128, 127]`.

---

#### `OP_LOAD16` — Load Unsigned 16-bit

```
regs[dst].u64 = *(uint16_t*)regs[addr].ptr
```

---

#### `OP_LOAD16S` — Load Signed 16-bit

```
regs[dst].i64 = *(int16_t*)regs[addr].ptr
```

---

#### `OP_LOAD32` — Load Unsigned 32-bit

```
regs[dst].u64 = *(uint32_t*)regs[addr].ptr
```

---

#### `OP_LOAD32S` — Load Signed 32-bit

```
regs[dst].i64 = *(int32_t*)regs[addr].ptr
```

The most common load for C `int` fields in host structs.

---

#### `OP_LOAD64` — Load 64-bit

```
regs[dst].u64 = *(uint64_t*)regs[addr].ptr
```

---

#### `OP_LOAD_PTR` — Load Pointer

```
regs[dst].ptr = *(void**)regs[addr].ptr
```

Dereferences a pointer-to-pointer. Used to traverse pointer chains.

```c
// r0 = &pp (void**); dereference twice to get the int
emit_load_ptr(&bc, 1, 0);   // r1 = *pp  (void*)
emit_load32s(&bc, 2, 1);    // r2 = **pp (int)
```

---

### Memory Stores

```
Encoding:  [op:u16][addr:u8][src:u8]
Size:      4 bytes
```

Writes the low bits of `regs[src]` to the memory at `regs[addr].ptr`. High bits of
`src` are silently truncated for narrow stores.

---

#### `OP_STORE8` — Store Byte

```
*(uint8_t*)regs[addr].ptr = (uint8_t)regs[src].u8
```

---

#### `OP_STORE16` — Store 16-bit

```
*(uint16_t*)regs[addr].ptr = (uint16_t)regs[src].u16
```

---

#### `OP_STORE32` — Store 32-bit

```
*(uint32_t*)regs[addr].ptr = (uint32_t)regs[src].u32
```

The most common store for C `int` fields.

---

#### `OP_STORE64` — Store 64-bit

```
*(uint64_t*)regs[addr].ptr = regs[src].u64
```

---

#### `OP_STORE_PTR` — Store Pointer

```
*(void**)regs[addr].ptr = regs[src].ptr
```

Writes a pointer-sized value. Used to update pointer slots in host structs or arrays.

---

### Pointer Arithmetic

---

#### `OP_LEA` — Load Effective Address

```
Encoding:  [op:u16][dst:u8][base:u8][offset:i32]
Size:      8 bytes
```

```
regs[dst].ptr = (char*)regs[base].ptr + offset
```

Computes a byte-offset address from a base pointer and a **literal signed 32-bit
offset** embedded in the instruction.

```c
// Advance pointer by sizeof(int32_t) = 4 bytes
emit_lea(&bc, 0, 0, 4);

// Access struct field at byte offset 8
emit_lea(&bc, 1, 0, 8);

// Move backward one element
emit_lea(&bc, 2, 2, -4);
```

> **Important:** `offset` is a **literal i32** in the instruction encoding, not a
> register index. For variable byte offsets, compute the address using integer
> arithmetic and use the resulting register directly as `addr` in LOAD/STORE.

**Array traversal pattern:**
```c
// r0 = &arr[0]; iterate sizeof(int32_t)=4 bytes per element
uint32_t loop = bc.size;
emit_load32s(&bc, 2, 0);         // r2 = *r0
emit_add_i32(&bc, 3, 3, 2);      // sum += r2
emit_lea(&bc, 0, 0, 4);          // r0 += 4 (next element)
emit_sub_i32(&bc, 1, 1, one);    // count--
// branch back to loop ...
```

**Struct field access pattern:**
```c
// r0 = &obj; field `y` at byte offset 4
emit_lea(&bc, 1, 0, 4);
emit_load32s(&bc, 2, 1);         // r2 = obj.y
```

---

### Unconditional Branches

All three `GOTO` variants jump unconditionally. The displacement is **signed** and
**relative to the next instruction** (the byte after the last operand byte).

---

#### `OP_GOTO` — Short Jump

```
Encoding:  [op:u16][offset:i8]
Size:      3 bytes
```

Range: -128..+127 bytes. Use for tight loops where size matters.

```
pc = next_pc + offset
```

---

#### `OP_GOTO_16` — Medium Jump

```
Encoding:  [op:u16][offset:i16]
Size:      4 bytes
```

Range: -32768..+32767 bytes. The most commonly used unconditional jump.

---

#### `OP_GOTO_32` — Long Jump

```
Encoding:  [op:u16][offset:i32]
Size:      6 bytes
```

Range: +/-2 GB. Use for very large programs.

---

### Conditional Branches — Register Pair

```
Encoding:  [op:u16][A:u8][B:u8][offset:i16]
Size:      6 bytes
```

Compares `regs[A].i32` and `regs[B].i32` as **signed 32-bit integers**. If the
condition is true, jumps by `offset` bytes from the next instruction. Otherwise
falls through.

---

#### `OP_IF_EQ` — Branch if Equal
```
if (regs[A].i32 == regs[B].i32) pc += offset
```

#### `OP_IF_NE` — Branch if Not Equal
```
if (regs[A].i32 != regs[B].i32) pc += offset
```

#### `OP_IF_LT` — Branch if Less Than
```
if (regs[A].i32 < regs[B].i32) pc += offset
```

#### `OP_IF_GE` — Branch if Greater or Equal
```
if (regs[A].i32 >= regs[B].i32) pc += offset
```

#### `OP_IF_GT` — Branch if Greater Than
```
if (regs[A].i32 > regs[B].i32) pc += offset
```

#### `OP_IF_LE` — Branch if Less or Equal
```
if (regs[A].i32 <= regs[B].i32) pc += offset
```

**Example — for loop `i = 0; i < 10; i++`:**

```c
emit_const_i32(&bc, 0, 0);    // r0 = i
emit_const_i32(&bc, 1, 10);   // r1 = limit
emit_const_i32(&bc, 2, 1);    // r2 = step

uint32_t loop_top = bc.size;
// ... loop body ...
emit_add_i32(&bc, 0, 0, 2);   // i++
uint32_t p = emit_if_lt(&bc, 0, 1);  // if i < 10, loop back
bc_patch_back(&bc, p, loop_top);
```

---

### Conditional Branches — vs Zero

```
Encoding:  [op:u16][A:u8][offset:i16]
Size:      5 bytes
```

Compares `regs[A].i32` against zero. If the condition is true, jumps by `offset`
bytes from the next instruction.

---

#### `OP_IF_EQZ` — Branch if Equal to Zero
```
if (regs[A].i32 == 0) pc += offset
```

#### `OP_IF_NEZ` — Branch if Not Equal to Zero
```
if (regs[A].i32 != 0) pc += offset
```

#### `OP_IF_LTZ` — Branch if Less Than Zero
```
if (regs[A].i32 < 0) pc += offset
```

#### `OP_IF_GEZ` — Branch if Greater or Equal to Zero
```
if (regs[A].i32 >= 0) pc += offset
```

#### `OP_IF_GTZ` — Branch if Greater Than Zero
```
if (regs[A].i32 > 0) pc += offset
```

#### `OP_IF_LEZ` — Branch if Less or Equal to Zero
```
if (regs[A].i32 <= 0) pc += offset
```

**Example — while loop:**
```c
// while (count != 0) { ...; count--; }
uint32_t loop_top = bc.size;
// ... body ...
emit_sub_i32(&bc, 0, 0, one_reg);
uint32_t p = emit_if_nez(&bc, 0);
bc_patch_back(&bc, p, loop_top);
```

**Example — if/else:**
```c
// if (r0 == 0) { r1 = 100; } else { r1 = 200; }
uint32_t skip = emit_if_nez(&bc, 0);  // if r0 != 0, skip then
emit_const_i32(&bc, 1, 100);
uint32_t end = emit_goto_16_fwd(&bc);
bc_patch_here(&bc, skip);             // else block
emit_const_i32(&bc, 1, 200);
bc_patch_here(&bc, end);
```

---

### Native Function Calls

Native functions are C functions registered via `vm_register_function()` before
execution. They bridge the VM and host environment for I/O, system calls, complex
math, or any operation not expressible in bytecode.

---

#### `OP_CALL` — Call Native Function (with return value)

```
Encoding:  [op:u16][dst:u8][id:u32][argc:u8][r0:u8]...[rN:u8]
Size:      8 + argc bytes
```

1. Looks up `id` in the native function table.
   Traps `VM_ERR_BAD_FUNCTION` if `id >= 256` or the slot is null.
   Traps `VM_ERR_BAD_ARGC` if `argc > 64`.
2. Builds an argument array from registers `r0..rN` in listed order.
3. Calls `native_funcs[id](ctx, argc, args, &out)`.
4. Stores `out` in both `regs[dst]` and `ctx->result`.
5. If the native returns a non-zero `VMError`, execution halts.

```c
// Call function id=2 with args r1, r2 -> result in r0
emit_call2(&bc, 0, 2, 1, 2);
```

**Registering a native function:**
```c
static VMError my_add(VMContext* ctx, uint32_t argc,
                      VMRegister* args, VMRegister* out)
{
    (void)ctx;
    if (argc < 2) return VM_ERR_BAD_ARGC;
    out->i32 = args[0].i32 + args[1].i32;
    return VM_OK;
}
vm_register_function(&ctx, 0, my_add);
```

---

#### `OP_CALL_VOID` — Call Native Function (discard return value)

```
Encoding:  [op:u16][id:u32][argc:u8][r0:u8]...[rN:u8]
Size:      7 + argc bytes
```

Identical to `OP_CALL` except there is no `dst` register — the return value is
written only to `ctx->result`, not into any register. Use for side-effecting
natives (print, write, etc.).

```c
// Call print_i32(r0) — no result register
emit_call_void1(&bc, 0 /* id */, 0 /* r0 */);
```

> **Note:** Even for `CALL_VOID`, `ctx->result` is updated. The host can inspect it
> after execution.

---

### Return

---

#### `OP_RETURN_VOID` — Return Without Value

```
Encoding:  [op:u16]
Size:      2 bytes
```

Terminates execution cleanly. `vm_execute()` returns `VM_OK`. `ctx->result` is
not modified.

---

#### `OP_RETURN` — Return a Register Value

```
Encoding:  [op:u16][src:u8]
Size:      3 bytes
```

Copies all 64 bits of `regs[src]` into `ctx->result`, then terminates execution.
`vm_execute()` returns `VM_OK`.

```c
emit_return(&bc, 0);  // return r0

// After vm_execute():
int32_t result  = ctx.result.i32;
double  fresult = ctx.result.f64;
void*   presult = ctx.result.ptr;
```

---

## Opcode Encoding Summary

| Opcode | Hex | Encoding | Size |
|--------|-----|----------|------|
| `OP_NOP` | 0x0000 | — | 2 |
| `OP_MOVE` | 0x0001 | `[dst:u8][src:u16]` | 5 |
| `OP_CONST_I8` | 0x0002 | `[dst:u8][val:i8]` | 4 |
| `OP_CONST_I16` | 0x0003 | `[dst:u8][val:i16]` | 5 |
| `OP_CONST_I32` | 0x0004 | `[dst:u8][val:i32]` | 7 |
| `OP_CONST_I64` | 0x0005 | `[dst:u8][val:i64]` | 11 |
| `OP_CONST_F32` | 0x0006 | `[dst:u8][bits:u32]` | 7 |
| `OP_CONST_F64` | 0x0007 | `[dst:u8][bits:u64]` | 11 |
| `OP_ADD_I32` | 0x0008 | `[dst:u8][lhs:u8][rhs:u8]` | 5 |
| `OP_SUB_I32` | 0x0009 | `[dst:u8][lhs:u8][rhs:u8]` | 5 |
| `OP_MUL_I32` | 0x000A | `[dst:u8][lhs:u8][rhs:u8]` | 5 |
| `OP_DIV_I32` | 0x000B | `[dst:u8][lhs:u8][rhs:u8]` | 5 |
| `OP_REM_I32` | 0x000C | `[dst:u8][lhs:u8][rhs:u8]` | 5 |
| `OP_ADD_I64` | 0x000D | `[dst:u8][lhs:u8][rhs:u8]` | 5 |
| `OP_SUB_I64` | 0x000E | `[dst:u8][lhs:u8][rhs:u8]` | 5 |
| `OP_MUL_I64` | 0x000F | `[dst:u8][lhs:u8][rhs:u8]` | 5 |
| `OP_DIV_I64` | 0x0010 | `[dst:u8][lhs:u8][rhs:u8]` | 5 |
| `OP_REM_I64` | 0x0011 | `[dst:u8][lhs:u8][rhs:u8]` | 5 |
| `OP_DIV_U32` | 0x0012 | `[dst:u8][lhs:u8][rhs:u8]` | 5 |
| `OP_REM_U32` | 0x0013 | `[dst:u8][lhs:u8][rhs:u8]` | 5 |
| `OP_DIV_U64` | 0x0014 | `[dst:u8][lhs:u8][rhs:u8]` | 5 |
| `OP_REM_U64` | 0x0015 | `[dst:u8][lhs:u8][rhs:u8]` | 5 |
| `OP_ADD_F32` | 0x0016 | `[dst:u8][lhs:u8][rhs:u8]` | 5 |
| `OP_SUB_F32` | 0x0017 | `[dst:u8][lhs:u8][rhs:u8]` | 5 |
| `OP_MUL_F32` | 0x0018 | `[dst:u8][lhs:u8][rhs:u8]` | 5 |
| `OP_DIV_F32` | 0x0019 | `[dst:u8][lhs:u8][rhs:u8]` | 5 |
| `OP_ADD_F64` | 0x001A | `[dst:u8][lhs:u8][rhs:u8]` | 5 |
| `OP_SUB_F64` | 0x001B | `[dst:u8][lhs:u8][rhs:u8]` | 5 |
| `OP_MUL_F64` | 0x001C | `[dst:u8][lhs:u8][rhs:u8]` | 5 |
| `OP_DIV_F64` | 0x001D | `[dst:u8][lhs:u8][rhs:u8]` | 5 |
| `OP_NEG_I32` | 0x001E | `[dst:u8][src:u8]` | 4 |
| `OP_NEG_I64` | 0x001F | `[dst:u8][src:u8]` | 4 |
| `OP_NEG_F32` | 0x0020 | `[dst:u8][src:u8]` | 4 |
| `OP_NEG_F64` | 0x0021 | `[dst:u8][src:u8]` | 4 |
| `OP_NOT_I32` | 0x0022 | `[dst:u8][src:u8]` | 4 |
| `OP_NOT_I64` | 0x0023 | `[dst:u8][src:u8]` | 4 |
| `OP_AND_I32` | 0x0024 | `[dst:u8][lhs:u8][rhs:u8]` | 5 |
| `OP_OR_I32`  | 0x0025 | `[dst:u8][lhs:u8][rhs:u8]` | 5 |
| `OP_XOR_I32` | 0x0026 | `[dst:u8][lhs:u8][rhs:u8]` | 5 |
| `OP_AND_I64` | 0x0027 | `[dst:u8][lhs:u8][rhs:u8]` | 5 |
| `OP_OR_I64`  | 0x0028 | `[dst:u8][lhs:u8][rhs:u8]` | 5 |
| `OP_XOR_I64` | 0x0029 | `[dst:u8][lhs:u8][rhs:u8]` | 5 |
| `OP_SHL_I32` | 0x002A | `[dst:u8][val:u8][amt:u8]` | 5 |
| `OP_SHR_I32` | 0x002B | `[dst:u8][val:u8][amt:u8]` | 5 |
| `OP_USHR_I32` | 0x002C | `[dst:u8][val:u8][amt:u8]` | 5 |
| `OP_SHL_I64` | 0x002D | `[dst:u8][val:u8][amt:u8]` | 5 |
| `OP_SHR_I64` | 0x002E | `[dst:u8][val:u8][amt:u8]` | 5 |
| `OP_USHR_I64` | 0x002F | `[dst:u8][val:u8][amt:u8]` | 5 |
| `OP_CMP_I32` | 0x0030 | `[dst:u8][lhs:u8][rhs:u8]` | 5 |
| `OP_CMP_I64` | 0x0031 | `[dst:u8][lhs:u8][rhs:u8]` | 5 |
| `OP_CMP_F32` | 0x0032 | `[dst:u8][lhs:u8][rhs:u8]` | 5 |
| `OP_CMP_F64` | 0x0033 | `[dst:u8][lhs:u8][rhs:u8]` | 5 |
| `OP_CMP_F32_GT` | 0x0034 | `[dst:u8][lhs:u8][rhs:u8]` | 5 |
| `OP_I32_TO_I8`  | 0x0035 | `[dst:u8][src:u8]` | 4 |
| `OP_I32_TO_I16` | 0x0036 | `[dst:u8][src:u8]` | 4 |
| `OP_I32_TO_I64` | 0x0037 | `[dst:u8][src:u8]` | 4 |
| `OP_I32_TO_F32` | 0x0038 | `[dst:u8][src:u8]` | 4 |
| `OP_I32_TO_F64` | 0x0039 | `[dst:u8][src:u8]` | 4 |
| `OP_I64_TO_I32` | 0x003A | `[dst:u8][src:u8]` | 4 |
| `OP_I64_TO_F32` | 0x003B | `[dst:u8][src:u8]` | 4 |
| `OP_I64_TO_F64` | 0x003C | `[dst:u8][src:u8]` | 4 |
| `OP_F32_TO_I32` | 0x003D | `[dst:u8][src:u8]` | 4 |
| `OP_F32_TO_I64` | 0x003E | `[dst:u8][src:u8]` | 4 |
| `OP_F32_TO_F64` | 0x003F | `[dst:u8][src:u8]` | 4 |
| `OP_F64_TO_I32` | 0x0040 | `[dst:u8][src:u8]` | 4 |
| `OP_F64_TO_I64` | 0x0041 | `[dst:u8][src:u8]` | 4 |
| `OP_F64_TO_F32` | 0x0042 | `[dst:u8][src:u8]` | 4 |
| `OP_LOAD8`    | 0x0043 | `[dst:u8][addr:u8]` | 4 |
| `OP_LOAD8S`   | 0x0044 | `[dst:u8][addr:u8]` | 4 |
| `OP_LOAD16`   | 0x0045 | `[dst:u8][addr:u8]` | 4 |
| `OP_LOAD16S`  | 0x0046 | `[dst:u8][addr:u8]` | 4 |
| `OP_LOAD32`   | 0x0047 | `[dst:u8][addr:u8]` | 4 |
| `OP_LOAD32S`  | 0x0048 | `[dst:u8][addr:u8]` | 4 |
| `OP_LOAD64`   | 0x0049 | `[dst:u8][addr:u8]` | 4 |
| `OP_LOAD_PTR` | 0x004A | `[dst:u8][addr:u8]` | 4 |
| `OP_STORE8`   | 0x004B | `[addr:u8][src:u8]` | 4 |
| `OP_STORE16`  | 0x004C | `[addr:u8][src:u8]` | 4 |
| `OP_STORE32`  | 0x004D | `[addr:u8][src:u8]` | 4 |
| `OP_STORE64`  | 0x004E | `[addr:u8][src:u8]` | 4 |
| `OP_STORE_PTR` | 0x004F | `[addr:u8][src:u8]` | 4 |
| `OP_LEA`      | 0x0050 | `[dst:u8][base:u8][offset:i32]` | 8 |
| `OP_GOTO`     | 0x0051 | `[offset:i8]` | 3 |
| `OP_GOTO_16`  | 0x0052 | `[offset:i16]` | 4 |
| `OP_GOTO_32`  | 0x0053 | `[offset:i32]` | 6 |
| `OP_IF_EQ`    | 0x0054 | `[A:u8][B:u8][offset:i16]` | 6 |
| `OP_IF_NE`    | 0x0055 | `[A:u8][B:u8][offset:i16]` | 6 |
| `OP_IF_LT`    | 0x0056 | `[A:u8][B:u8][offset:i16]` | 6 |
| `OP_IF_GE`    | 0x0057 | `[A:u8][B:u8][offset:i16]` | 6 |
| `OP_IF_GT`    | 0x0058 | `[A:u8][B:u8][offset:i16]` | 6 |
| `OP_IF_LE`    | 0x0059 | `[A:u8][B:u8][offset:i16]` | 6 |
| `OP_IF_EQZ`   | 0x005A | `[A:u8][offset:i16]` | 5 |
| `OP_IF_NEZ`   | 0x005B | `[A:u8][offset:i16]` | 5 |
| `OP_IF_LTZ`   | 0x005C | `[A:u8][offset:i16]` | 5 |
| `OP_IF_GEZ`   | 0x005D | `[A:u8][offset:i16]` | 5 |
| `OP_IF_GTZ`   | 0x005E | `[A:u8][offset:i16]` | 5 |
| `OP_IF_LEZ`   | 0x005F | `[A:u8][offset:i16]` | 5 |
| `OP_CALL`      | 0x0060 | `[dst:u8][id:u32][argc:u8][r0..rN:u8]` | 8+argc |
| `OP_CALL_VOID` | 0x0061 | `[id:u32][argc:u8][r0..rN:u8]` | 7+argc |
| `OP_RETURN_VOID` | 0x0062 | — | 2 |
| `OP_RETURN`    | 0x0063 | `[src:u8]` | 3 |

---

## Public API

```c
/* Zero-initialise a VMContext. Must be called before any other vm_* call. */
void vm_init(VMContext* ctx);

/* Register a native function at id (0 <= id < 256).
   Overwrites any existing entry at that id.
   Returns VM_ERR_BAD_FUNCTION if id is out of range. */
VMError vm_register_function(VMContext* ctx, uint32_t id, VMNativeFn fn);

/* Execute bytecode.
   ctx           - initialised VMContext
   regs          - caller-allocated register file (memset to 0 recommended)
   reg_count     - number of valid registers in regs[]
   pc            - starting byte offset into bytecode (usually 0)
   bytecode      - read-only instruction buffer
   bytecode_size - buffer length in bytes

   Returns VM_OK on clean exit, or a non-zero VMError on failure.
   ctx->result holds the value from OP_RETURN (if reached). */
VMError vm_execute(VMContext*     ctx,
                   VMRegister*    regs,
                   uint32_t       reg_count,
                   uint32_t       pc,
                   const uint8_t* bytecode,
                   uint32_t       bytecode_size);
```

### Native function signature

```c
typedef VMError (*VMNativeFn)(
    VMContext*  ctx,        /* executing context                  */
    uint32_t    argc,       /* number of arguments                */
    VMRegister* args,       /* argument array [argc]              */
    VMRegister* out_result  /* write return value here (not NULL) */
);
```

Return `VM_OK` on success. Any non-zero `VMError` immediately halts `vm_execute()`
and is returned to the host.

### Compile flags

| Flag | Effect |
|------|--------|
| `-DVM_DEBUG` | Enables instruction tracing. Gated per-context via `ctx->debug` (set non-zero to enable). Output goes to `stderr`. |

---

## Assembler & Disassembler API

### Two-Pass Assembler (`vm_assembler.h`)

Converts CVM assembly text source (`.cvma`) into executable binary bytecode.

```c
/* Assemble text source code to binary bytecode result */
VMAssembleResult vm_assemble(const char* source_text);

/* Free heap-allocated bytecode buffer inside result */
void vm_assemble_free(VMAssembleResult* res);

/* Assemble directly into a caller-allocated buffer */
VMError vm_assemble_to_buffer(const char* source_text,
                              uint8_t* out_buf, size_t max_size,
                              size_t* out_size,
                              char* err_buf, size_t err_buf_size);
```

### Disassembler (`vm_disassembler.h`)

Decodes CVM binary bytecode into formatted textual assembly representation.

```c
/* Disassemble a single instruction at byte offset 'pc' */
int vm_disassemble_instruction(const uint8_t* code, size_t size, size_t pc,
                               char* out_buf, size_t buf_size);

/* Disassemble with custom formatting options */
int vm_disassemble_instruction_ext(const uint8_t* code, size_t size, size_t pc,
                                   char* out_buf, size_t buf_size,
                                   const VMDisasmOptions* opts);

/* Disassemble entire bytecode buffer into a newly allocated string (caller frees) */
char* vm_disassemble(const uint8_t* code, size_t size);

/* Disassemble and print entire bytecode to stream (e.g. stdout) */
void vm_disassemble_file(FILE* stream, const uint8_t* code, size_t size);
```

