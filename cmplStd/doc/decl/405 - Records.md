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

