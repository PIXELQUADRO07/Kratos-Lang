# Functions and Actions

Function syntax is **Implemented**.

| Keyword | Token | Role |
| --- | --- | --- |
| `craft` | `CRAFT` | Define a callable unit |
| `yield` | `YIELD` | Return from a callable unit |
| `shout` | `SHOUT` | Write a value to stdout |
| `wield` | `WIELD` | Import another source file |

`len(value)` is a built-in expression returning a `k_int` for strings and
arrays.

`slice(value, start, end)` returns the range `[start, end)` for strings and
one-dimensional arrays.

String concatenation uses `+`, for example `"hello, " + name`.

## Function Declaration

The return type comes first, followed by `craft`, the function name, and a
parenthesized parameter list:

```kratos
k_int craft add(k_int a, k_int b) {
	yield a + b;
}
```

A function with no return value uses `k_void`:

```kratos
k_void craft greet(k_string name) {
	shout(name);
}
```

`k_void` cannot declare a variable. A non-`k_void` craft must `yield` on every
path. A `k_void` craft may fall off the end of its block.

After global initializers, `kratosc` calls `k_void craft main()` if it exists.

## `yield`

`yield expression;` exits the enclosing function. The type must match the
declared return type. `yield;` is allowed only in `k_void` crafts.

## `shout`

`shout(expression);` writes a textual representation followed by a newline.
It is a statement, not a value.

## `wield`

`wield "path";` is valid at top level. The path is resolved relative to the
file that contains the `wield`. Imported top-level declarations are spliced
into the program. Cyclic imports are errors. There is no `as alias` form.

## Invocation

`add(1, 2)`. Functions are not stored in variables. There is no overloading
and no default parameter values.

## Open Design Decisions (not in 0.2.0)

- first-class functions;
- `wield` aliases and package names;
- pluggable `shout` sinks (stdout only).
