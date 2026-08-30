# Kratos-Lang for VS Code

Language support for Kratos-Lang 0.3.0.

The extension provides:

- `.kratos` language recognition;
- TextMate syntax highlighting;
- bracket, quote, and `$ ... $` comment support;
- editor indentation for blocks.

## Local installation

Package the extension directory with `vsce`, then install the generated
package:

```sh
vsce package Kratos-Lang-vscode
code --install-extension kratos-lang-0.3.0.vsix
```

For development without packaging, use VS Code's extension development path
support to launch the extension locally.

The grammar follows the frozen syntax in
[../docs/specification.md](../docs/specification.md). Language-server support
is intentionally not included yet.
