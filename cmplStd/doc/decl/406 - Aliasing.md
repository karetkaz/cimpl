## Aliasing

Aliasing is an analogue to c preprocessor define, except it is not preprocessed as text.
When using an alias the referenced expression or typename is expanded inline.
Arguments of inline expressions may be cached, and evaluated only once.

**Example**: type aliasing

```
inline double = float64;
inline float = float32;
inline byte = uint8;
```

**Example**: constant

```
inline pi = 3.14159265359;
inline true = 0 == 0;
inline false = 0 != 0;
```

**Example**: macro

```
inline min(int a, int b) = a < b ? a : b;
inline max(float32 a, float32 b) = a > b ? a : b;
```

- the right hand side can also use local variables if static is not used

- static should force the right side not to use local variables

- const should force the right side to be constant

