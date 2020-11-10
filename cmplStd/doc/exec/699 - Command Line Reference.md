# Command Line Reference

To compile and execute a script file, the cmpl application must be invoked with the corresponding arguments.

>cmpl \[global.options\]... \[file \[file.options\]...\]...

## Examples

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

### compile

>cmpl test.ci

If there is no global (run, debug or profile) option specified, the given source file(s) will be compiled only.

### debug with breakpoints

>cmpl -debug gl.ext.so -w0 gl.ext.ci -w0 test.gl.ci -wa -b12 -b/o15 -b/o/L19

- Start execution in debug mode `-debug`.

- Import extension library `gl.ext.so`

	- `-w0` Suppress all warnings.

- Compile extension source `gl.ext.ci`

	- `-w0` Suppress all warnings.

- Compile source `test.gl.ci`

	- `-wa` Show all warnings.

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

- `-run[*]`               run at full speed, but without: debug information, stacktrace, bounds checking, ...

	- `/g /G`             dump global variable values (/G includes extra information)

	- `/m /M`             dump memory usage (/M includes extra information)

- `-debug[*]`             run with attached debugger, pausing on uncaught errors and break points

	- `/g /G`             dump global variable values (/G includes extra information)

	- `/m /M`             dump memory usage (/M includes extra information)

	- `/t /T`             trace the execution (/T includes extra information)

	- `/p /P`             print caught errors (/P includes extra information)

	- `/a`                pause on all(caught) errors

	- `/s`                pause on startup

- `-profile[*]`           run code with profiler: coverage, method tracing

	- `/g /G`             dump global variable values (/G includes extra information)

	- `/m /M`             dump memory usage (/M includes extra information)

	- `/t /T`             trace the execution (/T includes extra information)

	- `/p /P`             print execution times (/P includes extra information)

- `-std<file>`            specify custom standard library file (empty file name disables std library compilation).

- `-mem<int>[kKmMgG]`     override memory usage for compiler and runtime(heap)

- `-log[*]<int> <file>`   set logger for: compilation errors and warnings, runtime debug messages

	- `<int>`             set the default log(warning) level 0 - 15

	- `/a`                append to the log file

	- `/d`                dump to the log file

- `-dump[?] <file>`       set output for: dump(symbols, assembly, abstract syntax tree, coverage, call tree)

	- `.scite`            dump api for SciTE text editor

	- `.json`             dump api and profile data in javascript object notation format

- `-api[*]`               dump symbols

	- `/a /A`             include all library symbols(/A includes builtins)

	- `/m`                include `main` builtin initializer symbol

	- `/d`                dump details of symbol

	- `/p`                dump params and fields

- `-doc[*]`               dump documentation

	- `/a /A`             include all library symbols(/A includes builtins)

	- `/m`                include main builtin symbol

	- `/d`                dump details of symbol

	- `/p`                dump params and fields

- `-use[*]`               dump usages

	- `/a /A`             include all library symbols(/A includes builtins)

	- `/m`                include main builtin symbol

	- `/d`                dump details of symbol

	- `/p`                dump params and fields

- `-asm[*]<int>`          dump assembled code: jmp +80

	- `/a /A`             include all library symbols(/A includes builtins)

	- `/m`                include main builtin symbol

	- `/d`                dump details of symbol

	- `/p`                dump params and fields

	- `/g`                use global address: jmp @0x003d8c

	- `/n`                prefer names over addresses: jmp <main+80>

	- `/s`                print source code statements

- `-ast[*]`               dump syntax tree

	- `/a /A`             include all library symbols(/A includes builtins)

	- `/m`                include main builtin symbol

	- `/d`                dump details of symbol

	- `/p`                dump params and fields

	- `/t`                dump sub-expression type information

	- `/l`                do not expand statements (print on single line)

	- `/b`                don't keep braces ('{') on the same line

	- `/e`                don't keep `else if` constructs on the same line


## Compilation units

After the global options are processed, the remaining arguments are processed as units to be compiled.

- `<files with options>`  filename followed by switches

	- `<file>`            if file extension is (.so|.dll) load as library else compile

	- `-w[a|x|<int>]`     set or disable warning level for current file

	- `-b[*]<int>`        break point on <int> line in current file

		- `/[l|L]`        print only, do not pause (/L includes stack trace)

		- `/o`            one shot breakpoint, disable after first hit

