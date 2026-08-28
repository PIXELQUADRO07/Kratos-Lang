# Kratos-Lang LSP

This directory contains the first minimal Language Server Protocol adapter for
Kratos-Lang 0.1.0. It has no third-party dependencies and delegates parsing,
semantic analysis, and diagnostic rendering to the repository's `kratosc`
executable.

Supported messages:

- `initialize` / `shutdown` / `exit`;
- `textDocument/didOpen`;
- `textDocument/didChange` with full document synchronization;
- `textDocument/publishDiagnostics`.
- `textDocument/completion` for the frozen keywords and built-ins;
- `textDocument/hover` for keyword and type documentation;
- `textDocument/definition` for local variable and function declarations.

## Run

Build the compiler first, then start the server from the repository root:

```sh
make
python3 Kratos-Lang-LSP/server.py
```

Set `KRATOSC` when the compiler is elsewhere:

```sh
KRATOSC=/path/to/kratosc python3 Kratos-Lang-LSP/server.py
```

References, rename, and signature help belong to later iterations backed by
the reusable core library.
