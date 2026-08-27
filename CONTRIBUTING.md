# Contributing to Kratos Lang

Kratos Lang is an early-stage experimental project. Contributions that make
the current behavior clearer, better tested, or easier to build are especially
useful while the compiler architecture is being established.

## Before You Start

Read the [README](README.md) and the relevant page in `docs/`. Check whether a
feature is marked `Implemented`, `Partial`, or `Planned` before changing its
behavior. Keep the implementation and documentation status aligned.

## Development Workflow

Build the current prototype:

```sh
make
```

Run the lexer demonstration:

```sh
./kratosc
```

Clean generated output:

```sh
make clean
```

When adding behavior, include focused tests as soon as the test harness exists.
At minimum, manually verify the affected token or syntax and run the complete
build before submitting a change.

## Coding Guidelines

- Use C11-compatible code and compile with warnings enabled.
- Keep changes focused on one behavior or subsystem.
- Preserve the existing public APIs unless a design change requires otherwise.
- Prefer clear control flow and explicit ownership of source positions.
- Report malformed input instead of silently accepting it when diagnostics are
	available.
- Update the relevant English documentation when syntax or semantics change.
- Do not commit generated binaries or build output.

## Commits and Pull Requests

Use a short imperative commit subject, for example `Add lexer comment tokens`.
Keep unrelated changes in separate commits. A pull request should explain the
user-visible behavior, list validation commands, and call out known limitations.

## Design Changes

Language syntax and semantics are still being defined. For a change that affects
the grammar, type system, runtime model, or keyword meanings, document the
proposal first and show how it fits the roadmap. Avoid making the lexer imply
support for syntax that the parser cannot yet consume.
