# Control Flow

Control-flow keywords are reserved by the lexer, but parsing and execution are
not implemented.

## Intended Conditional Form

The planned syntax is:

```kratos
if condition {
	craft Primary
} elif other_condition {
	craft Secondary
} else {
	craft Fallback
}
```

The keywords `if`, `elif`, and `else` are currently emitted as `IF`, `ELIF`, and
`ELSE`. Braces, condition parsing, block scope, and branch execution are
**Planned**.

## Action Keywords

`hold`, `press`, `drive`, `sweep`, `snap`, and `push` are recognized as reserved
keywords. Their arguments, effects, and runtime interface are not defined yet.

## Required Semantic Rules

Before control flow can be implemented, the project needs to define:

- whether conditions must have type `k_bool`;
- whether blocks create lexical scopes;
- whether `elif` and `else` are restricted to an immediately preceding `if`;
- how unreachable branches and missing conditions are diagnosed;
- how actions interact with generated code and the runtime.
