# Functions and Actions

Function syntax is **Planned**. The lexer reserves four function-oriented
keywords:

| Keyword | Token | Intended role |
| --- | --- | --- |
| `craft` | `CRAFT` | Define or declare a callable unit |
| `yield` | `YIELD` | Return a value from a callable unit |
| `shout` | `SHOUT` | Output or reporting action |
| `wield` | `WIELD` | Invoke or control an action |

## Current Demonstration

The current sample contains:

```kratos
craft Test
```

This is tokenized as `CRAFT` followed by `IDENTIFIER`. There is no parser or
callable representation yet.

## Open Design Decisions

The language still needs a precise definition for:

- parameter and return-type syntax;
- declaration and invocation syntax;
- whether functions are first-class values;
- `yield` behavior in `k_void`-like and value-returning functions;
- the distinction between functions and domain actions;
- runtime I/O behavior for `shout` and `wield`.

These decisions should be recorded in the specification before implementation.
