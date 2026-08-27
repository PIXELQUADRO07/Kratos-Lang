# Control Flow

Control-flow forms are parsed, type-checked, and executed.

Conditions of `if`, `hold`, `press`, and `drive` must be `k_bool`. Blocks
create lexical scopes. `drive`'s initializer and `sweep`'s element are scoped
to the loop. `elif` and `else` bind to the immediately preceding `if`. `snap`
and `push` outside a loop are compile-time errors.

## Conditional Form

```kratos
if (condition) {
	shout("primary");
} elif (other_condition) {
	shout("secondary");
} else {
	shout("fallback");
}
```

Parentheses around the condition are required.

## Action Keywords

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

### `press` (do-while)

```kratos
press {
	Numero = Numero + 1;
} hold (Numero < 10);
```

### `drive` (for)

```kratos
drive (k_int i = 0; i < 10; i = i + 1) {
	shout(i);
}
```

### `sweep` (foreach)

```kratos
k_int[] lista = [1, 2, 3];
sweep (k_int x in lista) {
	shout(x);
}
```

The element is declared fresh. Its type must match the array element type.
Nested arrays are not supported.

### `snap` and `push`

Valid only inside the nearest enclosing `hold`, `press`, `drive`, or `sweep`.
`push` in a `drive` still runs the step clause in the interpreter after the
body, via the loop structure (the step runs at the end of each iteration
unless `snap` exits).
