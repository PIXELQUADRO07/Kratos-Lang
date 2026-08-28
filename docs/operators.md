# Operators

Operators are scanned by the lexer, parsed with precedence, type-checked, and
evaluated by the interpreter. Mixed `k_int` and `k_float` arithmetic promotes
to `k_float`. Assigning `k_int` to `k_float` is allowed; the reverse is not.

## Assignment

| Spelling | Token | Meaning |
| --- | --- | --- |
| `=` | `ASSIGN` | Assign to a non-`k_const` identifier |

Indexed assignment (`Lista[0] = 1`) is **Implemented**.

## Arithmetic and Comparison

| Token names | Spelling |
| --- | --- |
| `PLUS`, `MINUS`, `STAR`, `SLASH`, `PERCENT` | `+`, `-`, `*`, `/`, `%` |
| `EQUAL`, `NOT_EQUAL` | `==`, `!=` |
| `LESS`, `GREATER`, `LESS_EQUAL`, `GREATER_EQUAL` | `<`, `>`, `<=`, `>=` |

`%` requires two `k_int` values. Division or modulo by zero is a runtime error.

## Logic

| Token | Spelling |
| --- | --- |
| `AND`, `OR` | `&&`, `\|\|` |
| `NOT` | `not` |

`&&` and `||` require `k_bool` operands and **short-circuit**.

## Precedence (low to high)

`||` → `&&` → `== !=` → `< > <= >=` → `+ -` → `* / %` → unary `not`/`-` →
index `[]` and calls → primary
