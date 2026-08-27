# Syntax

This page describes the lexical syntax and the shape of a Kratos program as of
version 0.1.0.

## Source Text

Source is read as a sequence of ASCII characters. Spaces, tabs, carriage returns,
and newlines separate tokens. Newlines increment the reported source line but are
not emitted as tokens.

Comments use matching dollar signs and may span lines:

```kratos
$ this is a comment $
k_int Answer = 42;
```

An unclosed comment is a lexical error.

## Identifiers

An identifier starts with an ASCII letter or `_`, followed by zero or more ASCII
letters, digits, or underscores:

```text
Numero
_internal_value
stage2
```

Keywords are reserved when their complete spelling is present. For example,
`craftwork` is an identifier, not `CRAFT` followed by `IDENTIFIER`.

## Statements

A declaration always has a type, a name, `=`, an initializer, and `;`:

```text
k_int Numero = 10;
k_const k_int Limit = 100;
k_int[] Lista = [1, 2, 3];
```

Top-level items are declarations, `craft` functions, or `wield "file";`.

## Punctuation and Operators

Implemented token spellings include `=`, `==`, `!=`, `<`, `>`, `<=`, `>=`,
`+`, `-`, `*`, `/`, `%`, `&&`, `||`, `not`, `( ) { } [ ]`, `;`, `,`, and `.`.
A lone `!`, `&`, or `|` is a lexical error; use `not`, `&&`, and `||`.

## Example

```kratos
k_string Name = "Kratos";

k_void craft main() {
	shout(Name);
}
```
