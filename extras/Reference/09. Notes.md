
## Compilation memory management

The memory is organized in 4 segments:

- meta: read only
	- identifiers and string constants (are emitted in the compilation phase).
	- typename and variable metadata (are emitted in the compilation phase).
	- [TODO] enumeration values.

- code: read only
	- functions body.

- data: writeable
	- static variables.

- heap: writeable
	- heap memory
	- stack(s).

[TODO] code and data are mixed together.
[TODO] enums are generated int the mixed code + data section.

In the compilation phase the lower part of the memory is filled with persistent data which remains available for code execution,
while the upper part is filled with token structures and other temporary data, that will be overwritten on code execution.

## Constant evaluation [TODO]

Constant expression evaluation should be done by the vm.

An expression is constant:

- if no memory writing is performed except on the top of the stack.
- memory read occurs only from the read only region.
- no non const function is invoked?
- no native call is invoked? (what about sin(double x) and the others).
- no stack manipulation is performed?

## Code generation and execution

Every compilation results in a `.main` hidden function, which contains the initialization of all static variables,
and all the global statements (which are not part of function bodies).

The main function terminates by calling the halt library function, which is the last instruction in the main function.

### Implementing enumerations

- There are basically 2 different enumeration kinds: reference or value based.
	- when the base type is value type, the members of the enumeration will be ordered in the memory,
		like in an array. In this case the lookup is easy and can be done in constant time.
	- when the base type is reference type, ex strings or functions, the enumeration holds references to each member.
		lookup in this case must match each member in the enumeration, if references are unordered.

## Function delegates
- Functions are assigned and passed to functions as delegates,
meaning that there is an extra parameter pushed to the stack,
a pointer to the callee stack to access captured variables,
or the (this/self/first arg) object in case of instance methods.

- When generating functions, an extra instruction has to be generated
before the implementation, this way any function can be passed as delegate.
This firs instruction should be `set.b32 0`, pop the closure parameter
from the stack replacing it with the return address placed by call.
