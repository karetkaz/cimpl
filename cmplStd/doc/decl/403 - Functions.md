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

