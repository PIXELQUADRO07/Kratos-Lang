# Functions and Actions

Function syntax is **Partial**: the shape below is decided, but no parser
implements it yet. The lexer reserves four function-oriented keywords:

| Keyword | Token | Intended role |
| --- | --- | --- |
| `craft` | `CRAFT` | Define or declare a callable unit |
| `yield` | `YIELD` | Return a value from a callable unit |
| `shout` | `SHOUT` | Output or reporting action |
| `wield` | `WIELD` | Invoke or control an action |

## Function Declaration Syntax (Planned)

The return type comes first, C-style, followed by `craft`, the function name,
and a parenthesized, comma-separated parameter list. Each parameter has an
explicit type, exactly like a variable declaration without the initializer:

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

`k_void` is a new type, reserved for this purpose only: it cannot be used to
declare a variable.

## `yield`

`yield expression;` exits the enclosing function and produces `expression` as
its result. The expression's type must match the function's declared return
type. In a `k_void craft`, `yield;` (no expression) exits early; a `k_void`
function may also fall off the end of its block without an explicit `yield`.

## `shout`

`shout(expression);` is a built-in output action, not a user-declared
function. It accepts one expression of any implemented type and writes its
textual representation. It does not return a value and cannot be assigned.

## `wield`

`wield "path";` brings the declarations of another Kratos source file into
scope, addressed by a string literal path. Import resolution, visibility, and
whether `wield` can be scoped (e.g. `wield "path" as alias;`) are still
**Planned** and not yet decided.

## Invocation

Calling a function uses the plain identifier syntax already reserved by the
lexer for parentheses: `add(1, 2)`. There is no separate keyword for calling
an existing `craft`.

## Current Demonstration

The current sample contains:

```kratos
craft Test
```

This predates the syntax above and will need to be updated once the parser
implements function declarations; a bare `craft Test` with no return type,
parameter list, or body is no longer valid under this design.

## Open Design Decisions

The language still needs a precise definition for:

- whether functions are first-class values (can be stored in variables,
	passed as arguments);
- `wield` import resolution: file paths vs. module names, aliasing, partial
	imports;
- whether `craft` supports default parameter values or overloading (leaning
	no, to keep the grammar simple, but not yet decided);
- runtime representation of `shout` output (stdout only, or a pluggable
	sink).

These decisions should be recorded in the specification before implementation.
