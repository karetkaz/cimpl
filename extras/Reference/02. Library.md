# Library reference

<!-- TODO
	Builtins
		emit
		raise
		memmgr (realloc, malloc, free)
		memcpy
		memset
		execSafe := tryExec
		execAsync
		...
	String
	Stream
-->

## The `emit` intrinsic function.

Emit is a low level builtin intrinsic for low level operations like emitting virtual machine instructions.

- emit can have as arguments values and opcodes.
- the return type is defined by the type of the first argument.

**Example:**

```
define add(Complex a, Complex b) = Complex(emit(struct, v2d.add, Complex(b), Complex(a)));
```

the above example has the following meaning:

```
- // parameters are evaluated from right to left
- push a          ; put by value a on the stack.
- push b          ; put by value b on the stack.
- v2d.add         ; Packed Double-Precision Floating-Point Add(see: sse2 ADDPD instruction)
```

- TODO: if the first argument is the struct keyword in a declaration,
the resulting type will match the declared type.

**Example:**

```
// create a slice
char a[] = emit(struct, ref("string"), i32(3));
// then a == {'s', 't', 'r'};
```

**Example:**

```
float32 a = 500;
int32 b = emit(struct, float32(a));
assert(b == 1140457472);
```

## Exception handling

There is no try catch to handle exceptions, but the `raise` function can be used to abort the execution of the code.

### The `raise` builtin function.

`void raise(int level, string message, variant inspect, int trace);`

The `raise` function can be used to abort the execution, to log a message with or without the current stacktrace.

Arguments:
- `level`: level to be used:
	- `raise.abort`
	- `raise.error`
	- `raise.warn`
	- `raise.info`
	- `raise.debug`
	- `raise.verbose`

- `message`: the message to be printed.
- `inspect`: a variant to be inspected, useful in assertions.
- `trace`: prints the last n stack trace, available only in debug mode.

In case the raise function is invoked with `abort` the message is logged, and the execution of the application is aborted.
Using other levels the the execution continues, and the message is logged only if the corresponding level is enabled.
In addition, the function logs the file name and line number where the function was invoked.


### The `tryExec` builtin function.

`int tryExec(void action());`

`int tryExec(pointer args, void action(pointer args));`

the function executes the given action, returning 0 if it was executed with success.

possible return values are defined by the implementation enumeration:

```
enum {
	noError,
	invalidIP,
	invalidSP,
	stackOverflow,
	memReadError,
	memWriteError,
	divisionByZero,
	illegalInstruction,
	nativeCallError,
	executionAborted
};
```
[TODO] expose the enumeration to the compiler.
