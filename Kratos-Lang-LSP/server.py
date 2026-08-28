#!/usr/bin/env python3
"""Minimal Kratos language server backed by kratosc --check."""

import json
import os
import re
import subprocess
import sys
from typing import Any

DIAGNOSTIC_RE = re.compile(
    r"^(error|warning)\[K(\d+)\]: (.+)\n"
    r"  --> (.*):(\d+):(\d+)\n"
)

COMPLETION_WORDS = (
    "k_int", "k_float", "k_bool", "k_char", "k_string", "k_const", "k_void",
    "if", "elif", "else", "hold", "press", "drive", "sweep", "in", "snap", "push",
    "craft", "yield", "shout", "wield", "true", "false", "not", "record",
)

HOVER_TEXT = {
    "craft": "`craft` declares a function.",
    "yield": "`yield` returns a value from a craft.",
    "shout": "`shout(expression);` writes a value to standard output.",
    "wield": "`wield \"path\";` imports another Kratos source file.",
    "hold": "`hold (condition) { ... }` repeats while a condition is true.",
    "press": "`press { ... } hold (condition);` executes a post-condition loop.",
    "drive": "`drive (...) { ... }` runs a three-part loop.",
    "sweep": "`sweep (type name in collection) { ... }` iterates an array.",
    "not": "`not` negates a `k_bool` expression.",
    "k_int": "Signed integer type.",
    "k_float": "Floating-point type.",
    "k_bool": "Boolean type.",
    "k_char": "Character type.",
    "k_string": "String type.",
    "k_const": "Declares a value that cannot be reassigned.",
    "k_void": "The no-value return type for a craft.",
    "record": "`record Name { type field; ... }` declares a composite record type (struct).",
}


def read_message() -> dict[str, Any] | None:
    content_length = None
    while True:
        line = sys.stdin.buffer.readline()
        if not line:
            return None
        if line in (b"\r\n", b"\n"):
            break
        name, _, value = line.decode("ascii").partition(":")
        if name.lower() == "content-length":
            content_length = int(value.strip())
    if content_length is None:
        return None
    payload = sys.stdin.buffer.read(content_length)
    return json.loads(payload.decode("utf-8"))


def send_message(message: dict[str, Any]) -> None:
    payload = json.dumps(message, separators=(",", ":"), ensure_ascii=False).encode("utf-8")
    header = f"Content-Length: {len(payload)}\r\n\r\n".encode("ascii")
    sys.stdout.buffer.write(header + payload)
    sys.stdout.buffer.flush()


def response(request_id: Any, result: Any) -> None:
    send_message({"jsonrpc": "2.0", "id": request_id, "result": result})


def uri_path(uri: str) -> str:
    if uri.startswith("file://"):
        path = uri[7:]
        return path or "<stdin>"
    return uri or "<stdin>"


def diagnostics_for(source: str, filename: str) -> list[dict[str, Any]]:
    kratosc = os.environ.get("KRATOSC", "./kratosc")
    result = subprocess.run(
        [kratosc, "--check"],
        input=source,
        text=True,
        capture_output=True,
        check=False,
    )
    diagnostics = []
    for match in DIAGNOSTIC_RE.finditer(result.stderr):
        severity, code, message, _, line, column = match.groups()
        diagnostics.append(
            {
                "severity": 1 if severity == "error" else 2,
                "message": message,
                "source": "kratosc",
                "code": f"K{code}",
                "range": {
                    "start": {"line": int(line) - 1, "character": int(column) - 1},
                    "end": {"line": int(line) - 1, "character": int(column)},
                },
            }
        )
    return diagnostics


def publish(uri: str, source: str) -> None:
    send_message(
        {
            "jsonrpc": "2.0",
            "method": "textDocument/publishDiagnostics",
            "params": {"uri": uri, "diagnostics": diagnostics_for(source, uri_path(uri))},
        }
    )


