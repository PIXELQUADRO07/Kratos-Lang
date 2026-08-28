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
