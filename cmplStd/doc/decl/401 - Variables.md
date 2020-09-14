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

