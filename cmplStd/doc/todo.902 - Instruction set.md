## Instruction set
[TODO: documentation]

## Opcode matrix
| *      | 0        | 1        | 2         | 3         | 4         | 5          | 6         | 7         | 8         | 9          | A         | B         | C         | D         | E         | F            |
|:-------|:---------|:---------|:----------|:----------|:----------|:-----------|:----------|:----------|:----------|:-----------|:----------|:----------|:----------|:----------|:----------|:-------------|
| 0 sys  | nop      | nfc      | call      | ret       | jmp       | jnz        | jz        | task      | sync      | inc.sp     | load.sp   | not       | inc       | mad       | -         | -            |
| 1 stk  | dup.x32  | dup.x64  | dup.x128  | set.x32   | set.x64   | set.x128   | mov.x32   | mov.x64   | mov.x128  | load.z32   | load.z64  | load.z128 | load.c32  | load.c64  | copy.mem  | ~~load.ref~~ |
| 2 mem  | load.m32 | load.m64 | load.m128 | store.m32 | store.m64 | store.m128 | load.i8   | load.i16  | load.i32  | load.i64   | load.i128 | store.i8  | store.i16 | store.i32 | store.i64 | store.i128   |
| 3 u32  | cmt      | and      | or        | mul       | div       | mod        | xor       | -         | clt       | cgt        | shl       | shr       | sar       | -         | cvt.i64   | ext.bit      |
| 4 u64  | cmt      | and      | or        | mul       | div       | mod        | xor       | -         | clt       | cgt        | shl       | shr       | sar       | -         | -         | -            |
| 5 i32  | neg      | add      | sub       | mul       | div       | mod        | -         | ceq       | clt       | cgt        | cvt.bool  | cvt.i64   | cvt.f32   | cvt.f64   | -         | -            |
| 6 i64  | neg      | add      | sub       | mul       | div       | mod        | -         | ceq       | clt       | cgt        | cvt.i32   | cvt.bool  | cvt.f32   | cvt.f64   | -         | -            |
| 7 f32  | neg      | add      | sub       | mul       | div       | mod        | -         | ceq       | clt       | cgt        | cvt.i32   | cvt.i64   | cvt.bool  | cvt.f64   | -         | ~~load.f32~~ |
| 8 f64  | neg      | add      | sub       | mul       | div       | mod        | -         | ceq       | clt       | cgt        | cvt.i32   | cvt.i64   | cvt.f32   | cvt.bool  | -         | ~~load.f64~~ |
| 9 v4f  | neg      | add      | sub       | mul       | div       | -          | -         | ceq       | min       | max        | dp3       | dp4       | dph       | -         | -         | -            |
| A bit  | -        | -        | -         | -         | -         | -          | -         | -         | -         | -          | -         | -         | -         | -         | -         | -            |
| B int  | -        | -        | -         | -         | -         | -          | -         | -         | -         | -          | -         | -         | -         | -         | -         | -            |
| C flt  | -        | -        | -         | -         | -         | -          | -         | -         | -         | -          | -         | -         | -         | -         | -         | -            |
| D p128 | -        | -        | -         | -         | -         | -          | -         | -         | -         | -          | -         | -         | -         | -         | -         | -            |
| E p256 | -        | -        | -         | -         | -         | -          | -         | -         | -         | -          | -         | -         | -         | -         | -         | -            |
| F ext  | -        | -        | -         | -         | -         | -          | -         | -         | -         | -          | -         | -         | -         | -         | -         | -            |

