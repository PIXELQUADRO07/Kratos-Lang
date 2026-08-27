# Language Overview

Kratos Lang is an experimental, statically typed language centered on explicit
declarations and action-oriented vocabulary. The current repository contains a
lexer prototype, not a complete compiler.

## Status Labels

- **Implemented**: recognized by the current lexer and represented by a token.
- **Partial**: a token or design exists, but validation or later compiler stages
	are missing.
- **Planned**: part of the intended language, but not implemented in the
	repository.

## Design Direction

The intended language should favor readable source, explicit types, simple
control flow, and a small runtime model. Keywords are lowercase. User-defined
names use identifiers beginning with a letter or underscore and may contain
letters, digits, and underscores.

## Current Implementation

The lexer currently implements:

- whitespace skipping and line tracking;
- identifiers and the declared keyword set;
- `true` and `false` boolean literals;
- integer and decimal literals;
- character and string literal scanning;
- `=` and `;` punctuation;
- EOF and unknown-character tokens.

The parser, AST, semantic analyzer, code generator, runtime, and automated tests
are planned but currently empty. The token enum reserves names for future
operators and delimiters; those names are not lexer support yet.

## Development Stages

1. Make lexical rules complete and diagnostics precise.
2. Define an AST and parse declarations and expressions.
3. Add name resolution and static type checking.
4. Implement control flow, functions, and actions.
5. Select a code generation target and define runtime behavior.
6. Stabilize the grammar with conformance tests and versioned specifications.
