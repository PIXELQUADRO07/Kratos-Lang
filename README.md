# Kratos Lang

Kratos Lang is an experimental programming language and compiler project.
The language is designed around explicit declarations and a small vocabulary
of action-oriented keywords such as `craft`, `yield`, `hold`, and `press`.

## Project Status

The project is currently an early lexer prototype. The lexer can tokenize
identifiers, keywords, booleans, integers, decimal numbers, character
literals, string literals, assignment, and semicolons. The parser, AST,
semantic analysis, code generation, runtime, and test suite are not implemented
yet.

The token enum already reserves names for additional operators and delimiters,
but reserved names do not mean that those tokens are supported by the lexer.
Unsupported input currently produces `TOKEN_ERROR`.

## Quick Start

Requirements:

- A C11 compiler, such as GCC or Clang
- `make` (optional; the commands below also work directly)

Build and run the lexer demonstration:

```sh
make
./kratosc
```

The demonstration tokenizes the sample program in `src/main.c` and prints one
token per line. To build without Make:

```sh
cc -std=c11 -Wall -Wextra -pedantic -Isrc \
	src/main.c src/lexer/lexer.c -o kratosc
./kratosc
```

Clean the generated binary with:

```sh
make clean
```

## Current Example

```kratos
k_int Numero = 10;
k_float Pi = 3.14;
k_bool Verifica = true;
k_bool AltraVerifica = false;
k_char Lettera = 'K';
k_string Nome = "Kratos";
craft Test
```

The implemented lexer recognizes the declarations above as tokens. It does
not parse or execute them yet.

## Repository Layout

```text
src/
	main.c              Lexer demonstration entry point
	lexer/              Token definitions and lexer implementation
	ast/                Planned abstract syntax tree
	parser/             Planned parser
	semantic/           Planned semantic analysis
	codegen/            Planned code generation
	runtime/            Planned runtime support
	utils/              Planned shared utilities
	tests/              Reserved test directories
docs/                 Language notes and planned specification
examples/             Reserved example programs
```

## Documentation

- [Language overview](docs/language.md)
- [Syntax](docs/syntax.md)
- [Types and literals](docs/types.md)
- [Operators](docs/operators.md)
- [Control flow](docs/control-flow.md)
- [Functions and actions](docs/functions.md)
- [Language specification](docs/specification.md)

The documentation deliberately distinguishes implemented behavior from planned
design. The specification is aspirational until the corresponding compiler
stages exist.

## Testing

There is no automated test suite yet. The current verification is the build and
lexer demonstration:

```sh
make
./kratosc
```

The next testing milestone is a standalone lexer test suite covering token
boundaries, malformed literals, line tracking, and unknown characters.

## Roadmap

1. Complete lexer punctuation, operators, comments, escapes, and diagnostics.
2. Define the AST and implement declarations and expressions in the parser.
3. Add symbol tables, type checking, and useful source diagnostics.
4. Implement control flow, functions, and action semantics.
5. Add a code generation target and a small runtime.
6. Add automated unit, integration, and end-to-end tests.
7. Stabilize the language specification and publish versioned releases.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for the current development workflow.
