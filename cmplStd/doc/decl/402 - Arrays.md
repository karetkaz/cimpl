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

