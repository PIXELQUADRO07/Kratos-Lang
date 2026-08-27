# Control Flow

Control-flow keywords are reserved by the lexer, but parsing and execution are
not implemented.

## Intended Conditional Form

The planned syntax is:

```kratos
if condition {
	craft Primary
} elif other_condition {
	craft Secondary
} else {
	craft Fallback
}
```

The keywords `if`, `elif`, and `else` are currently emitted as `IF`, `ELIF`, and
`ELSE`. Braces, condition parsing, block scope, and branch execution are
**Planned**.

## Action Keywords

`hold`, `press`, `drive`, `sweep`, `snap`, and `push` are recognized as
reserved keywords. Their syntax (below) is decided; parsing and execution are
still **Planned**.

| Keyword | Role | Syntax |
| --- | --- | --- |
| `hold` | `while` loop | `hold (condition) { ... }` |
| `press` | `do-while` loop | `press { ... } hold (condition);` |
| `drive` | `for` loop | `drive (init; condition; step) { ... }` |
| `sweep` | `foreach` loop | `sweep (type identifier in collection) { ... }` |
| `snap` | `break` | `snap;` |
| `push` | `continue` | `push;` |

### `hold` (while)

```kratos
hold (Numero < 10) {
	Numero = Numero + 1;
}
```

The condition is evaluated before each iteration; the body runs only while it
is `true`.

### `press` (do-while)

```kratos
press {
	Numero = Numero + 1;
} hold (Numero < 10);
```

The body runs once unconditionally, then repeats while the trailing `hold
(condition)` is `true`. The `hold (condition);` clause is required and ends
with a semicolon, distinguishing it from a standalone `hold` loop.

### `drive` (for)

```kratos
drive (k_int i = 0; i < 10; i = i + 1) {
	shout(i);
}
```

Three semicolon-separated clauses: an initializer (typically a variable
declaration scoped to the loop), a `k_bool` condition, and a step expression
run after each iteration.

### `sweep` (foreach)

```kratos
sweep (k_int x in lista) {
	shout(x);
}
```

`identifier` is declared fresh, scoped to the loop body, and takes each
element of `collection` in turn; its declared type must match the element
type of the collection. Collections themselves (arrays/lists) are not yet
implemented, so `sweep` cannot run end-to-end until a collection type exists
— the syntax above is reserved ahead of that work.

### `snap` (break) and `push` (continue)

Both are bare statements, valid only inside the nearest enclosing `hold`,
`press`, `drive`, or `sweep`. `snap;` exits that loop immediately; `push;`
skips to the next iteration (running `drive`'s step clause first, if
applicable).

## Required Semantic Rules

Before control flow can be implemented, the project needs to define:

- whether conditions must have type `k_bool` (leaning yes, for all of `if`,
	`hold`, `press`, and `drive`);
- whether blocks create lexical scopes (leaning yes, including `drive`'s
	initializer and `sweep`'s bound identifier);
- whether `elif` and `else` are restricted to an immediately preceding `if`;
- how unreachable branches and missing conditions are diagnosed;
- how `snap`/`push` outside any loop are diagnosed (should be a compile-time
	error, not a runtime one);
- the element type and syntax for collections, needed before `sweep` can be
	implemented.
