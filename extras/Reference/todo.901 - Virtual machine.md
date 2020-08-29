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

- meta\[r--]:
	- identifiers and string constants (are emitted in the compilation phase).
	- typename and variable metadata (are emitted in the compilation phase).
	- enumeration values and initializer literals.

- code\[r-x]:
	- functions implementations.

- data\[rw-]:
	- static variables.

- heap\[rw-]:
	- heap memory
	- stack(s).

In the compilation phase the lower part of the memory is filled
with persistent data which remains available for code execution,
while the upper part is filled with token structures and other
temporary data, that will be overwritten on code execution.

