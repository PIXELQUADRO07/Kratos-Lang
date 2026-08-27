# Kratos Lang Specification

This document is a working specification for an experimental language. It is
not yet a conformance standard: the repository currently implements lexical
scanning only.

## Versioning

Until a first language release exists, syntax and semantics described as
**Planned** may change. Implemented behavior is defined by the lexer sources and
the demonstration program in `src/main.c`.

## Lexical Model

The lexer consumes source from left to right and returns tokens containing:

- a token type;
- a pointer into the original source;
- the token length;
- the current source line.

Whitespace is skipped. Identifiers are ASCII alphanumeric sequences beginning
with a letter or underscore. Keywords are recognized by exact spelling.

## Implemented Token Families

- identifiers and the keyword set listed in [language.md](language.md);
- `INTEGER`, `FLOAT`, `TRUE`, and `FALSE`;
- `CHAR_LITERAL` and `STRING_LITERAL` scanning;
- `ASSIGN`, `SEMICOLON`, `EOF`, and `ERROR`.

## Grammar Draft

The following is an indicative draft, not a parser contract. It now includes
the decided (but unimplemented) shape of functions and control flow; anything
marked **Planned** elsewhere in this document is still subject to change.

```ebnf
program       = { declaration | function } ;

declaration   = [ "k_const" ] type identifier "=" expression ";" ;
type          = "k_int" | "k_float" | "k_bool" | "k_char" | "k_string" ;

function      = return_type "craft" identifier "(" [ params ] ")" block ;
return_type   = type | "k_void" ;
params        = param { "," param } ;
param         = type identifier ;

block         = "{" { statement } "}" ;
statement     = declaration
              | assignment
              | if_stmt
              | hold_stmt
              | press_stmt
              | drive_stmt
              | sweep_stmt
              | "snap" ";"
              | "push" ";"
              | "yield" [ expression ] ";"
              | "shout" "(" expression ")" ";"
              | "wield" string ";"
              | expr_stmt ;

assignment    = identifier "=" expression ";" ;
expr_stmt     = expression ";" ;

if_stmt       = "if" "(" expression ")" block
                { "elif" "(" expression ")" block }
                [ "else" block ] ;

hold_stmt     = "hold" "(" expression ")" block ;
press_stmt    = "press" block "hold" "(" expression ")" ";" ;
drive_stmt    = "drive" "(" declaration expression ";" expression ")" block ;
sweep_stmt    = "sweep" "(" type identifier "in" identifier ")" block ;

expression    = literal | identifier | expression operator expression ;
literal       = integer | float | boolean | character | string ;
boolean       = "true" | "false" ;
```

Operator precedence, blocks-as-scopes, comments, escapes, and error recovery
remain **Planned**. `sweep`'s grammar is reserved but not runnable until a
collection type is designed (see [control-flow.md](control-flow.md)).

## Compiler Pipeline

The intended pipeline is:

```text
source -> lexer -> parser/AST -> semantic analysis -> code generation -> runtime
```

Only the lexer stage currently has implementation. The other directories are
reserved for the stages above and need both code and tests.

## Conformance Requirements

A future implementation should provide deterministic diagnostics with source
locations, reject malformed literals and invalid declarations, and include lexer,
parser, semantic, integration, and runtime tests. A language version will only
be considered stable once those behaviors are specified and tested.
