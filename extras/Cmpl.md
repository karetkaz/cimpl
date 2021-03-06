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

	* [Fixed-size arrays](#fixed-size-arrays): `int fixed[30];` (length known by the compiler)

	* [Dynamic-size arrays](#dynamic-size-arrays): `int dynamic[] = fixed;` (length known at run-time)

	* [Unknown-size arrays](#unknown-size-arrays): `int memcmp(byte a[*], byte b[*], int size);` (length known by developer)

	* [Associative arrays](#associative-arrays): `double constants[string] = {"pi": 3.1415, ...};`

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

# Lexical structure

## Comments

* line comments: `// ...`

* block comments: `/* ... */`

* nested comments: `/+ ... +/`

## Identifiers

Identifiers are named references of types, variables and functions.

**[Syntax](Cmpl.g4)**

```antlrv4
Identifier: Letter (Letter | Number)*;
```

## Keywords

Keywords are reserved words, which can not be used as identifiers.

* break

* const

* continue

* else

* emit

* enum

* for

* if

* inline

* parallel

* return

* static

* struct

## Literals

[Literals](https://en.wikipedia.org/wiki/Literal_(computer_programming)) are compile time constant values.

**[Syntax](Cmpl.g4)**

```antlrv4
Literal
    : '0'[bB][0-1]+
    | '0'[oO]?[0-7]+
    | '0'[xX][0-9a-fA-F]+
    | Decimal Suffix?
    | Decimal Exponent Suffix?
    | '.' Number+ Exponent? Suffix?
    | Decimal '.' Number* Exponent? Suffix?
    | '\'' .*? '\''
    | '"' .*? '"'
    ;
```

### Integer and floating-point literals

Integers can be represented in decimal, binary, octal, or hexadecimal format,
while floating point literals can be specified only in decimal format.

#### Decimal prefix

The following prefixes may be used to specify the radix of an integer:

* binary: `0b` or `0B`

* octal: `0` or `0o` or `0O`

* hexadecimal: `0x` or `0X`

#### Decimal suffix

The following suffixes are available to override the type of a decimal value:

* `d`: results a int32 constant, ex: `inline i32Value = 3d;`

* `D`: results a int64 constant, ex: `inline i64Value = 3D;`

* `u`: results a uint32 constant, ex: `inline u32Value = 3u;`

* `U`: results a uint64 constant, ex: `inline u64Value = 3U;`

* `f`: results a float32 constant, ex: `inline f32Value = 3f;`

* `F`: results a float64 constant, ex: `inline f64Value = 3F;`

### Character and string literals

[TODO: documentation]

#### Escape sequences

Escape sequences are used to define certain special characters within string literals.

The following escape sequences are available:

* `\'`: single quote

* `\"`: double quote

* `\?`: question mark

* `\\`: backslash

* `\a`: audible bell

* `\b`: backspace

* `\f`: form feed - new page

* `\n`: line feed - new line

* `\r`: carriage return

* `\t`: horizontal tab

* `\v`: vertical tab

* `\nnn`: arbitrary octal value, maximum of 3 characters.

* `\xnn`: arbitrary hexadecimal value, always 2 characters.

* `\<new line>`: wraps on next line.

- Octal escape sequences have a limit of three octal digits,
but terminate at the first character that is not a valid octal digit if encountered sooner.

- Hexadecimal escape sequences parses always the next two characters.

#### Multi line Strings

A string literal is multi line if it starts with a backslash followed by a newline.

**Example**

```cmpl
string html = "\
<html>
  <head/>
  <body>
    <p>
      Hello<br/>
      Multiline<br/>
      World<br/>
    </p>
  </body>
</html>
";
```

## Operators

[TODO: documentation]

* !, ~, -, +, &

* (), [], .

* +, -, \*, /, %

* &, |, ^, <<, >>

* ==, !=

* <, <=, >, >=

* ||, &&, ?:

* =, +=, -=, *=, /=, %=, &=, |=, ^=, <<=, >>=, >>>=

* ,

# Expressions

Expressions are responsible for computing values.

**[Syntax](Cmpl.g4)**

```antlrv4
expression
    : Literal                                                                                        # LiteralExpression
    | Identifier                                                                                  # IdentifierExpression
    | '(' expressionList? ')'                                                                  # ParenthesizedExpression
    | expression '(' expressionList? ')'                                                        # FunctionCallExpression
    | '[' expressionList? ']'                                                                  # ParenthesizedExpression
    | expression '[' expressionList? ']'                                                         # ArrayAccessExpression
    | expression '[' expression '..' expression ']'                                               # ArraySliceExpression
    | expression '.' Identifier                                                                 # MemberAccessExpression
    | unary expression                                                                                 # UnaryExpression
    | expression arithmetic expression                                                            # ArithmeticExpression
    | expression bitwise expression                                                                  # BitwiseExpression
    | expression relational expression                                                            # RelationalExpression
    | expression equality expression                                                                # EqualityExpression
    | expression ('='| '*=' | '/=' | '%=' | '+=' | '-=' ) expression                              # AssignmentExpression
    | expression ('&=' | '|=' | '^=' | '<<=' | '>>=') expression                                  # AssignmentExpression
    | expression ('&&' | '||') expression                                                            # LogicalExpression
    | expression '?' expression ':' expression                                                   # ConditionalExpression
    ;
```

## Primary expression
[TODO: documentation]

## Unary expressions

Unary expressions are composed from an unary operator and one operand.

- `!`: logical not. Results true or false.

- `~`: complement. Valid for integer types.

- `-`: unary minus. Change the sign of a number.

- `+`: unary plus. ???

**[Syntax](Cmpl.g4)**

```
unary: ('&' | '+' | '-' | '~' | '!');
```

## Binary expressions

Binary expressions are composed from a binary operator and two operands.

### Arithmetic expression

Are used to return new values computed from the two operands.

- `+, -` Additive operators

- `*, /, %` Multiplicative operators

**[Syntax](Cmpl.g4)**

```
arithmetic: ('*' | '/' | '%' | '+' | '-');
```

### Bitwise expression

May be used on integer types only.

- `&, |, ^` Bitwise operators

- `<<, >>` Bit shift operators

**[Syntax](Cmpl.g4)**

```
bitwise: ('&' | '|' | '^' | '<<' | '>>');
```

### Relational expressions

Are used to compare two operands, if one is les or greater than the other.
The result is a boolean value(true or false).

**[Syntax](Cmpl.g4)**

```
relational: ('<' | '<=' | '>' | '>=');
```

### Equality expressions

Are used to check if the left and right operands are equal or not.
The result is a boolean value(true or false).

**[Syntax](Cmpl.g4)**

```
equality: ('==' | '!=');
```

### Assignment expressions

This operator assigns the value on the right to the operand on the lef side.

**Example**

```
a = 5;
a += 3 + b;
```

Composed operators are expanded, this means that `a += 3 + b` is converted to `a = a + (3 + b)`.

### Logical expressions

[TODO: implementation]

- `||`: logical or operator returns true if **any** of its operands is true.

- `&&`: logical and operator returns true if **all** of its operands are true.

**Example**

```
bool variable = a() || b() || c() || d();
```

The value of `variable` will be true if any of the functions(a, b, c, d) is evaluated to true.
The evaluation stops when the first expression is evaluated to true.


**Example**

```
bool variable = a() && b() && c() && d();
```

The value of `variable` will be true if all of the functions(a, b, c, d) are evaluated to true.
The evaluation stops when the first expression is evaluated to false.

## Ternary expression

The `?:` (conditional) operator is the only ternary operator with 3 operands:

- test operand: based on its value the second or the third operand is returned.

- true operand: evaluated and returned only if the first operand evaluates to true.

- false operand: evaluated and returned only if the first operand evaluates to false.

**Example**

```
int variable = a() ? b() : c();
```

First `a()` is evaluated, and if its value is:

- true: `b()` is evaluated and returned

- false: `c()` is evaluated and returned

## Operator precedence table

| Operator   | Title          | Example    | Description |
|-----|-----------------------|------------|-------------|
| 15: Primary ||| Associates left to right |
| ( ) | Function call           | sqrt(x)    ||
| [ ] | Array subscript         | values[10] ||
|  .  | Member access           | child.name ||
| 14: Unary ||| Associates right to left |
|  +  | Unary plus              | +a         ||
|  -  | Unary minus             | -a         ||
|  ~  | Bitwise not             | ~077       ||
|  !  | Logical not             | !ready     ||
| 13: Multiplicative ||| Associates left to right |
|  *  | Multiplication          | i * j||
|  /  | Division                | i / j||
|  %  | Modulus                 | i % j||
| 12: Additive ||| Associates left to right |
|  +  | Addition                | value + i||
|  -  | Subtraction             | x - 10||
| 11: Bit shift ||| Associates left to right |
|  << | Left shift              | byte << 4||
| \>> | Right shift             | i >> 2||
| 10: Relational ||| Associates left to right |
|  <  | Less than               | i < 10||
|  <= | Less than or equal to   | i <= j||
| \>  | Greater than            | i > 0||
| \>= | Greater than or eq to   | count >= 90||
| 9: Equality ||| Associates left to right |
|  == | Equal to               | result == 0 ||
|  != | Not equal to           | c != EOF ||
| 8: Bitwise AND ||| Associates left to right |
|  &  | Bitwise AND            | word & 077 ||
| 7: Bitwise XOR ||| Associates left to right |
|  ^  | Bitwise XOR            | word1 ^ word2 ||
| 6: Bitwise OR ||| Associates left to right |
| &#124; | Bitwise OR             | word &#124; bits ||
| 5: Logical AND ||| Associates left to right |
|  && | Logical AND            | j > 0 && j < 10 ||
| 4: Logical OR ||| Associates left to right |
|&#124;&#124;| Logical OR             | i > 80 &#124;&#124; ready ||
| 3: Conditional ||| Associates right to left |
|  ?: | Conditional operator   |a > b ? a : b ||
| 2: Assignment ||| Associates right to left |
| = /= %= += -= &= ^= &#124;= <<= >>= | Assignment operators | ||
| 1: Collection ||| Associates left to right |
|  ,  | Comma operator         | i = 10, j = 0||

# Statements
Statements are the basic blocks of a program.

**[Syntax](Cmpl.g4)**
```antlrv4
statement
    : ';'                                                                                               # EmptyStatement
    | 'inline' Literal ';'                                                                            # IncludeStatement
    | qualifiers? '{' statementList '}'                                                              # CompoundStatement
    | qualifiers? 'if' '(' for_init ')' statement ('else' statement)?                                      # IfStatement
    | qualifiers? 'for' '(' for_init? ';' expression? ';' expression? ')' statement                       # ForStatement
    | 'for' '(' variable ':' expression ')' statement                                                 # ForEachStatement
    | 'return' initializer? ';'                                                                        # ReturnStatement
    | 'break' ';'                                                                                       # BreakStatement
    | 'continue' ';'                                                                                 # ContinueStatement
    | expression ';'                                                                               # ExpressionStatement
    | declaration                                                                                 # DeclarationStatement
    ;
```

## Block statement
Block statement groups zero ore more statement as a single statement.

### Parallel block statement
Parallel block statements can be used to execute a block of statements parallel.

## If statement
The if statement is a selection statement, that can be used to execute a section of code, only if a condition is met.

Main purpose of the if statement is to handle exceptional cases in the control flow.

### Static if statement
The static if construct can be used as a compile time check.

**Example**
```
// if double is not defined, define it as float64
static if (typename(double) == null) {
inline double = float64;
}
```

- if the condition evaluates to true:
  - the declarations contained by the block will be exposed to the outer scope.

- if the condition evaluates to false:
  - declarations contained by the block will be exposed only to the inner scope.
  - statements inside the scope will be type-checked.
  - compilation error will be reported as warnings.
  - inlined files will be not processed.
  - byte code generation will be skipped.

## Switch statement

## For statement
The for statement is a repetition statement, that can be used to execute a section of code in a loop, while a condition is met.

The syntax of the for statement is similar to c like language for statement syntax.

### For-each statement
The for-each statement enumerates the elements of an iterable.

To use the foreach like form of the for statement, two functions are required to be defined:
* `iterator`: this function prepares the iterator from a type.
	* the argument for this function is the object you want to iterate.
	* it should return an iterable type(this will be the first argument of the next function).
* `next`: this function advances to the next iterable element, it may be defined also in the iterator.
	* the first argument is the object returned by the iterator function. This has to be passed by reference.
	* the second argument is optional, in case we want to iterate with a value, and not the iterator object. This argument must be passed by reference or as inout.
	* it must return a boolean value: true if there was a next element, false otherwise.

**Example**
```
struct Range {
	const int min;
	const int max;
}
inline Range(int min, int max) = { min: min, max: max };

struct RangeIterator {
	int current;
	const int end;

	// RangeIterator is iterable
	static bool next(RangeIterator &this) {
		if (this.current < this.end) {
			this.current += 1;
			return true;
		}
		return false;
	}

    // make RangeIterator iterable using an int
    static bool next(RangeIterator &it, int &value) {
        if (RangeIterator.next(it)) {
            value = it.current;
            return true;
        }
        return false;
    }
}

// make the iterator for the Range type (make Range iterable)
inline iterator(Range r) = RangeIterator {
	current: r.min;
	end: r.max;
};

// now we can iterate over any range using the iterator
for (RangeIterator it : Range(10, 20)) {
	println(it.current);
}

// also we can iterate over the range with an int value
for (int i : Range(10, 20)) {
	println(i);
}

```

### Static for statement
[TODO: implementation]

The static construct of the statement expands inline the statement of the loop.

**Example**
```
static for (int i = 0; i < 5; i += 1) {
	print(i);
}
```

generates exactly the same code like:

```
print(0);
print(1);
print(2);
print(3);
print(4);
```

### Parallel for statement
[TODO: implementation]

The parallel version of the for statement executes the statements of the loop on a worker,
than waits each of them to finish (in case we have fever workers than jobs or a single worker,
the job will be executed on the main worker).

**Example**: parallel for statement
```
parallel for (int i = 0; i < 5; i += 1) {
	print(i);
}
print(99);
```

**Example**: for statement with a parallel block statement
```
for (int i = 0; i < 5; i += 1) parallel {
	print(i);
}
print(99);
```

These two examples may result different output. In the first example the last statement:
`print(99);` will be executed last, while in the second example it is possible that
this is not the last executed statement.

## While statement

## Do-while statement

## Break statement
The break statement terminates the execution of the innermost enclosing loop.

[TODO: example]

## Continue statement
The continue statement terminates the current and begins the next iteration of the innermost enclosing loop.

[TODO: example]

## Return statement
The return statement terminates the execution of the current function.

[TODO: example]
- If an expression is given, before returning the expression is assigned to the result parameter of the function.
- It is allowed to return an expression of type void (invocation of a function returning void),
even if the function specifies a void return type. The expression will be evaluated, but nothing will be returned.

## Declaration statement
Declaration statement declares a typename, function, variable or an alias.

## Expression statement
Expression statements is an expression terminated with ';'

Only some of the expressions can be used to form a statement:
- assignment: `a = 2;`
- invocation: `foo();`

Expression statements such as `a * 4;` are considered invalid.

# Declarations

Declarations adds new variables, functions or types to the program.

**[Syntax](Cmpl.g4)**

```antlrv4
declaration
    : qualifiers? 'enum' identifier? (':' typename)? '{' propertyList '}'                              # EnumDeclaration
    | qualifiers? 'struct' identifier? (':' (Literal | typename))? '{' declarationList '}'             # TypeDeclaration
    | qualifiers? 'inline' identifier ('(' parameterList? ')')? '=' initializer ';'                   # PropertyOperator
    | qualifiers? 'inline' '('')' ('(' parameterList? ')')? '=' initializer ';'                        # InvokerOperator
    | qualifiers? 'inline' '['']' ('(' parameterList? ')')? '=' initializer ';'                        # IndexerOperator
    | qualifiers? 'inline' unary ('(' parameterList? ')')? '=' initializer ';'                           # UnaryOperator
    | qualifiers? 'inline' arithmetic ('(' parameterList? ')')? '=' initializer ';'                 # ArithmeticOperator
    | qualifiers? 'inline' bitwise ('(' parameterList? ')')? '=' initializer ';'                       # BitwiseOperator
    | qualifiers? 'inline' relational ('(' parameterList? ')')? '=' initializer ';'                 # RelationalOperator
    | qualifiers? 'inline' equality ('(' parameterList? ')')? '=' initializer ';'                     # EqualityOperator
    | qualifiers? (variable | function) ( '=' initializer)? ';'                                    # VariableDeclaration
    | qualifiers? function '{' statementList '}'                                                # FunctionImplementation
    ;
```

## Variables

A [Variable](https://en.wikipedia.org/wiki/Variable_(computer_science)) is
a symbolic name associated with a value, this value may be changed at runtime.

### Constant variables

Variables marked with the `const` attribute may be assigned only at initialization.

### Static variables

Variables marked with the `static` attribute will point to the same global memory.
Initialization of all static variables are executed when the main function is executed.

**Example**: call count

```cmpl
int countCalls() {
	// global count variable
	static int count = 0;

	return count += 1;
}
```

**Example**: instance count

```
struct instance {
	// global count variable
	static int count = 0;

	// instance variable
	const int instanceNr;
}

// construct the next instance
inline instance() = {
	instanceNr: instance.count += 1;
};
```

### Initialization

[TODO]

**Example**: initializer function

```
// call the initializer function
Complex x = Complex(1, 2);

```

**Example**: literal initialization

```
// literal initialization
Complex x = {re: 1, im: 2};

// literal initialization with type
Model x = Sphere {x:0, y:0, z:0, radius: 20};
```

**Example**: array initialization

```
// initialize all elements with the value 4
int a[100] = {*: 4};

// initialize the first 3 elements with the given values
// the rest will be initalized with 64
int a[100] = {1, 2, 3, *: 64};

// initialize all elements with null, then override some of them
string xmlEscape[255] = {
	*: null;
	'"': "&quot;";
	'\'': "&apos;";
	'<': "&lt;";
	'>': "&gt;";
	'&': "&amp;";
};
```

#### Default type initializer

Some of the builtin types have default type initializer (int, float, ...),
and some must be initialized when a new instance is created (pointer, variant, typename, function).
All enumerated types should have no default initializer, so they must be explicitly initialized.

**Example**

```
int a;       // ok, a is initialized with default type initializer.
typename b;  // error: variable `b` must be initialized.
```

#### Default field initializer

All constant fields of a record must be initialized when creating an instance.

**Example**
```
Complex x;                     // error: variable `x` must be initialized.
Complex x = {};                // error: all fields of `x` must be initialized.
Complex x = {re: 2};           // ok, `im` initialized with default field initializer.
Complex x = {re: 2; im: -1;};  // ok, all fields are initialized.
```

## Arrays

The array type is a collection of zero or more elements of the same type.

### Fixed-size arrays
The fixed size array will allocate the space required for all the elements

**Example**
```
int a[2] = {42, 97};
```

- Are passed to functions by reference.
- Type of elements and length is known by the compiler.

### Dynamic-size arrays
Slices are a pair of a pointer to the first element and an integer containing the length of the slice.

**Example**
```
int a[] = {42, 97, 13};
```

- Are initialized, assigned and passed to functions by reference followed by the length.
- Type of elements is known by the compiler.
- The length is known at runtime.

### Unknown-size arrays
Pointers are arrays without length. This type is helpful to pass data to native functions.
Use `pointer` for the special `void *` type from c, when the type of elements is unknown.

**Example**
```
int a[*];
```

- Are initialized, assigned and passed to functions by reference.
- Type of elements is known by the compiler.
- The length may be known by the developer.

### Associative arrays
[TODO: implementation]

**Example**

```
double constants[string] = {
	pi: Math.pi;
	"e": Math.e;
};

assert(constants["pi"] == Math.pi);
assert(constants["e"] == Math.e);
assert(constants.e == Math.e);
```

Collection types(Array, Stack, Queue, Set, Bag, Map, …) are part of the run-time library.

## Functions
[TODO: documentation]

- Functions that are not implemented, will be converted to function references.
- Functions references must be initialized, or implemented (re-declared as implemented function).
- Functions are assigned and passed to functions as delegates:
	- for inline functions the current stack pointer is pushed to give access to local variables.
	- for member functions the objects reference is pushed to give access to the _self_ variable.

**Example**
```
float64 sum(float64 data...) {
	double sum = 0;
	foreach(data, inline void(float64 value) {
		// sum is a closure variable.
		sum += value;
	});
	return sum;
}
```

### Forward declared functions
Forward declared functions are functions that can be used before they are implemented.
Every forward declared function will be converted to a function reference (can decrease performance).

**Example**
```
// forward declaration of function (no implementation).
double forward();

// invoke the function.
double x = forward();

// implementaton of forward declared function.
double forward() {
	return Math.pi;
}
```

### Function references
Function references can be initialized to point to a function.
If a functions parameter is a function it is converted to a function reference.

**Example**
```
// referencing the sin function
double reference(double value) = Math.sin;

// invoke the function.
double x = reference();

// compare is a function reference.
void sort(int values[], int compare(int a, int b)) {
	...
}
```

### Functions with special names

If a function name is a type name, by convention it is considered to be a constructor for that type,
so it should return an instance of the type represented by its name.

## Enumerations
An enumeration declares a list of named constants all of a given type.

- It may also add a new type, the enumeration type, if the enumeration name is specified.
- The enumeration type is a reference type, and its instances may be assigned
only with null or the values listed inside the enumeration.
- The type of the enumerated values can be specified.
If not specified the type of elements of the enum will be int.
- Every enumeration variable declaration must be initialized.

**Example**
```
struct vec3f {
	float x;
	float y;
	float z;
}

enum coord: vec3f {
	x: vec3f(1, 0, 0);
	y: vec3f(0, 1, 0);
	z: vec3f(0, 0, 1);
}

coord e1 = coord.x;             // ok: enum variable can be assigned with a value from the enumeration.
coord e2 = e1;                  // ok: enum variable can be assigned with the same type of variable.
e2 = coord.x;                   // ok.
e2 = e1;                        // ok.

vec3f v1 = coord.y;             // ok: values from enum are of type vec3f, so it can be assigned.
vec3f v2 = e1;                  // ok: enum variables can be assigned to variables of their base type.
v2 = coord.x;                   // ok.
v2 = e2;                        // ok.

coord e3 = v1;                  // error: enum variable can be assigned only from a value from the enumeration.
e3 = v1;                        // error.
coord e0;                       // error: enum variable must be initialized.
```

### Anonymous enumeration
An anonymous enumeration does not create the enumeration type,
and it exposes the enumerated values to the declaring scope.

**[Example](../../lib/std/math/Complex.ci)**
```
struct complex {
	const float64 re;       // real
	const float64 im = 0;   // imaginary

	enum: complex {
		zero: { re: 0 };
		unit: { re: 1 };
	}
}
```

### Indexing enumeration
The enumerated type can be indexed with ordinal(int) or name(string).

**Example**: ordinal
```
assert(coord[1] == coord.y);
```

**Example**: name
```
assert(coord["y"] == coord.y);
```

### Iterating enumeration
The values of an enumerated type can be iterated using the for-each statement.

**Example**
```
for (coord elem: coord) {
	...
}
```

## Records

[Records](https://en.wikipedia.org/wiki/Record_(computer_science)) are user specified compound types.

Records may contain only instance or static members and methods:

### Members

[TODO: documentation]

### Static members

```
struct Math {
	static const float64 pi = 3.14159265358979323846264338327950288419716939937510582097494459;
	...
	static float64 min(float64 a, float64 b) {
		return a < b ? a : b;
	}
	...
}

float64 two_pi = 2 * Math.pi;

float64 minimum = Math.min(a, b);
```

if a member/method inside the record is declared `static`, it will become a global variable/function, and
it will be accessible only through the declaring type:

- `pi` is a `static` member, and can be accessed as `Math.pi`.

- `min` is a `static` method, and can be accessed as `Math.min`.

- if a `static` method and is initialized, it will become a [function reference](#function-references)

- if a `static` method is not implemented, it will become a [forward declared function](#forward-declared-functions)

### Constant members

[TODO: documentation]

**[Example](../../lib/std/math/Complex.ci)**

```
struct complex {
	const float64 re;           // real part
	const float64 im = 0;       // imaginary part
	...
}

complex a = {re: 2, im: -1};    // ok: all const members are initialized: re = 2 and im = -1
complex b = {re: 2};            // ok: all const members are initialized: re = 2 and im = 0
complex c = {im: 1};            // error: re must be initialized
complex d;                      // error: re must be initialized
a.re = 6;                       // error: re is constant and can not be assigned, only initialized.
```

if a member inside the record is declared as `const`, the compiler will require its initialization
and reject further assignments:

- `re` must be initialized each time a complex variable is instantiated (has no default field initializer).

- if `im` is not explicitly initialized, it will be initialized with the default field initializer.

### Methods

[TODO: fix documentation]

**[Example](../../lib/todo/todo.Stream.ci)**

```
struct TextReader: Closeable {
	const ByteReader reader;

	// abstract method
	int decode(char chars[], ByteReader reader);

	// virtual method
	int read(TextReader this, char chars[]) {
		return this.decode(chars, this.reader);
	}

	// static method
	static int read(TextReader this) {
		char chars[1];
		result = this.read(chars);
		if (result > 0) {
			result = chars[0];
		}
		return result;
	}

	// overrided method
	void close(TextReader this) {
		this.reader.close();
	}

	// ...
}
```

if a method inside the record is not implemented or initialized, it will behave as an **abstract** method.

- the method must be overridden in a record that extends this record.

- the method will be looked up at runtime, not at compile time.

if a method inside the record is initialized, it will behave as a **virtual** method.

- the method can be overridden in a record that extends this record.

- the method will be looked up at runtime, not at compile time.

- should be used only to delegate the implementation.

- it will declare only the member function.

if a method inside the record is implemented, it will behave as a **virtual** method.

- the record will declare both the member and the static function with the same name.

- the method can be overridden in a record that extends this record.

- the method will be looked up at runtime, not at compile time.

In the example:

- `reader` is declared as `const`, so it must be initialized on instance creation, and can not be changed by assignment.

	- the compiler will reject assignments as `instance.reader = null;`

- `int decode(char chars[], ByteReader reader)` is **abstract**, it must be overridden in the inheritance chain.

- `int read(TextReader this, char chars[])` is **virtual**, it can be overridden in the inheritance chain.
	- invoked as `TextReader.read(instance)` ensures that the original method is invoked.
	- invoked as `instance.read()` might call an _extension_ method.

- `int read(TextReader this)` is **static** method, it will be looked up at compile time.
	- invoked as `TextReader.read(instance, buff)` ensures that the original method is invoked.
	- invoked as `instance.read(buff)` might call an _extension_ or an _overridden_ method.

- `void close(Utf8Reader this)` overrides the abstract method from the base class `Closeable`.

### Static methods

[TODO: fix documentation]

### Static records

Records which are declared `static` will have all members static (sort of a namespaces).
These types of records will have no size, and can not be instantiated.
The best example of its usage is `static struct Math {...}` 

### Constant records

[TODO: implementation]

Records which are declared `const` will have all its members as constants (immutable record).

### Packed records

A record can be packed with a small integer of a power of 2. [0, 1, 2, 4, 8, 16, 32],
this means that the compiler will not generate gaps between the members to achieve best performance.

**Example**

```
struct union32: 0 {
	int32 i32;
	float32 f32;
}

union32 fi32 = { i32: 3 };
```

- When a struct is packed with 0, it becomes a c like union,
meaning every member of it will start at the same memory location.

**Example**

```cmpl
struct instruction: 1 {
	uint8 opc;	// operation
	int32 arg;	// argument
}
```

- When a record is packed with 1, the compiler will not generate any gap between the members,
this may result in very expensive memory access to the members.

### Extended records

Extended records are usually allocated on the heap, and are automatically released,
when there are no more references pointing to the object.
The compiler however should allocate on the heap only variables that escape their scope.
The base type of every extended type is the builtin type `object`.

**Example**

```
struct Complex: object {
	const double re;
	const double im = 0;
}

Complex c1 = { re: 8 };
```

The variable `c1`:

- might be allocated on heap.

- is initialized with: `re: 8` and `im: 0`;

## Aliasing

Aliasing is an analogue to c preprocessor define, except it is not preprocessed as text.
When using an alias the referenced expression or typename is expanded inline.
Arguments of inline expressions may be cached, and evaluated only once.

**Example**: type aliasing

```
inline double = float64;
inline float = float32;
inline byte = uint8;
```

**Example**: constant

```
inline pi = 3.14159265359;
inline true = 0 == 0;
inline false = 0 != 0;
```

**Example**: macro

```
inline min(int a, int b) = a < b ? a : b;
inline max(float32 a, float32 b) = a > b ? a : b;
```

- the right hand side can also use local variables if static is not used

- static should force the right side not to use local variables

- const should force the right side to be constant

## Operator overloading

Operators can be overloaded using the `inline` keyword.

### Type construction/conversion operator

**Example**

```
inline complex(float64 re) = { re: re };
complex a = complex(9);
```

**Example**

```
struct Celsius { double degrees; }
struct Fahrenheit { double degrees; }

// explicit initialization
inline Celsius(float64 value) = { degrees: value };
inline Fahrenheit(float64 value) = { degrees: value };

// explicit conversion
inline Celsius(Fahrenheit value) = Celsius((value.degrees - 32) / 1.8);
inline Fahrenheit(Celsius value) = Fahrenheit(value.degrees * 1.8 + 32);

// implicit conversion
inline (float64 value) = Celsius(value);
inline (float64 value) = Fahrenheit(value);
inline (Fahrenheit value) = Celsius(value);
inline (Celsius value) = Fahrenheit(value);

Celsius boilC = Celsius(100.);                 // => inline Celsius(float64 value)
Fahrenheit boilF = Fahrenheit(boilC);          // => inline Fahrenheit(Celsius value)

Celsius boilImplicitC = 100.;                  // => inline (float64 value) = Celsius(value)
Fahrenheit boilImplicitF = boilC;              // => inline (Celsius value) = Fahrenheit(value)
```

### Unary and binary operators

**Example**

```
inline -(Complex a) = Complex(-a.re, -a.im);
inline +(Complex a, Complex b) = Complex(a.re + b.re, a.im + b.im);

Complex a = Complex(3);                 //  3 + 0 * i
Complex b = -a;                         // -3 + 0 * i
Complex c = a + a;                      //  6 + 0 * i
```

### Property/Extension operators

**Example**

```
Complex a = Complex(3);                 // 3 + 0 * i
Complex b = Complex(5, 1);              // 5 + 1 * i

inline abs(Complex a) = Math.hypot(a.re, a.im);
double x = a.abs();

inline add(Complex a, Complex b) = Complex(a.re + b.re, a.im + b.im);
Complex sum = a.add(b);

inline [](Complex c, int idx] = idx == 0 ? c.re : idx == 1 ? c.im : Math.Nan;
inline [](Complex c, string idx] = idx == "re" ? c.re : idx == "im" ? c.im : Math.Nan;
inline ()(Complex c, int idx) = c[i];

float64 re = a[0];        // => inline [](Complex c, int idx)
float64 im = a["im"];     // => inline [](Complex c, string idx)
float64 re2 = a(0);       // => inline ()(Complex c, int idx)
```

# Type system
Every declared type is also a static variable referencing its metadata,
the internal representation of the symbol used by the compiler which is exposed to the run-time.

## Builtin types
The most fundamental types the virtual machine can work with, are exposed to the compiler.

### void
The type void is an empty type with no values.
No variables of this type can be instantiated.
Mostly used when function does not return a value.
It can also be used as a function to void(finalize) a variable.

### bool
May take on the values true and false only.
true and false are defined constants in the language.

### char
1 byte, ASCII character.

### int8
1 byte, signed (two's complement).
Covers values from -128 to 127.

### int16
2 bytes, signed (two's complement),
Covers values from -32,768 to 32,767.

### int32
4 bytes, signed (two's complement).
Covers values from -2,147,483,648 to 2,147,483,647.

### int64
8 bytes signed (two's complement).
Covers values from -9,223,372,036,854,775,808 to +9,223,372,036,854,775,807.

### uint8
1 byte, unsigned (two's complement).
Covers values from 0 to 255.

### uint16
2 bytes, unsigned (two's complement).
Covers values from 0 to 65,535.

### uint32
4 bytes, unsigned (two's complement).
Covers values from 0 to 4,294,967,295.

### uint64
8 bytes, unsigned (two's complement).
Covers values from 0 to 18,446,744,073,709,551,615.

### float32
4 bytes, IEEE 754.
Covers a range from 1.40129846432481707e-45 to 3.40282346638528860e+38
(positive or negative).

### float64
8 bytes IEEE 754.
Covers a range from 4.94065645841246544e-324 to 1.79769313486231570e+308
(positive or negative).

### pointer
`pointer` is a data type whose value refers directly to (or "points to") another value.
It contains utility functions for low level memory operations (memset, memcpy, ...).

Using it as a function with an identifier argument, it will return the address of the variable.
This might be useful to compare if variable references point to the same value or object.

### variant
`variant` is a dynamic type, which carries the type of the value, and a pointer to the value.

it may be defined as:
```
const struct variant {
	typename type;
	pointer value;
}
```

### typename
`typename` is the compilers internal symbol representation structure.
It contains static utility functions for reflection.

Using it as a function with an identifier argument, it will return the type of the variable:
- if the identifier is not defined, it will return `null`
- if the identifier is a type, it will return `typename`
- if the identifier is a variable, it will return its type
- if the identifier is a variant variable, it will extract the type

**Example**
```
// if integer is not defined, define it as int32
static if (typename(integer) == null) {
inline integer = int32;
}
```

**Example**
```
// check if int128 is defined and is a type.
bool hasInt128 = typename(int128) == typename;
```

### function
`function` is the base type of all functions.

### object
[TODO: implementation]

`object` is the base type for all heap allocated reference types.

Every type inherited from object will be reference counted,
and destroyed when there are no more references pointing to it.

## Builtin aliases
Builtin type aliases:
- `int`: alias for `int32` or `int64` depending on the word size of the vm

Builtin constants:
- `null`: pointer value, representing missing data.

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

