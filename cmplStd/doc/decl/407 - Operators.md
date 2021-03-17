## Operator overloading

Operators can be overloaded using the `inline` keyword.

### Type construction/conversion operator

**Example**

```
inline complex(float64 re) = { re: re };
complex a = complex(9);
```

**Example**

```
struct Celsius { double degrees; }
struct Fahrenheit { double degrees; }

// explicit initialization
inline Celsius(float64 value) = { degrees: value };
inline Fahrenheit(float64 value) = { degrees: value };

// explicit conversion
inline Celsius(Fahrenheit value) = Celsius((value.degrees - 32) / 1.8);
inline Fahrenheit(Celsius value) = Fahrenheit(value.degrees * 1.8 + 32);

// implicit conversion
inline (float64 value) = Celsius(value);
inline (float64 value) = Fahrenheit(value);
inline (Fahrenheit value) = Celsius(value);
inline (Celsius value) = Fahrenheit(value);

Celsius boilC = Celsius(100.);                 // => inline Celsius(float64 value)
Fahrenheit boilF = Fahrenheit(boilC);          // => inline Fahrenheit(Celsius value)

Celsius boilImplicitC = 100.;                  // => inline (float64 value) = Celsius(value)
Fahrenheit boilImplicitF = boilC;              // => inline (Celsius value) = Fahrenheit(value)
```

### Unary and binary operators

**Example**

```
inline -(Complex a) = Complex(-a.re, -a.im);
inline +(Complex a, Complex b) = Complex(a.re + b.re, a.im + b.im);

Complex a = Complex(3);                 //  3 + 0 * i
Complex b = -a;                         // -3 + 0 * i
Complex c = a + a;                      //  6 + 0 * i
```

### Property/Extension operators

**Example**

```
Complex a = Complex(3);                 // 3 + 0 * i
Complex b = Complex(5, 1);              // 5 + 1 * i

inline abs(Complex a) = Math.hypot(a.re, a.im);
double x = a.abs();

inline add(Complex a, Complex b) = Complex(a.re + b.re, a.im + b.im);
Complex sum = a.add(b);

inline [](Complex c, int idx] = idx == 0 ? c.re : idx == 1 ? c.im : Math.Nan;
inline [](Complex c, string idx] = idx == "re" ? c.re : idx == "im" ? c.im : Math.Nan;
inline ()(Complex c, int idx) = c[i];

float64 re = a[0];        // => inline [](Complex c, int idx)
float64 im = a["im"];     // => inline [](Complex c, string idx)
float64 re2 = a(0);       // => inline ()(Complex c, int idx)
```

