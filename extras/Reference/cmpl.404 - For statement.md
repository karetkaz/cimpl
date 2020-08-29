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

