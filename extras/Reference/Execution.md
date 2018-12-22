# Virtual machine
[TODO: documentation]

## Stack
[TODO: documentation]

## Registers
[TODO: documentation]  
The virtual machine registers:
- ip: Instruction pointer
- sp: Stack pointer
- tp: Trace pointer (used only when debugging/profiling)
- bp: Base pointer constant (to detect stack overflow)
- ss: Stack size constant

## Memory layout
The memory is organized in 4 segments:

- meta[r--]:
	- identifiers and string constants (are emitted in the compilation phase).
	- typename and variable metadata (are emitted in the compilation phase).
	- enumeration values and initializer literals.

- code[r-x]:
	- functions implementations.

- data[rw-]:
	- static variables.

- heap[rw-]:
	- heap memory
	- stack(s).

In the compilation phase the lower part of the memory is filled
with persistent data which remains available for code execution,
while the upper part is filled with token structures and other
temporary data, that will be overwritten on code execution.

# Instruction set
[TODO: documentation]

## Instruction `nop`
Perform no operation.

### Description
Instruction code: `0x00`  
Instruction length: 1 byte  

### Stack change
Requires 0 operands: […  
Returns 0 values: […  

## Instruction `nfc`
Perform native ("C") function call.

### Description
Instruction code: `0x01`  
Instruction length: 4 bytes  

### Stack change
Requires N operands: […  
Returns M values: […  
N and M are equivalent with the function's argument and return value.

## Instruction `call`
Perform [TODO]

### Description
Instruction code: `0x02`  
Instruction length: 1 byte  

### Stack change
Requires 1 operand: […, a  
Returns 1 value: […, a, [TODO]  

## Instruction `ret`
Perform [TODO]

### Description
Instruction code: `0x03`  
Instruction length: 1 byte  

### Stack change
Requires 1 operand: […, a  
Returns 0 value: […, [TODO]  

## Instruction `jmp`
Perform [TODO]

### Description
Instruction code: `0x04`  
Instruction length: 4 bytes  

### Stack change
Requires 0 operands: […  
Returns 0 values: […, [TODO]  

## Instruction `jnz`
Perform [TODO]

### Description
Instruction code: `0x05`  
Instruction length: 4 bytes  

### Stack change
Requires 1 operand: […, a  
Returns 0 value: […, [TODO]  

## Instruction `jz`
Perform [TODO]

### Description
Instruction code: `0x06`  
Instruction length: 4 bytes  

### Stack change
Requires 1 operand: […, a  
Returns 0 value: […, [TODO]  

## Instruction `task`
Perform [TODO]

### Description
Instruction code: `0x07`  
Instruction length: 4 bytes  

### Stack change
Requires 0 operands: […  
Returns 0 values: […, [TODO]  

## Instruction `sync`
Perform [TODO]

### Description
Instruction code: `0x08`  
Instruction length: 2 bytes  

### Stack change
Requires 0 operands: […  
Returns 0 values: […, [TODO]  

## Instruction `not.b32`
Perform [TODO]

### Description
Instruction code: `0x0a`  
Instruction length: 1 byte  

### Stack change
Requires 1 operand: […, a  
Returns 1 value: […, !a  

## Instruction `inc.i32`
Perform [TODO]

### Description
Instruction code: `0x0b`  
Instruction length: 4 bytes  

### Stack change
Requires 1 operand: […, a  
Returns 1 value: […, a, [TODO]  

## Instruction `mad.u32`
Perform [TODO]

### Description
Instruction code: `0x0c`  
Instruction length: 4 bytes  

### Stack change
Requires 2 operands: […, a, b  
Returns 1 values: […, a, [TODO]  

## Instruction `inc.sp`
Perform [TODO]

### Description
Instruction code: `0x10`  
Instruction length: 4 bytes  

### Stack change
Requires 0 operands: […  
Returns 0 values: […, [TODO]  

## Instruction `load.sp`
Perform [TODO]

### Description
Instruction code: `0x11`  
Instruction length: 4 bytes  

### Stack change
Requires 0 operands: […  
Returns 1 values: […, a, [TODO]  

## Instruction `dup.x1`
Perform [TODO]

### Description
Instruction code: `0x12`  
Instruction length: 2 bytes  

### Stack change
Requires 0 operands: […  
Returns 1 values: […, a, [TODO]  

## Instruction `dup.x2`
Perform [TODO]

### Description
Instruction code: `0x13`  
Instruction length: 2 bytes  

### Stack change
Requires 0 operands: […  
Returns 2 values: […, a, b, [TODO]  

## Instruction `dup.x4`
Perform [TODO]

### Description
Instruction code: `0x14`  
Instruction length: 2 bytes  

### Stack change
Requires 0 operands: […  
Returns 4 values: […, a, b, c, d, [TODO]  

## Instruction `set.x1`
Perform [TODO]

### Description
Instruction code: `0x15`  
Instruction length: 2 bytes  

### Stack change
Requires 1 operand: […, a  
Returns 0 value: […, [TODO]  

## Instruction `set.x2`
Perform [TODO]

### Description
Instruction code: `0x16`  
Instruction length: 2 bytes  

### Stack change
Requires 2 operands: […, a, b  
Returns 0 values: […, [TODO]  

## Instruction `set.x4`
Perform [TODO]

### Description
Instruction code: `0x17`  
Instruction length: 2 bytes  

### Stack change
Requires 4 operands: […, a, b, c, d  
Returns 0 values: […, [TODO]  

## Instruction `load.z32`
Perform [TODO]

### Description
Instruction code: `0x18`  
Instruction length: 1 byte  

### Stack change
Requires 0 operands: […  
Returns 1 values: […, a, [TODO]  

## Instruction `load.z64`
Perform [TODO]

### Description
Instruction code: `0x19`  
Instruction length: 1 byte  

### Stack change
Requires 0 operands: […  
Returns 2 values: […, a, b, [TODO]  

## Instruction `load.z128`
Perform [TODO]

### Description
Instruction code: `0x1a`  
Instruction length: 1 byte  

### Stack change
Requires 0 operands: […  
Returns 4 values: […, a, b, c, d, [TODO]  

## Instruction `load.c32`
Perform [TODO]

### Description
Instruction code: `0x1b`  
Instruction length: 5 bytes  

### Stack change
Requires 0 operands: […  
Returns 1 values: […, a, [TODO]  

## Instruction `load.c64`
Perform [TODO]

### Description
Instruction code: `0x1c`  
Instruction length: 9 bytes  

### Stack change
Requires 0 operands: […  
Returns 2 values: […, a, b, [TODO]  

## Instruction `load.f32`
Perform [TODO]

### Description
Instruction code: `0x1d`  
Instruction length: 5 bytes  

### Stack change
Requires 0 operands: […  
Returns 1 values: […, a, [TODO]  

## Instruction `load.f64`
Perform [TODO]

### Description
Instruction code: `0x1e`  
Instruction length: 9 bytes  

### Stack change
Requires 0 operands: […  
Returns 2 values: […, a, b, [TODO]  

## Instruction `load.ref`
Perform [TODO]

### Description
Instruction code: `0x1f`  
Instruction length: 5 bytes  

### Stack change
Requires 0 operands: […  
Returns 1 values: […, a, [TODO]  

## Instruction `load.i8`
Perform [TODO]

### Description
Instruction code: `0x20`  
Instruction length: 1 byte  

### Stack change
Requires 1 operand: […, a  
Returns 1 value: […, a, [TODO]  

## Instruction `load.i16`
Perform [TODO]

### Description
Instruction code: `0x21`  
Instruction length: 1 byte  

### Stack change
Requires 1 operand: […, a  
Returns 1 value: […, a, [TODO]  

## Instruction `load.i32`
Perform [TODO]

### Description
Instruction code: `0x22`  
Instruction length: 1 byte  

### Stack change
Requires 1 operand: […, a  
Returns 1 value: […, a, [TODO]  

## Instruction `load.i64`
Perform [TODO]

### Description
Instruction code: `0x23`  
Instruction length: 1 byte  

### Stack change
Requires 1 operand: […, a  
Returns 2 value: […, a, b, [TODO]  

## Instruction `load.i128`
Perform [TODO]

### Description
Instruction code: `0x24`  
Instruction length: 1 byte  

### Stack change
Requires 1 operand: […, a  
Returns 4 value: […, a, b, c, d, [TODO]  

## Instruction `store.i8`
Perform [TODO]

### Description
Instruction code: `0x25`  
Instruction length: 1 byte  

### Stack change
Requires 2 operands: […, a, b  
Returns 0 values: […, [TODO]  

## Instruction `store.i16`
Perform [TODO]

### Description
Instruction code: `0x26`  
Instruction length: 1 byte  

### Stack change
Requires 2 operands: […, a, b  
Returns 0 values: […, [TODO]  

## Instruction `store.i32`
Perform [TODO]

### Description
Instruction code: `0x27`  
Instruction length: 1 byte  

### Stack change
Requires 2 operands: […, a, b  
Returns 0 values: […, [TODO]  

## Instruction `store.i64`
Perform [TODO]

### Description
Instruction code: `0x28`  
Instruction length: 1 byte  

### Stack change
Requires 3 operands: […, a, b, c  
Returns 0 values: […, [TODO]  

## Instruction `store.i128`
Perform [TODO]

### Description
Instruction code: `0x29`  
Instruction length: 1 byte  

### Stack change
Requires 5 operands: […, a, b, c, d, e  
Returns 0 values: […, [TODO]  

## Instruction `load.m32`
Perform [TODO]

### Description
Instruction code: `0x2a`  
Instruction length: 4 bytes  

### Stack change
Requires 0 operands: […  
Returns 1 values: […, a, [TODO]  

## Instruction `load.m64`
Perform [TODO]

### Description
Instruction code: `0x2b`  
Instruction length: 4 bytes  

### Stack change
Requires 0 operands: […  
Returns 2 values: […, a, b, [TODO]  

## Instruction `store.m32`
Perform [TODO]

### Description
Instruction code: `0x2d`  
Instruction length: 4 bytes  

### Stack change
Requires 1 operand: […, a  
Returns 0 value: […, [TODO]  

## Instruction `store.m64`
Perform [TODO]

### Description
Instruction code: `0x2e`  
Instruction length: 4 bytes  

### Stack change
Requires 2 operands: […, a, b  
Returns 0 values: […, [TODO]  

## Instruction `copy.mem`
Perform [TODO]

### Description
Instruction code: `0x2f`  
Instruction length: 4 bytes  

### Stack change
Requires 2 operands: […, a, b  
Returns 0 values: […, [TODO]  

## Instruction `cmt.b32`
Perform [TODO]

### Description
Instruction code: `0x30`  
Instruction length: 1 byte  

### Stack change
Requires 1 operand: […, a  
Returns 1 value: […, a, [TODO]  

## Instruction `and.b32`
Perform [TODO]

### Description
Instruction code: `0x31`  
Instruction length: 1 byte  

### Stack change
Requires 2 operands: […, a, b  
Returns 1 values: […, a, [TODO]  

## Instruction `or.b32`
Perform [TODO]

### Description
Instruction code: `0x32`  
Instruction length: 1 byte  

### Stack change
Requires 2 operands: […, a, b  
Returns 1 values: […, a, [TODO]  

## Instruction `mul.u32`
Perform [TODO]

### Description
Instruction code: `0x33`  
Instruction length: 1 byte  

### Stack change
Requires 2 operands: […, a, b  
Returns 1 values: […, a, [TODO]  

## Instruction `div.u32`
Perform [TODO]

### Description
Instruction code: `0x34`  
Instruction length: 1 byte  

### Stack change
Requires 2 operands: […, a, b  
Returns 1 values: […, a, [TODO]  

## Instruction `mod.u32`
Perform [TODO]

### Description
Instruction code: `0x35`  
Instruction length: 1 byte  

### Stack change
Requires 2 operands: […, a, b  
Returns 1 values: […, a, [TODO]  

## Instruction `xor.b32`
Perform [TODO]

### Description
Instruction code: `0x36`  
Instruction length: 1 byte  

### Stack change
Requires 2 operands: […, a, b  
Returns 1 values: […, a, [TODO]  

## Instruction `clt.u32`
Perform [TODO]

### Description
Instruction code: `0x38`  
Instruction length: 1 byte  

### Stack change
Requires 2 operands: […, a, b  
Returns 1 values: […, a, [TODO]  

## Instruction `cgt.u32`
Perform [TODO]

### Description
Instruction code: `0x39`  
Instruction length: 1 byte  

### Stack change
Requires 2 operands: […, a, b  
Returns 1 values: […, a, [TODO]  

## Instruction `shl.b32`
Perform [TODO]

### Description
Instruction code: `0x3a`  
Instruction length: 1 byte  

### Stack change
Requires 2 operands: […, a, b  
Returns 1 values: […, a, [TODO]  

## Instruction `shr.b32`
Perform [TODO]

### Description
Instruction code: `0x3b`  
Instruction length: 1 byte  

### Stack change
Requires 2 operands: […, a, b  
Returns 1 values: […, a, [TODO]  

## Instruction `sar.b32`
Perform [TODO]

### Description
Instruction code: `0x3c`  
Instruction length: 1 byte  

### Stack change
Requires 2 operands: […, a, b  
Returns 1 values: […, a, [TODO]  

## Instruction `u32.2i64`
Perform [TODO]

### Description
Instruction code: `0x3e`  
Instruction length: 1 byte  

### Stack change
Requires 1 operand: […, a  
Returns 2 value: […, a, b, [TODO]  

## Instruction `b32.`
Perform [TODO]

### Description
Instruction code: `0x3f`  
Instruction length: 2 bytes  

### Stack change
Requires 1 operand: […, a  
Returns 1 value: […, a, [TODO]  

## Instruction `cmt.b64`
Perform [TODO]

### Description
Instruction code: `0x40`  
Instruction length: 1 byte  

### Stack change
Requires 2 operands: […, a, b  
Returns 2 values: […, a, b, [TODO]  

## Instruction `and.b64`
Perform [TODO]

### Description
Instruction code: `0x41`  
Instruction length: 1 byte  

### Stack change
Requires 4 operands: […, a, b, c, d  
Returns 2 values: […, a, b, [TODO]  

## Instruction `or.b64`
Perform [TODO]

### Description
Instruction code: `0x42`  
Instruction length: 1 byte  

### Stack change
Requires 4 operands: […, a, b, c, d  
Returns 2 values: […, a, b, [TODO]  

## Instruction `mul.u64`
Perform [TODO]

### Description
Instruction code: `0x43`  
Instruction length: 1 byte  

### Stack change
Requires 4 operands: […, a, b, c, d  
Returns 2 values: […, a, b, [TODO]  

## Instruction `div.u64`
Perform [TODO]

### Description
Instruction code: `0x44`  
Instruction length: 1 byte  

### Stack change
Requires 4 operands: […, a, b, c, d  
Returns 2 values: […, a, b, [TODO]  

## Instruction `mod.u64`
Perform [TODO]

### Description
Instruction code: `0x45`  
Instruction length: 1 byte  

### Stack change
Requires 4 operands: […, a, b, c, d  
Returns 2 values: […, a, b, [TODO]  

## Instruction `xor.b64`
Perform [TODO]

### Description
Instruction code: `0x46`  
Instruction length: 1 byte  

### Stack change
Requires 4 operands: […, a, b, c, d  
Returns 2 values: […, a, b, [TODO]  

## Instruction `clt.u64`
Perform [TODO]

### Description
Instruction code: `0x48`  
Instruction length: 1 byte  

### Stack change
Requires 4 operands: […, a, b, c, d  
Returns 1 values: […, a, [TODO]  

## Instruction `cgt.u64`
Perform [TODO]

### Description
Instruction code: `0x49`  
Instruction length: 1 byte  

### Stack change
Requires 4 operands: […, a, b, c, d  
Returns 1 values: […, a, [TODO]  

## Instruction `shl.b64`
Perform [TODO]

### Description
Instruction code: `0x4a`  
Instruction length: 1 byte  

### Stack change
Requires 3 operands: […, a, b, c  
Returns 2 values: […, a, b, [TODO]  

## Instruction `shr.b64`
Perform [TODO]

### Description
Instruction code: `0x4b`  
Instruction length: 1 byte  

### Stack change
Requires 3 operands: […, a, b, c  
Returns 2 values: […, a, b, [TODO]  

## Instruction `sar.b64`
Perform [TODO]

### Description
Instruction code: `0x4c`  
Instruction length: 1 byte  

### Stack change
Requires 3 operands: […, a, b, c  
Returns 2 values: […, a, b, [TODO]  

## Instruction `neg.i32`
Perform [TODO]

### Description
Instruction code: `0x50`  
Instruction length: 1 byte  

### Stack change
Requires 1 operand: […, a  
Returns 1 value: […, a, [TODO]  

## Instruction `add.i32`
Perform [TODO]

### Description
Instruction code: `0x51`  
Instruction length: 1 byte  

### Stack change
Requires 2 operands: […, a, b  
Returns 1 values: […, a, [TODO]  

## Instruction `sub.i32`
Perform [TODO]

### Description
Instruction code: `0x52`  
Instruction length: 1 byte  

### Stack change
Requires 2 operands: […, a, b  
Returns 1 values: […, a, [TODO]  

## Instruction `mul.i32`
Perform [TODO]

### Description
Instruction code: `0x53`  
Instruction length: 1 byte  

### Stack change
Requires 2 operands: […, a, b  
Returns 1 values: […, a, [TODO]  

## Instruction `div.i32`
Perform [TODO]

### Description
Instruction code: `0x54`  
Instruction length: 1 byte  

### Stack change
Requires 2 operands: […, a, b  
Returns 1 values: […, a, [TODO]  

## Instruction `mod.i32`
Perform [TODO]

### Description
Instruction code: `0x55`  
Instruction length: 1 byte  

### Stack change
Requires 2 operands: […, a, b  
Returns 1 values: […, a, [TODO]  

## Instruction `ceq.i32`
Perform [TODO]

### Description
Instruction code: `0x57`  
Instruction length: 1 byte  

### Stack change
Requires 2 operands: […, a, b  
Returns 1 values: […, a, [TODO]  

## Instruction `clt.i32`
Perform [TODO]

### Description
Instruction code: `0x58`  
Instruction length: 1 byte  

### Stack change
Requires 2 operands: […, a, b  
Returns 1 values: […, a, [TODO]  

## Instruction `cgt.i32`
Perform [TODO]

### Description
Instruction code: `0x59`  
Instruction length: 1 byte  

### Stack change
Requires 2 operands: […, a, b  
Returns 1 values: […, a, [TODO]  

## Instruction `i32.2bool`
Perform [TODO]

### Description
Instruction code: `0x5a`  
Instruction length: 1 byte  

### Stack change
Requires 1 operand: […, a  
Returns 1 value: […, a, [TODO]  

## Instruction `i32.2f32`
Perform [TODO]

### Description
Instruction code: `0x5b`  
Instruction length: 1 byte  

### Stack change
Requires 1 operand: […, a  
Returns 1 value: […, a, [TODO]  

## Instruction `i32.2i64`
Perform [TODO]

### Description
Instruction code: `0x5c`  
Instruction length: 1 byte  

### Stack change
Requires 1 operand: […, a  
Returns 2 value: […, a, b, [TODO]  

## Instruction `i32.2f64`
Perform [TODO]

### Description
Instruction code: `0x5d`  
Instruction length: 1 byte  

### Stack change
Requires 1 operand: […, a  
Returns 2 value: […, a, b, [TODO]  

## Instruction `neg.i64`
Perform [TODO]

### Description
Instruction code: `0x60`  
Instruction length: 1 byte  

### Stack change
Requires 2 operands: […, a, b  
Returns 2 values: […, a, b, [TODO]  

## Instruction `add.i64`
Perform [TODO]

### Description
Instruction code: `0x61`  
Instruction length: 1 byte  

### Stack change
Requires 4 operands: […, a, b, c, d  
Returns 2 values: […, a, b, [TODO]  

## Instruction `sub.i64`
Perform [TODO]

### Description
Instruction code: `0x62`  
Instruction length: 1 byte  

### Stack change
Requires 4 operands: […, a, b, c, d  
Returns 2 values: […, a, b, [TODO]  

## Instruction `mul.i64`
Perform [TODO]

### Description
Instruction code: `0x63`  
Instruction length: 1 byte  

### Stack change
Requires 4 operands: […, a, b, c, d  
Returns 2 values: […, a, b, [TODO]  

## Instruction `div.i64`
Perform [TODO]

### Description
Instruction code: `0x64`  
Instruction length: 1 byte  

### Stack change
Requires 4 operands: […, a, b, c, d  
Returns 2 values: […, a, b, [TODO]  

## Instruction `mod.i64`
Perform [TODO]

### Description
Instruction code: `0x65`  
Instruction length: 1 byte  

### Stack change
Requires 4 operands: […, a, b, c, d  
Returns 2 values: […, a, b, [TODO]  

## Instruction `ceq.i64`
Perform [TODO]

### Description
Instruction code: `0x67`  
Instruction length: 1 byte  

### Stack change
Requires 4 operands: […, a, b, c, d  
Returns 1 values: […, a, [TODO]  

## Instruction `clt.i64`
Perform [TODO]

### Description
Instruction code: `0x68`  
Instruction length: 1 byte  

### Stack change
Requires 4 operands: […, a, b, c, d  
Returns 1 values: […, a, [TODO]  

## Instruction `cgt.i64`
Perform [TODO]

### Description
Instruction code: `0x69`  
Instruction length: 1 byte  

### Stack change
Requires 4 operands: […, a, b, c, d  
Returns 1 values: […, a, [TODO]  

## Instruction `i64.2i32`
Perform [TODO]

### Description
Instruction code: `0x6a`  
Instruction length: 1 byte  

### Stack change
Requires 2 operands: […, a, b  
Returns 1 values: […, a, [TODO]  

## Instruction `i64.2f32`
Perform [TODO]

### Description
Instruction code: `0x6b`  
Instruction length: 1 byte  

### Stack change
Requires 2 operands: […, a, b  
Returns 1 values: […, a, [TODO]  

## Instruction `i64.2bool`
Perform [TODO]

### Description
Instruction code: `0x6c`  
Instruction length: 1 byte  

### Stack change
Requires 2 operands: […, a, b  
Returns 1 values: […, a, [TODO]  

## Instruction `i64.2f64`
Perform [TODO]

### Description
Instruction code: `0x6d`  
Instruction length: 1 byte  

### Stack change
Requires 2 operands: […, a, b  
Returns 2 values: […, a, b, [TODO]  

## Instruction `neg.f32`
Perform [TODO]

### Description
Instruction code: `0x70`  
Instruction length: 1 byte  

### Stack change
Requires 1 operand: […, a  
Returns 1 value: […, a, [TODO]  

## Instruction `add.f32`
Perform [TODO]

### Description
Instruction code: `0x71`  
Instruction length: 1 byte  

### Stack change
Requires 2 operands: […, a, b  
Returns 1 values: […, a, [TODO]  

## Instruction `sub.f32`
Perform [TODO]

### Description
Instruction code: `0x72`  
Instruction length: 1 byte  

### Stack change
Requires 2 operands: […, a, b  
Returns 1 values: […, a, [TODO]  

## Instruction `mul.f32`
Perform [TODO]

### Description
Instruction code: `0x73`  
Instruction length: 1 byte  

### Stack change
Requires 2 operands: […, a, b  
Returns 1 values: […, a, [TODO]  

## Instruction `div.f32`
Perform [TODO]

### Description
Instruction code: `0x74`  
Instruction length: 1 byte  

### Stack change
Requires 2 operands: […, a, b  
Returns 1 values: […, a, [TODO]  

## Instruction `mod.f32`
Perform [TODO]

### Description
Instruction code: `0x75`  
Instruction length: 1 byte  

### Stack change
Requires 2 operands: […, a, b  
Returns 1 values: […, a, [TODO]  

## Instruction `ceq.f32`
Perform [TODO]

### Description
Instruction code: `0x77`  
Instruction length: 1 byte  

### Stack change
Requires 2 operands: […, a, b  
Returns 1 values: […, a, [TODO]  

## Instruction `clt.f32`
Perform [TODO]

### Description
Instruction code: `0x78`  
Instruction length: 1 byte  

### Stack change
Requires 2 operands: […, a, b  
Returns 1 values: […, a, [TODO]  

## Instruction `cgt.f32`
Perform [TODO]

### Description
Instruction code: `0x79`  
Instruction length: 1 byte  

### Stack change
Requires 2 operands: […, a, b  
Returns 1 values: […, a, [TODO]  

## Instruction `f32.2i32`
Perform [TODO]

### Description
Instruction code: `0x7a`  
Instruction length: 1 byte  

### Stack change
Requires 1 operand: […, a  
Returns 1 value: […, a, [TODO]  

## Instruction `f32.2bool`
Perform [TODO]

### Description
Instruction code: `0x7b`  
Instruction length: 1 byte  

### Stack change
Requires 1 operand: […, a  
Returns 1 value: […, a, [TODO]  

## Instruction `f32.2i64`
Perform [TODO]

### Description
Instruction code: `0x7c`  
Instruction length: 1 byte  

### Stack change
Requires 1 operand: […, a  
Returns 2 value: […, a, b, [TODO]  

## Instruction `f32.2f64`
Perform [TODO]

### Description
Instruction code: `0x7d`  
Instruction length: 1 byte  

### Stack change
Requires 1 operand: […, a  
Returns 2 value: […, a, b, [TODO]  

## Instruction `neg.f64`
Perform [TODO]

### Description
Instruction code: `0x80`  
Instruction length: 1 byte  

### Stack change
Requires 2 operands: […, a, b  
Returns 2 values: […, a, b, [TODO]  

## Instruction `add.f64`
Perform [TODO]

### Description
Instruction code: `0x81`  
Instruction length: 1 byte  

### Stack change
Requires 4 operands: […, a, b, c, d  
Returns 2 values: […, a, b, [TODO]  

## Instruction `sub.f64`
Perform [TODO]

### Description
Instruction code: `0x82`  
Instruction length: 1 byte  

### Stack change
Requires 4 operands: […, a, b, c, d  
Returns 2 values: […, a, b, [TODO]  

## Instruction `mul.f64`
Perform [TODO]

### Description
Instruction code: `0x83`  
Instruction length: 1 byte  

### Stack change
Requires 4 operands: […, a, b, c, d  
Returns 2 values: […, a, b, [TODO]  

## Instruction `div.f64`
Perform [TODO]

### Description
Instruction code: `0x84`  
Instruction length: 1 byte  

### Stack change
Requires 4 operands: […, a, b, c, d  
Returns 2 values: […, a, b, [TODO]  

## Instruction `mod.f64`
Perform [TODO]

### Description
Instruction code: `0x85`  
Instruction length: 1 byte  

### Stack change
Requires 4 operands: […, a, b, c, d  
Returns 2 values: […, a, b, [TODO]  

## Instruction `ceq.f64`
Perform [TODO]

### Description
Instruction code: `0x87`  
Instruction length: 1 byte  

### Stack change
Requires 4 operands: […, a, b, c, d  
Returns 1 values: […, a, [TODO]  

## Instruction `clt.f64`
Perform [TODO]

### Description
Instruction code: `0x88`  
Instruction length: 1 byte  

### Stack change
Requires 4 operands: […, a, b, c, d  
Returns 1 values: […, a, [TODO]  

## Instruction `cgt.f64`
Perform [TODO]

### Description
Instruction code: `0x89`  
Instruction length: 1 byte  

### Stack change
Requires 4 operands: […, a, b, c, d  
Returns 1 values: […, a, [TODO]  

## Instruction `f64.2i32`
Perform [TODO]

### Description
Instruction code: `0x8a`  
Instruction length: 1 byte  

### Stack change
Requires 2 operands: […, a, b  
Returns 1 values: […, a, [TODO]  

## Instruction `f64.2f32`
Perform [TODO]

### Description
Instruction code: `0x8b`  
Instruction length: 1 byte  

### Stack change
Requires 2 operands: […, a, b  
Returns 1 values: […, a, [TODO]  

## Instruction `f64.2i64`
Perform [TODO]

### Description
Instruction code: `0x8c`  
Instruction length: 1 byte  

### Stack change
Requires 2 operands: […, a, b  
Returns 2 values: […, a, b, [TODO]  

## Instruction `f64.2bool`
Perform [TODO]

### Description
Instruction code: `0x8d`  
Instruction length: 1 byte  

### Stack change
Requires 2 operands: […, a, b  
Returns 1 values: […, a, [TODO]  

## Instruction `neg.v4f`
Perform [TODO]

### Description
Instruction code: `0x90`  
Instruction length: 1 byte  

### Stack change
Requires 4 operands: […, a, b, c, d  
Returns 4 values: […, a, b, c, d, [TODO]  

## Instruction `add.v4f`
Perform [TODO]

### Description
Instruction code: `0x91`  
Instruction length: 1 byte  

### Stack change
Requires 8 operands: […, a, b, c, d, e, f, g, h  
Returns 4 values: […, a, b, c, d, [TODO]  

## Instruction `sub.v4f`
Perform [TODO]

### Description
Instruction code: `0x92`  
Instruction length: 1 byte  

### Stack change
Requires 8 operands: […, a, b, c, d, e, f, g, h  
Returns 4 values: […, a, b, c, d, [TODO]  

## Instruction `mul.v4f`
Perform [TODO]

### Description
Instruction code: `0x93`  
Instruction length: 1 byte  

### Stack change
Requires 8 operands: […, a, b, c, d, e, f, g, h  
Returns 4 values: […, a, b, c, d, [TODO]  

## Instruction `div.v4f`
Perform [TODO]

### Description
Instruction code: `0x94`  
Instruction length: 1 byte  

### Stack change
Requires 8 operands: […, a, b, c, d, e, f, g, h  
Returns 4 values: […, a, b, c, d, [TODO]  

## Instruction `ceq.v4f`
Perform [TODO]

### Description
Instruction code: `0x97`  
Instruction length: 1 byte  

### Stack change
Requires 8 operands: […, a, b, c, d, e, f, g, h  
Returns 1 values: […, a, [TODO]  

## Instruction `min.v4f`
Perform [TODO]

### Description
Instruction code: `0x98`  
Instruction length: 1 byte  

### Stack change
Requires 8 operands: […, a, b, c, d, e, f, g, h  
Returns 4 values: […, a, b, c, d, [TODO]  

## Instruction `max.v4f`
Perform [TODO]

### Description
Instruction code: `0x99`  
Instruction length: 1 byte  

### Stack change
Requires 8 operands: […, a, b, c, d, e, f, g, h  
Returns 4 values: […, a, b, c, d, [TODO]  

## Instruction `dp3.v4f`
Perform [TODO]

### Description
Instruction code: `0x9a`  
Instruction length: 1 byte  

### Stack change
Requires 8 operands: […, a, b, c, d, e, f, g, h  
Returns 1 values: […, a, [TODO]  

## Instruction `dp4.v4f`
Perform [TODO]

### Description
Instruction code: `0x9b`  
Instruction length: 1 byte  

### Stack change
Requires 8 operands: […, a, b, c, d, e, f, g, h  
Returns 1 values: […, a, [TODO]  

## Instruction `dph.v4f`
Perform [TODO]

### Description
Instruction code: `0x9c`  
Instruction length: 1 byte  

### Stack change
Requires 8 operands: […, a, b, c, d, e, f, g, h  
Returns 1 values: […, a, [TODO]  

## Instruction `neg.v2d`
Perform [TODO]

### Description
Instruction code: `0xa0`  
Instruction length: 1 byte  

### Stack change
Requires 4 operands: […, a, b, c, d  
Returns 4 values: […, a, b, c, d, [TODO]  

## Instruction `add.v2d`
Perform [TODO]

### Description
Instruction code: `0xa1`  
Instruction length: 1 byte  

### Stack change
Requires 8 operands: […, a, b, c, d, e, f, g, h  
Returns 4 values: […, a, b, c, d, [TODO]  

## Instruction `sub.v2d`
Perform [TODO]

### Description
Instruction code: `0xa2`  
Instruction length: 1 byte  

### Stack change
Requires 8 operands: […, a, b, c, d, e, f, g, h  
Returns 4 values: […, a, b, c, d, [TODO]  

## Instruction `mul.v2d`
Perform [TODO]

### Description
Instruction code: `0xa3`  
Instruction length: 1 byte  

### Stack change
Requires 8 operands: […, a, b, c, d, e, f, g, h  
Returns 4 values: […, a, b, c, d, [TODO]  

## Instruction `div.v2d`
Perform [TODO]

### Description
Instruction code: `0xa4`  
Instruction length: 1 byte  

### Stack change
Requires 8 operands: […, a, b, c, d, e, f, g, h  
Returns 4 values: […, a, b, c, d, [TODO]  

## Instruction `ceq.v2d`
Perform [TODO]

### Description
Instruction code: `0xa7`  
Instruction length: 1 byte  

### Stack change
Requires 8 operands: […, a, b, c, d, e, f, g, h  
Returns 1 values: […, a, [TODO]  

## Instruction `min.v2d`
Perform [TODO]

### Description
Instruction code: `0xa8`  
Instruction length: 1 byte  

### Stack change
Requires 8 operands: […, a, b, c, d, e, f, g, h  
Returns 4 values: […, a, b, c, d, [TODO]  

## Instruction `max.v2d`
Perform [TODO]

### Description
Instruction code: `0xa9`  
Instruction length: 1 byte  

### Stack change
Requires 8 operands: […, a, b, c, d, e, f, g, h  
Returns 4 values: […, a, b, c, d, [TODO]  

## Instruction `swz.p4x`
Perform [TODO]

### Description
Instruction code: `0xaa`  
Instruction length: 2 bytes  

### Stack change
Requires 4 operands: […, a, b, c, d  
Returns 4 values: […, a, b, c, d, [TODO]  
