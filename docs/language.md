# Language Overview

Kratos Lang is an experimental, statically typed language centered on explicit
declarations and action-oriented vocabulary. Version **0.1.0** ships a lexer,
parser, type checker, interpreter, and C emitter.

## Status Labels

- **Implemented**: recognized and handled by the current compiler stages.
- **Partial**: parsed or typed, but with known limits (see the page for the feature).
- **Planned**: not part of 0.1.0.

## Design Direction

The language favors readable source, explicit types, simple control flow, and a
small runtime model. Keywords are lowercase. User-defined names use identifiers
beginning with a letter or underscore and may contain letters, digits, and
underscores.

Functions are not first-class values. There is no overloading. Source is ASCII.

## Current Implementation

- lexer: whitespace, `$ ... $` comments, keywords, literals, operators, delimiters;
- parser and AST for the grammar in [specification.md](specification.md);
- semantic analysis: scopes, `k_const`, types, `snap`/`push` in loops, `yield`;
- interpreter: globals, `craft` calls, control flow, arrays, `shout`;
- `wield "path";` at top level, resolved relative to the current file;
- `kratosc --emit-c` writes a C translation.

## Language freeze 0.1.0

Version 0.1.0 freezes the implemented grammar in
[specification.md](specification.md). Keywords, `$ ... $` comments, and
operator spellings will not change without a version bump. Editor tooling
and later compiler stages should treat that document as the source of truth.

## Development Stages

1. Lexical rules and diagnostics. **Done.**
2. AST and parser. **Done.**
3. Name resolution and static type checking. **Done.**
4. Control flow, functions, and actions. **Done.**
5. Interpreter and C backend. **Done.**
6. Tests and a versioned specification. **Done (0.1.0).**
