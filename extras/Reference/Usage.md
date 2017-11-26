# Command Line Reference

## Usage:

To compile and execute a script file, the cmpl program must be invoked with the corresponding arguments.

>cmpl \[global.options\]... \[file \[file.options\]...\]...


## Examples:

### run

>cmpl -run test.ci

When the application is started in **run** mode, the compiled code is executed at full speed.

### debug

>cmpl -debug test.ci

When the application is started in **debug** mode, the compiled code is executed with some additional checks.
In case of an error, the debugger will pause the execution of the code at the fault location.


### profile

>cmpl -profile test.ci

When the application is started in **profile** mode, the compiled code is executed and measured.
When the execution finishes, the collected data is dumped to the to the console, or to the specified file.

### debug with breakpoints

>cmpl -debug gl.ext.so -w0 gl.ext.ci -w0 test.gl.ci -wx -b12 -b/o15 -b/o/L19

- Start execution in debug mode `-debug`.

- Import extension library `gl.ext.so`

	- `-w0` Suppress all warnings.

- Compile extension source `gl.ext.ci`

	- `-w0` Suppress all warnings.

- Compile source `test.gl.ci`

	- `-wx` Treating warnings as errors.

	- `-b12` Break execution each time when line 12 is hit.

	- `-b/o15` Break execution when line 12 is first hit.

	- `-b/o/L19` Print message and stack trace when line 19 is first hit.


### profile and dump to JSON

>cmpl -profile/t -api/a/d/p -dump.json test.stdc.json "test.stdc.ci"

- Start execution in profile mode `-profile/t`.

	- `/t` include call tree in dump


- Include symbols in dump `-api/a/d/p`

	- `/a` include all(builtin) symbols in the dump

	- `/d` include symbol details in the dump

	- `/p` include function parameters and record fields


- dump in json format `-dump.json` to file `test.stdc.json`.

- compile source file `test.stdc.ci`

If JSON output format is used, the generated file can be opened with the Inspector.html tool to visualize the result.


## Global options


- `-std<file>`

	custom standard library file (empty file name disables std library compilation).


- `-mem<int>[kKmMgG]`

	override memory usage for compiler and runtime(heap)


- `-log[*]` `<file>`

	override log output from console to <file> for compiler(errors and warnings) and runtime(messages from raise).

	additional arguments:

	- `/a` append output to the existing log file


- `-dump[?]` `<file>`

	override dump output from console to <file> (symbols, assembly, syntax tree, coverage, call tree) output:

	additional arguments:

	- `.scite` dump api for SciTE text editor

	- `.json` dump api in javascript object notation format


- `-api[*]`

	dump symbols (typenames, variables and functions)

	additional arguments:

	- `/a` include all(builtin) symbols

	- `/m` include the `.main` initializer function

	- `/d` include symbol details in the dump (size, offset, owner, etc)

	- `/p` include function parameters and record fields

	- `/u` include usages of the symbols


- `-asm[*]`

	dump function instructions (disassembled code)

	additional arguments:

	- `/a` use global address: (@0x003d8c)

	- `/n` prefer names over addresses: <main+80>

	- `/s` print source code statements

	- `/m` include the `.main` initializer function

	- `/d` include symbol details in the dump (size, offset, owner, etc)

	- `/p` include function parameters and record fields

	- `/u` include usages of the symbols


- `-ast[*]`

	dump syntax tree

	additional arguments:

	- `/t` dump sub-expression type information

	- `/l` do not expand statements (print on single line, or replace with `{ ... }`)

	- `/b` format.option: don't keep braces ('{') on the same line

	- `/e` format.option: don't keep `else if` constructs on the same line

	- `/m` include the `.main` initializer function

	- `/d` include symbol details in the dump (size, offset, owner, etc)

	- `/p` include function parameters and record fields

	- `/u` include usages of the symbols


- `-run[*]`

	run code with maximum speed: no(debug information, stacktrace, bounds checking, ...)

	additional arguments:

	- `/g` on exit dump value of global variables


- `-debug[*]`

	execute the compiled code with attached debugger, pausing on uncaught errors and break points.

	additional arguments:

	- `/s` pause on startup

	- `/a` pause on all(caught and uncaught) errors

	- `/l` or `/L` print the cause message of caught errors (/L includes stack trace)

	- `/g` on exit dump value of global variables

	- `/h` on exit dump memory related statistics

	- `/p` on exit dump function statistics


- `-profile[*]`

	run code with attached profiler, calculate code coverage, trace method calls

	additional arguments:

	- `/t` dump call tree

	- `/a` include all the information in the dump

	- `/g` on exit dump value of global variables

	- `/h` on exit dump memory related statistics

	- `/p` on exit dump function statistics


## Compile files

After global options are processed, the rest of arguments are processed as file names to be compiled.

Every filename may have extra arguments, which must be given after this argument.

- `<file>`

	If file extension is `.so` or `.dll` import it as an extension library, else compile it as source code.


- `-w[a|x|<int>]`

	Override warning level for current file.

- `-b[*]<int>`

	Break execution on <int> line in current file (requires `-debug` global option).

	additional arguments:

	- `/o` One shot breakpoint, disabled after first hit.

	- `/l` or `/L` Print only, do not pause execution (/L includes stack trace).
