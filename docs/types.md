# Types and Literals

Kratos declares the type before the variable name. The primitive type keywords
and their literal forms are listed below.

| Type keyword | Literal examples | Status |
| --- | --- | --- |
| `k_int` | `10`, `0` | Implemented |
| `k_float` | `3.14`, `0.5` | Implemented |
| `k_bool` | `true`, `false` | Implemented |
| `k_char` | `'K'`, `'\n'` | Implemented |
| `k_string` | `"Kratos"`, `"line\n"` | Implemented |
| `k_void` | none — return type only | Implemented |

Arrays use one pair of brackets per dimension: `k_int[] Lista = [1, 2, 3];`
and `k_int[][] Matrix = [[1, 2], [3, 4]];`. `sweep` currently accepts only
one-dimensional arrays.

## Array Semantics

Array length is fixed when the array value is created. There is no append or
push operation for arrays; `push` remains the loop-control statement. Indexed
assignment can replace an existing element, but cannot change the length.

In the interpreter, evaluating an array identifier recursively clones the array
and its elements. Therefore `k_int[] b = a;` creates an independent copy:
mutating `b` does not mutate `a`.

The C backend represents every array as `KArr { void *items; size_t count; }`.
Nested arrays contain `KArr` elements allocated in another `KArr` buffer. A
plain C assignment such as `KArr b = a;` copies the struct but shares its
`items` pointer, so the corresponding assignment aliases the buffer. This is
a known backend semantic difference and is one reason array ownership should
be made explicit before adding more composite types.

Both backends reject negative and out-of-range indexing at runtime. The
interpreter reports an error and the generated C prints an error and exits
before dereferencing the invalid address.

## Design Decision: Dynamic Arrays

For version 0.2.0 arrays remain fixed-size values. A future mutable collection
would need a distinct name from the `push` loop keyword and an ownership rule
for both backends; no such operation is part of the current language.

## Integer and Float Literals

An integer is a sequence of decimal digits. A float is an integer part, a dot,
and at least one decimal digit:

```text
10      INTEGER
3.14    FLOAT
```

Leading and trailing decimal points, signs, exponents, and digit separators
are **Implemented**: `.5`, `10.`, `-2.5`, `1e3`, and `1_000`.

## Boolean Literals

```text
true    TRUE
false   FALSE
```

Conditions of `if`, `hold`, `press`, and `drive` must have type `k_bool`.

## Character and String Literals

Character literals use single quotes. String literals use double quotes. Escape
sequences `\n`, `\t`, `\r`, `\0`, `\\`, `\"`, and `\'` are **Implemented**.
Unterminated literals are lexical errors.

A `k_char` value must be written with single quotes. `"K"` is a `k_string`.

## Constants

`k_const type name = expression;` is **Implemented**. Assignment to that name
is a semantic error.

## `k_void`

`k_void` is only a `craft` return type. Declaring a variable or parameter as
`k_void` is a semantic error.

## Record Types

`record` declares a composite named type containing a fixed set of typed fields:

```
record Point {
    k_int x;
    k_int y;
}
```

A record literal uses the record name followed by brace-initialised fields:

```
Point origin = Point { x: 0, y: 0 };
```

All fields must be provided (in any order), and no field may appear twice.
Type mismatches and unknown fields are caught by the semantic analyser (`K315`).

Member access uses dot notation:

```
shout(origin.x);
origin.x = 42;
```

Records use **value semantics**: assigning a record to a new variable, or passing
it to a `craft`, creates a deep copy. Mutating the copy does not affect the
original.

A `k_const` record variable cannot have its fields reassigned (`K309`).

Records may be nested, and arrays of records are supported:

```
record Line { Point p1; Point p2; }
Point[] pts = [Point { x: 1, y: 2 }, Point { x: 3, y: 4 }];
```

The C code generator emits each record as a C `struct` prefixed with `krec_`.
