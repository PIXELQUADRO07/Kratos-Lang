# Kratos Lang Specification

Version **0.3.0**. This document describes the language as implemented by
`kratosc` in this repository. Behavior marked **Planned** is not required of
0.3.0 implementations.

## Records

Records are a composite named type implemented since 0.3.0.

```kratos
record Point {
    k_int x;
    k_int y;
}

Point origin = Point { x: 0, y: 0 };
k_void craft main() {
    origin.x = 3;
    shout(origin.y);
}
```

The rules are:

- A record declaration is **top-level** and introduces a named type.
- Each field has one primitive or previously declared record type. Array fields
  use the existing `[]` suffix.
- A record literal must provide **every** field exactly once, using `field:
  expression` entries. Field order inside the literal is arbitrary.
- Field access uses `value.field`; field assignment follows the mutability of the
  containing variable (`k_const` variables cannot have fields reassigned).
- Records have **value semantics**: assignment, craft parameters, and yields
  deep-copy the complete value.
- Field names are scoped to their record type.
- Duplicate fields in one record, unknown fields in a literal, missing fields in
  a literal, and type mismatches are semantic errors — all reported as **K315**.
- Direct self-reference (`record A { A self; }`) is rejected; self-referential
  array fields (`A[]`) are allowed.
- Nested records (`record Line { Point p1; Point p2; }`) and arrays of records
  (`Point[] pts = [...];`) are supported.
- The C code generator emits `typedef struct krec_Name krec_Name; struct krec_Name { ... };`.

## Versioning

0.2.0 is the current numbered language snapshot. This grammar and keyword set
are **frozen**: later versions may extend the language, but breaking changes
must bump the version (minor or major once 1.0.0 exists).

Implemented behavior is defined by the compiler sources, the tests in
`tests/` and `examples/`, and this specification.

## Language version 0.3.0

The following spellings are part of 0.3.0 and must not change without a
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

declaration   = [ "k_const" ] type { "[" "]" } identifier "=" expression ";" ;
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
- Array literals must have a uniform element type and depth matching the declaration.
- Array types may be nested, for example `k_int[][]`; nested array literals must
  have matching depth and uniform element types at each level.
- `len(value)` accepts strings and arrays and returns `k_int`.
- `slice(value, start, end)` accepts a string or one-dimensional array and
  returns the half-open range `[start, end)`. Bounds must satisfy
  `0 <= start <= end <= len(value)`; invalid bounds are runtime errors.
- `to_string`, `to_int`, and `to_float` are scalar conversion built-ins. Invalid
  string conversions are runtime errors.
- `wield` loads another file; cycles are errors.

## Runtime

`kratosc` evaluates global declarations in order, then calls `main` if it is
declared. `shout` writes to stdout with a trailing newline. `&&` / `||`
short-circuit. Array indexing validates negative and out-of-range indices in
the interpreter and in the C backend before dereferencing.

## Conformance Requirements

An implementation of 0.3.0 should match this grammar, the semantic rules
above, and the tests invoked by `make test`.
