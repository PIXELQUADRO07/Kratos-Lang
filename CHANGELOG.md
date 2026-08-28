# Changelog

## 0.3.0 - 2026-08-28

- Add `record` composite types with value semantics.
  - Top-level `record Name { type field; ... }` declaration.
  - Record literals `Name { field: expr, ... }` — all fields required, any order.
  - Member access (`rec.field`) and member assignment (`rec.field = expr`).
  - Nested records (`record Line { Point p1; Point p2; }`) and arrays of records.
  - Craft parameters and return values are deep-copied (value semantics).
  - `k_const` record variables reject field mutation (K309).
  - New diagnostic K315 for unknown, duplicate, missing, or mistyped fields.
  - C backend emits `krec_<Name>` structs; interpreter carries `VAL_RECORD` values.
  - 11 new unit tests and 2 new C-codegen integration tests.

## 0.2.0 - 2026-08-28

- Add indexed array assignment.
- Add nested arrays such as `k_int[][]`.
- Add `len()` for strings and arrays.
- Add `slice(string|array, start, end)` with an exclusive end index.
- Add `to_string`, `to_int`, and `to_float` scalar conversions.
- Add string concatenation with `+`.
- Add extended numeric literals and regression coverage for diagnostics and C output.

## 0.1.0 - 2026-08-28

- Freeze the implemented Kratos language syntax and grammar.
- Add the `kratosc --check`, `--version`, and `-o` CLI paths.
- Add rustc-style diagnostics with K1xx, K2xx, and K3xx codes.
- Add GitHub contribution, security, and CI project files.

Diagnostic codes are documented in [docs/diagnostics.md](docs/diagnostics.md).
