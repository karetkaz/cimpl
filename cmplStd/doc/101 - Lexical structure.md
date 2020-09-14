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

