# Introduction
Designed to be simple and minimalist.  
Syntax is similar to c/c++ influenced by:
* [C/C++](https://en.wikipedia.org/wiki/C_programming_language)
* [JavaScript](https://en.wikipedia.org/wiki/JavaScript)
* [D programming language](https://en.wikipedia.org/wiki/D_programming_language)
* [C# programming language](https://en.wikipedia.org/wiki/C_Sharp_programming_language)
* [Lua programming language](https://en.wikipedia.org/wiki/Lua_programming_language)

## Features
* Types are themselves variables of type `typename`.
	* Can be parameters of functions `int sizeof(typename type) { return type.size; }`.
	* Reflection is part of the language (some of the internal compiler functions exposed).

* Arrays are more than just typed pointers.
	* [Fixed-size arrays](#arrays-fixed-size-arrays): `int fixed[30];` (length known by the compiler)
	* [Dynamic-size arrays](#slices-dynamic-size-arrays): `int dynamic[] = fixed;` (length known at run-time)
	* [Unknown-size arrays](#pointers-unknown-size-arrays): `int memcmp(byte a[*], byte b[*], int size);` (length known by developer)
	* [Associative arrays](#maps-associative-arrays): `double constants[string] = {"pi": 3.1415, ...};`

* Enumerations are enumerations:
	* Enumeration values are named constants of a given base-type (number, object, function, ...).
	* Enumeration values used in expressions are treated as values of the base-type.
	* Enumeration type variables can be assigned only with values from the enumeration.
	* Enumeration type can be iterated and indexed with ordinal or name.

* Expressions can be aliased with the `inline` keyword.
	* aliasing a type: `inline double = float64;`
	* aliasing a constant: `inline pi = 3.14159265359;`
	* aliasing an expression: `inline min(int a, int b) = a < b ? a : b;`

* Conditional compile-time code generation and declarations with `static if` statement.
	* `static if (int == int32) { /* 32 bit platform code */ }`

* Functions
	* Extend any class with custom methods using [Uniform Function Call Syntax](https://en.wikipedia.org/wiki/Uniform_Function_Call_Syntax)
	* If a function name is a type name, it is treated as a constructor, and should return an instance of that type.

* `new` and `this` are not part of the language.
	* Create and initialize objects like in JavaScript `complex a = {re: 42, im: 2};`
	* `this` is not needed using [Uniform Function Call Syntax](https://en.wikipedia.org/wiki/Uniform_Function_Call_Syntax).

* `public`, `private` access level modifiers are not part of the language.
	* everything is accessible inside one file with no restriction.
	* accessing a non document commented variable, function field or method will raise a high level warning that can be treated as an error.
	* deprecation and any other annotations should be specified in the documentation comment.

