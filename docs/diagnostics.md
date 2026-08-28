# Diagnostics

Kratos 0.2.0 reports problems in a rustc-style form:

```text
error[K302]: undeclared variable 'Numero'
  --> main.kratos:12:5
   |
12 | shout(Numero);
   |       ^^^^^^
   |
   = help: declare Numero before using it
```

`kratosc --check` prints `Kratos-Lang: no errors found` when analysis succeeds.

Integer arithmetic overflow is not checked in 0.2.0 and has no K3xx code. It
is outside the defined runtime behavior for this release; a future language
version must choose a checked-overflow policy before promising portable results.

## Codes

### Lexer (K1xx)

| Code | Meaning |
|------|---------|
| K101 | Unterminated `$ ... $` comment |
| K102 | Unterminated string literal |
| K103 | Invalid or unterminated character literal |
| K104 | Invalid escape sequence |
| K105 | Unexpected character (including a lone `!`, `&`, or `\|`) |

### Parser (K2xx)

| Code | Meaning |
|------|---------|
| K201 | Invalid token |
| K202 | Unexpected token |
| K203 | Expected a type |
| K204 | Expected an expression |

### Semantic (K3xx)

| Code | Meaning |
|------|---------|
| K301 | Name already declared in this scope |
| K302 | Undeclared name |
| K303 | A `craft` is not a value |
| K304 | Type mismatch |
| K305 | Condition is not `k_bool` |
| K306 | `snap` / `push` outside a loop |
| K307 | Invalid `yield` |
| K308 | `wield` error (missing file, cycle, leftover node) |
| K309 | Assignment to `k_const` |
| K310 | Non-void `craft` does not `yield` on every path |
| K311 | `k_void` used as a variable or parameter |
| K312 | Invalid construct at this level |
| K313 | Nested arrays |
| K314 | Invalid call |
| K315 | Record or field error (unknown field, duplicate field, missing field, type mismatch, self-referential record without array) |