def completion_items(source: str, line: int, character: int) -> list[dict[str, str]]:
    lines = source.splitlines()
    current_line = lines[line] if 0 <= line < len(lines) else ""
    prefix = current_line[:character]
    match = re.search(r"[A-Za-z_][A-Za-z0-9_]*$", prefix)
    typed = match.group(0) if match else ""
    return [
        {"label": word, "kind": 14, "detail": "Kratos keyword"}
        for word in COMPLETION_WORDS
        if word.startswith(typed)
    ]


def word_at(source: str, line: int, character: int) -> str:
    lines = source.splitlines()
    current_line = lines[line] if 0 <= line < len(lines) else ""
    left = current_line[:character]
    right = current_line[character:]
    left_match = re.search(r"[A-Za-z_][A-Za-z0-9_]*$", left)
    right_match = re.match(r"[A-Za-z0-9_]*", right)
    return (left_match.group(0) if left_match else "") + (right_match.group(0) if right_match else "")


def definition_for(uri: str, source: str, word: str) -> dict[str, Any] | None:
    pattern = re.compile(
        rf"\b(?:k_const\s+)?(?:k_int|k_float|k_bool|k_char|k_string|k_void)\s+"
        rf"(?:craft\s+)?(?:\[\]\s+)?{re.escape(word)}\b"
    )
    match = pattern.search(source)
    if match is None:
        return None
    line = source.count("\n", 0, match.start())
    line_start = source.rfind("\n", 0, match.start()) + 1
    column = match.start() - line_start
    return {
        "uri": uri,
        "range": {
            "start": {"line": line, "character": column},
            "end": {"line": line, "character": column + len(word)},
        },
    }


def main() -> None:
    documents: dict[str, str] = {}
    while True:
        request = read_message()
        if request is None:
            return
        method = request.get("method")
        request_id = request.get("id")
        params = request.get("params", {})

        if method == "initialize":
            response(
                request_id,
                {
                    "capabilities": {
                        "textDocumentSync": {"openClose": True, "change": 1},
                        "completionProvider": {"triggerCharacters": ["_"]},
                        "hoverProvider": True,
                        "definitionProvider": True,
                    },
                    "serverInfo": {"name": "kratos-lsp", "version": "0.2.0"},
                },
            )
        elif method == "shutdown":
            response(request_id, None)
        elif method == "exit":
            return
        elif method == "textDocument/didOpen":
            document = params["textDocument"]
            uri = document["uri"]
            documents[uri] = document["text"]
            publish(uri, documents[uri])
        elif method == "textDocument/didChange":
            document = params["textDocument"]
            uri = document["uri"]
            changes = params.get("contentChanges", [])
            if changes and "text" in changes[-1]:
                documents[uri] = changes[-1]["text"]
                publish(uri, documents[uri])
        elif method == "textDocument/completion":
            document = params["textDocument"]
            uri = document["uri"]
            position = params["position"]
            response(
                request_id,
                {
                    "isIncomplete": False,
                    "items": completion_items(
                        documents.get(uri, ""), position["line"], position["character"]
                    ),
                },
            )
        elif method == "textDocument/hover":
            document = params["textDocument"]
            uri = document["uri"]
            position = params["position"]
            word = word_at(documents.get(uri, ""), position["line"], position["character"])
            response(
                request_id,
                {"contents": {"kind": "markdown", "value": HOVER_TEXT[word]}}
                if word in HOVER_TEXT
                else None,
            )
        elif method == "textDocument/definition":
            document = params["textDocument"]
            uri = document["uri"]
            position = params["position"]
            source = documents.get(uri, "")
            word = word_at(source, position["line"], position["character"])
            location = definition_for(uri, source, word)
            response(request_id, [location] if location is not None else [])
        elif request_id is not None:
            send_message(
                {
                    "jsonrpc": "2.0",
                    "id": request_id,
                    "error": {"code": -32601, "message": f"Method not found: {method}"},
                }
            )


if __name__ == "__main__":
    main()
