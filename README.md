# Kratos Lang

Kratos Lang is an experimental programming language and compiler project.
The language is designed around explicit declarations and a small vocabulary
of action-oriented keywords such as `craft`, `yield`, `hold`, and `press`.

## Project Status

The compiler pipeline is:

```text
source -> lexer -> parser/AST -> semantic analysis -> interpreter (or --emit-c)
```

Lexer, parser, AST, type checking, a tree-walk interpreter, and a C backend
are implemented. Implemented data features include nested arrays, indexed
assignment, `len`, `slice`, scalar conversions (`to_string`, `to_int`,
`to_float`), string concatenation, and **`record` composite types** with value
semantics (member access, member mutation, nested records, arrays of records).
Automated tests cover the lexer, parser, semantic rules, runtime edge cases,
and end-to-end example programs.

## Quick Start

Requirements:

- A C11 compiler, such as GCC or Clang
- `make` (optional; the commands below also work directly)

Build and run the hello example:

```sh
make
./kratosc examples/hello.kratos
```

CLI:

```text
kratosc [--tokens | --ast | --check | --emit-c] [-o <path>] [file.kratos]
kratosc --version
```

With no flags, `kratosc` parses the file, type-checks it, and runs it. If a
`k_void craft main()` exists, it is called after global initializers.

```sh
./kratosc --tokens examples/hello.kratos
./kratosc --ast examples/hello.kratos
./kratosc --emit-c examples/hello.kratos
./kratosc --check examples/hello.kratos
./kratosc --version
```

Without a file, source is read from stdin.

## Language version 0.3.0

The grammar and keyword set documented in [the specification](docs/specification.md)
are implemented as of 0.3.0. Breaking language changes require a version bump.

To build without Make:

```sh
cc -std=c11 -Wall -Wextra -pedantic -Isrc \
	src/main.c src/lexer/lexer.c src/parser/parser.c src/ast/ast.c \
	src/diag/diag.c src/utils/file.c src/semantic/semantic.c src/runtime/interp.c \
	src/codegen/codegen.c -o kratosc
./kratosc examples/hello.kratos
```

Clean generated binaries with:

```sh
make clean
```

## Current Example

```kratos
k_void craft main() {
	shout("Hello, Kratos");
}
```

## Repository Layout

```text
src/
	main.c              Compiler driver
	lexer/              Tokens and scanner
	parser/             Recursive-descent parser
	ast/                Abstract syntax tree
	semantic/           Name resolution and type checking
	runtime/            Tree-walk interpreter
	codegen/            C backend (`--emit-c`)
	utils/              File helpers
tests/                Unit tests (`make test`)
examples/             Sample programs
docs/                 Language notes and specification
```

## Documentation

- [Language overview](docs/language.md)
- [Syntax](docs/syntax.md)
- [Types and literals](docs/types.md)
- [Operators](docs/operators.md)
- [Control flow](docs/control-flow.md)
- [Functions and actions](docs/functions.md)
- [Language specification](docs/specification.md)

## Testing

```sh
make test
```

Run the suite with AddressSanitizer and UndefinedBehaviorSanitizer:

```sh
make test-sanitize
```

CI runs the stricter portable C11 build with:

```sh
make test-strict
```

This builds `kratosc`, runs the unit tests, executes example programs, and
compiles the C emitted from `examples/hello.kratos`.

## Roadmap

Completed in this tree:

1. Lexer punctuation, operators, comments `$ ... $`, escapes, and diagnostics.
2. AST and parser for declarations, expressions, functions, and control flow.
3. Symbol tables, type checking, and source diagnostics.
4. Control flow, functions, arrays, `sweep`, and `wield`.
5. Interpreter and a C code generation target.
6. Unit, integration, and end-to-end tests.
7. Specification and release tracking for **Kratos 0.2.0**.

Possible later work: a bytecode VM, richer collections, and module aliases.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for the current development workflow.
