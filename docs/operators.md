# Operators

Operator tokens are reserved in the token model, but only assignment is
currently recognized by `lexer_next_token()`.

## Implemented

| Spelling | Token | Meaning |
| --- | --- | --- |
| `=` | `ASSIGN` | Assignment syntax token |

The parser and type checker do not yet attach semantics to assignment.

## Planned Operators

The following token names are declared for future lexer and parser work:

| Token names | Intended spelling |
| --- | --- |
| `PLUS`, `MINUS`, `STAR`, `SLASH`, `PERCENT` | `+`, `-`, `*`, `/`, `%` |
| `EQUAL`, `NOT_EQUAL` | `==`, `!=` |
| `LESS`, `GREATER` | `<`, `>` |
| `LESS_EQUAL`, `GREATER_EQUAL` | `<=`, `>=` |
| `AND`, `OR` | `&&`, `||` |

Precedence, associativity, short-circuit behavior, numeric promotion, and
assignment mutability rules must be defined before these operators are treated
as part of the language specification.
