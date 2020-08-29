## Builtin functions
[TODO: documentation]

### The `emit` intrinsic function.
Emit is a low level builtin intrinsic for low level operations like emitting virtual machine instructions.

- arguments can be values and instructions.
- the result is not typed, it can be assigned or casted to any type,  
but the size of the type must match what is left on the stack.

**Example:**
```
struct Complex {
	float64 re;
	float64 im;
}

inline Complex(float64 re, float64 im) = Complex(emit(float64(im), float64(re)));
//inline -(Complex a, Complex b) = Complex(emit(Complex(a), Complex(b), sub.p2d));

Complex a = Complex(1., 2.);
Complex b = Complex(3., 4.);
Complex c = Complex(emit(Complex(a), Complex(b), add.p2d));
```

The last statement (`Complex(emit(Complex(a), Complex(b), add.p2d))`) expands to the following instructions:
```
push a          ; put by value a on the stack.
push b          ; put by value b on the stack.
add.v2d         ; Packed Double-Precision Floating-Point Add(see: sse2 ADDPD instruction)
```

**Example:**
```
float32 sin_pi_2 = emit(float32(3.14/2), float32.sin);
```

Pushes 3.14/2 as floating point and executes the `float32.sin` native function.

**Example:**
```
int32 b = int32(emit(float32(500)));
assert(b == 1140457472);
```

Pushes the value as floating point and interprets it as an integer.

**Example:**
```
char a[] = emit(int(3), "string");
// a: char[]([3] {'s', 't', 'r'})
```

Creates a slice from the string with length 3.

### The `raise` builtin function.
The function can be used to log a message with or without the current stacktrace and even to abort the execution.

`void raise(int level, int trace, string message, variant inspect);`

Arguments:
- `level`: level to be used:
	- `raise.abort`
	- `raise.error`
	- `raise.warn`
	- `raise.info`
	- `raise.debug`
	- `raise.verbose`

- `trace`: prints the last n stack trace, available only in debug mode.
- `message`: the message to be printed.
- `inspect`: a variant to be inspected, useful in assertions.

In case the raise function is invoked with `abort` the message is logged, and the execution of the application is aborted.
Using other levels the the execution continues, and the message is logged only if the corresponding level is enabled.
In addition, the function logs the file name and line number where the function was invoked.

### The `tryExec` builtin function.
There is no `try` `catch` statement built into the language to handle exceptions.
The `tryExec` function can be used to recover the execution if it was aborted by an error.

`int tryExec(pointer args, void action(pointer args));`

the function executes the given action, returning 0 if it was executed with success.

possible return values are defined by the implementation enumeration:

```
enum {
	noError,
	illegalState,
	stackOverflow,
	divisionByZero,
	illegalMemoryAccess,
	illegalInstruction,
	nativeCallError
}
```
[TODO] the function should return errors instead of codes
[TODO] expose the enumeration of errors to the compiler
[TODO] raise should abort with user defined errors

### Reflection helpers
[TODO: documentation]

- `typename.size`
- `typename.offset`
- `typename.base(type: typename): typename`
- `typename.file(type: typename): .cstr`
- `typename.line(type: typename): int32`
- `typename.name(type: typename): .cstr`

### Memory management
[TODO: documentation]

- `pointer.alloc(ptr: pointer, size: int32): pointer`
- `pointer.fill(dst: pointer, value: int32, size: int32): pointer`
- `pointer.copy(dst: pointer, src: pointer, size: int32): pointer`
- `pointer.move(dst: pointer, src: pointer, size: int32): pointer`

### Type management
[TODO: documentation]

- `variant.as(var: variant, type: typename): pointer`
- `object.create(type: typename): pointer`
- `object.as(obj: object, type: typename): pointer`

### System functions
[TODO: documentation]

- `System.exit(code: int32): void`
- `System.srand(seed: int32): void`
- `System.rand(): int32`
- `System.time(): int32`
- `System.clock(): int32`
- `System.millis(): int64`
- `System.sleep(millis: int64): void`

### Math functions
[TODO: documentation]

- `uint32.zxt(value: int32, offs: int32, count: int32): int32`
- `uint32.sxt(value: int32, offs: int32, count: int32): int32`
- `uint64.zxt(value: int64, offs: int32, count: int32): int64`
- `uint64.sxt(value: int64, offs: int32, count: int32): int64`
- `float32.sin(x: float32): float32`
- `float32.cos(x: float32): float32`
- `float32.tan(x: float32): float32`
- `float32.log(x: float32): float32`
- `float32.exp(x: float32): float32`
- `float32.pow(x: float32, y: float32): float32`
- `float32.sqrt(x: float32): float32`
- `float32.atan2(x: float32, y: float32): float32`
- `float64.sin(x: float64): float64`
- `float64.cos(x: float64): float64`
- `float64.tan(x: float64): float64`
- `float64.log(x: float64): float64`
- `float64.exp(x: float64): float64`
- `float64.pow(x: float64, y: float64): float64`
- `float64.sqrt(x: float64): float64`
- `float64.atan2(x: float64, y: float64): float64`

