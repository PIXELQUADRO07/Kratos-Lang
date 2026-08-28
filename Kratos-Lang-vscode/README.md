# Kratos-Lang for VS Code

Language support for Kratos-Lang 0.1.0.

The extension provides:

- `.kratos` language recognition;
- TextMate syntax highlighting;
- bracket, quote, and `$ ... $` comment support;
- editor indentation for blocks.

## Local installation

From the repository root:

```sh
code --install-extension Kratos-Lang-vscode
```

Alternatively, package the directory with `vsce` and install the generated
`.vsix` file.

The grammar follows the frozen syntax in
[../docs/specification.md](../docs/specification.md). Language-server support
is intentionally not included yet.
