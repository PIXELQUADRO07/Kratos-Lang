# Contributing to Kratos Lang

Kratos Lang is an experimental project. Changes that keep the compiler,
tests, and docs aligned are especially useful.

## Before You Start

Read the [README](README.md) and the relevant page in `docs/`. Check whether a
feature is marked `Implemented`, `Partial`, or `Planned` before changing its
behavior. Keep the implementation and documentation status aligned. The current
language snapshot is **0.1.0**.

## Development Workflow

Build:

```sh
make
```

Run an example:

```sh
./kratosc examples/hello.kratos
```

Run the test suite:

```sh
make test
```

Clean generated output:

```sh
make clean
```

When adding behavior, include focused tests. Run `make test` before submitting
a change.

## Coding Guidelines

- Use C11-compatible code and compile with warnings enabled.
- Keep changes focused on one behavior or subsystem.
- Preserve the existing public APIs unless a design change requires otherwise.
- Prefer clear control flow and explicit ownership of source positions.
- Report malformed input instead of silently accepting it.
- Update the relevant English documentation when syntax or semantics change.
- Do not commit generated binaries or build output.

## Commits and Pull Requests

Use a short imperative commit subject, for example `Add lexer comment tokens`.
Keep unrelated changes in separate commits. A pull request should explain the
user-visible behavior, list validation commands, and call out known limitations.

## Design Changes

For a change that affects the grammar, type system, runtime model, or keyword
meanings, document the proposal in `docs/` and show how it fits version 0.1.0
or a later snapshot.
