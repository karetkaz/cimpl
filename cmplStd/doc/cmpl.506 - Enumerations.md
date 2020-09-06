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

