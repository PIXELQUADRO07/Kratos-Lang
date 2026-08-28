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
    "craft", "yield", "shout", "wield", "true", "false", "not",
)


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
                    },
                    "serverInfo": {"name": "kratos-lsp", "version": "0.1.0"},
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
