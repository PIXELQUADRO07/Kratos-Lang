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
| `k_void` | none — return type only | Planned |

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

A `k_char` value must be written with single quotes. Double-quoted text (even
one character long, e.g. `"K"`) lexes as `STRING_LITERAL`, not `CHAR_LITERAL`;
assigning it to a `k_char` is a type mismatch to be caught during semantic
analysis, not a lexer error.

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

## `k_void`

`k_void` is reserved exclusively as the return type of a `craft` that produces
no value (see [functions.md](functions.md)). It cannot be used to declare a
variable; doing so is a semantic error once declarations are validated.
