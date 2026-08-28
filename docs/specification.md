# Kratos Lang Specification

Version **0.1.0**. This document describes the language as implemented by
`kratosc` in this repository. Behavior marked **Planned** is not required of
0.1.0 implementations.

## Versioning

0.1.0 is the first numbered language snapshot. This grammar and keyword set
are **frozen**: later versions may extend the language, but breaking changes
must bump the version (minor or major once 1.0.0 exists).

Implemented behavior is defined by the compiler sources, the tests in
`tests/` and `examples/`, and this specification.

## Language freeze 0.1.0

The following spellings are part of 0.1.0 and must not change without a
version bump:

- types: `k_int`, `k_float`, `k_bool`, `k_char`, `k_string`, `k_const`, `k_void`;
- control: `if`, `elif`, `else`;
- loops: `hold`, `press`, `drive`, `sweep`, `snap`, `push`, and `in`;
- actions: `craft`, `yield`, `shout`, `wield`;
- literals and logic: `true`, `false`, `not` (a lone `!` is a lexical error);
- comments: matching `$ ... $` (not `//`).

## Lexical Model

The lexer consumes source from left to right and returns tokens containing a
type, a pointer into the original source, a length, and a line number.

Whitespace is skipped. `$ ... $` comments are skipped. Identifiers are ASCII
alphanumeric sequences beginning with a letter or underscore. Keywords are
recognized by exact spelling.

Unterminated comments, strings, and character literals produce `TOKEN_ERROR`.
String and character escapes `\n \t \r \0 \\ \" \'` are decoded when building
the AST.

## Implemented Token Families

- identifiers and the keyword set listed in [language.md](language.md);
- `INTEGER`, `FLOAT`, `TRUE`, `FALSE`; numeric literals support leading or
  trailing decimal points, exponents, and `_` digit separators;
- `CHAR_LITERAL` and `STRING_LITERAL`;
- operators and delimiters listed in [operators.md](operators.md) and
  [syntax.md](syntax.md);
- `ASSIGN`, `SEMICOLON`, `EOF`, and `ERROR`.

## Grammar

```ebnf
program       = { declaration | function | wield_stmt } ;

declaration   = [ "k_const" ] type [ "[" "]" ] identifier "=" expression ";" ;
type          = "k_int" | "k_float" | "k_bool" | "k_char" | "k_string" | "k_void" ;

function      = return_type "craft" identifier "(" [ params ] ")" block ;
return_type   = type ;
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
              | wield_stmt
              | expr_stmt
              | block ;

wield_stmt    = "wield" string ";" ;

assignment    = ( identifier | identifier "[" expression "]" ) "=" expression ";" ;
expr_stmt     = expression ";" ;

if_stmt       = "if" "(" expression ")" block
                { "elif" "(" expression ")" block }
                [ "else" block ] ;

hold_stmt     = "hold" "(" expression ")" block ;
press_stmt    = "press" block "hold" "(" expression ")" ";" ;
drive_stmt    = "drive" "(" declaration expression ";" assignment_or_expr ")" block ;
sweep_stmt    = "sweep" "(" type identifier "in" identifier ")" block ;

expression    = or_expr ;
or_expr       = and_expr { "||" and_expr } ;
and_expr      = equality { "&&" equality } ;
equality      = comparison { ("==" | "!=") comparison } ;
comparison    = additive { ("<" | ">" | "<=" | ">=") additive } ;
additive      = multiplicative { ("+" | "-") multiplicative } ;
multiplicative= unary { ("*" | "/" | "%") unary } ;
unary         = ( "not" | "-" ) unary | postfix ;
postfix       = primary { "[" expression "]" } ;
primary       = literal | call | identifier | array_literal | "(" expression ")" ;
call          = identifier "(" [ expression { "," expression } ] ")" ;
array_literal = "[" [ expression { "," expression } ] "]" ;
literal       = integer | float | boolean | character | string ;
boolean       = "true" | "false" ;
```

## Compiler Pipeline

```text
source -> lexer -> parser/AST -> semantic analysis -> interpreter
                                              \-> --emit-c
```

Diagnostics use rustc-style reports (`error[Knnn]`) with file, line, column,
a source snippet, and an optional help note. Codes are listed in
[diagnostics.md](diagnostics.md). Parse errors set a flag and continue with
minimal panic recovery; the AST is not executed if that flag is set.

`kratosc --check` runs the lexer, parser, and semantic analyzer and then
stops (it does not interpret or emit C).

## Semantic Rules

- Blocks, `drive` initializers, and `sweep` elements introduce scopes.
- Duplicate names in one scope are errors.
- `k_void` is only a function return type.
- Conditions are `k_bool`.
- `snap` / `push` require an enclosing loop.
- Non-void crafts must `yield` on every path (`if` needs an `else` for this).
- Array literals must have a uniform element type matching the declaration.
- `wield` loads another file; cycles are errors.

## Runtime

`kratosc` evaluates global declarations in order, then calls `main` if it is
declared. `shout` writes to stdout with a trailing newline. `&&` / `||`
short-circuit.

## Conformance Requirements

An implementation of 0.1.0 should match this grammar, the semantic rules
above, and the tests invoked by `make test`.
