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

