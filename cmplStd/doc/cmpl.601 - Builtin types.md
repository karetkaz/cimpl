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

