# Types and Literals

Kratos declares the type before the variable name. The primitive type keywords
and their literal forms are listed below.

| Type keyword | Literal examples | Token status |
| --- | --- | --- |
| `k_int` | `10`, `0` | Implemented |
| `k_float` | `3.14`, `0.5` | Implemented |
| `k_bool` | `true`, `false` | Implemented |
| `k_char` | `'K'` | Partial |
| `k_string` | `"Kratos"` | Partial |

## Integer and Float Literals

An integer is a sequence of decimal digits. A float is an integer part, a dot,
and at least one decimal digit:

```text
10      INTEGER
3.14    FLOAT
```

Forms such as `.5`, `10.`, signs, exponents, and digit separators are
**Planned**. A dot not followed by a digit terminates the integer and is handled
by the current unsupported-input path.

## Boolean Literals

The lexer recognizes both boolean values:

```text
true    TRUE
false   FALSE
```

Type checking that restricts these values to `k_bool` is **Planned**.

## Character Literals

The current lexer scans a quote, one source character, and an optional closing
quote as `CHAR_LITERAL`:

```text
'K'     CHAR
```

Escape sequences, Unicode characters, malformed literals, and exactly-one-code-
point validation are **Planned**.

## String Literals

The current lexer scans text between double quotes as `STRING_LITERAL`:

```text
"Kratos"    STRING
```

Escapes, interpolation, unterminated-string diagnostics, and a formal encoding
policy are **Planned**. The current scanner also permits newlines inside a
string while updating the line counter.

## Constants

`k_const` is reserved by the lexer. Constant declaration syntax and immutability
checks are **Planned**.
