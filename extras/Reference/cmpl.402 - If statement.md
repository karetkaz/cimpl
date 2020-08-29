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

