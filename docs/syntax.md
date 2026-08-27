# Syntax

This page describes the current lexical syntax and the intended shape of a
Kratos program. Only the lexical rules are implemented at present.

## Source Text

Source is read as a sequence of ASCII characters by the current lexer. Spaces,
tabs, carriage returns, and newlines separate tokens. Newlines increment the
reported source line but are not emitted as tokens.

Comments are **Planned** and currently produce error tokens.

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

The intended declaration form is:

```text
k_int Numero = 10;
```

The lexer recognizes the declaration components as separate tokens. Parsing,
declaration validation, and semicolon rules are **Planned**.

## Recognized Punctuation

| Spelling | Token | Status |
| --- | --- | --- |
| `=` | `ASSIGN` | Implemented |
| `;` | `SEMICOLON` | Implemented |

Additional punctuation declared by the token enum is **Planned** until lexer and
parser support are added.

## Example

```kratos
k_string Name = "Kratos";
craft Test
```

This can be tokenized by the current demonstration, but it cannot yet be parsed
or executed.
