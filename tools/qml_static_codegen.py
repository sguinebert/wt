#!/usr/bin/env python3
"""
Generate a typed C++ StaticWidgetNode tree from a minimal QML-like DSL.

Supported node types:
- Container
- Text
- PushButton

Supported properties:
- id
- class
- text
- action
"""

from __future__ import annotations

import argparse
import dataclasses
from pathlib import Path
from typing import List, Optional


@dataclasses.dataclass
class Token:
    kind: str
    value: str
    pos: int


@dataclasses.dataclass
class Node:
    type_name: str
    props: dict[str, str]
    children: List["Node"]


class ParseError(RuntimeError):
    pass


def tokenize(source: str) -> List[Token]:
    tokens: List[Token] = []
    i = 0
    n = len(source)

    def peek(offset: int = 0) -> str:
        j = i + offset
        return source[j] if j < n else ""

    while i < n:
        c = source[i]

        if c.isspace():
            i += 1
            continue

        if c == "/" and peek(1) == "/":
            i += 2
            while i < n and source[i] != "\n":
                i += 1
            continue

        if c in "{}:;":
            tokens.append(Token("symbol", c, i))
            i += 1
            continue

        if c == '"':
            start = i
            i += 1
            out: List[str] = []
            while i < n:
                ch = source[i]
                if ch == '"':
                    i += 1
                    tokens.append(Token("string", "".join(out), start))
                    break
                if ch == "\\":
                    i += 1
                    if i >= n:
                        raise ParseError(f"unterminated escape at {start}")
                    esc = source[i]
                    if esc == "n":
                        out.append("\n")
                    elif esc == "r":
                        out.append("\r")
                    elif esc == "t":
                        out.append("\t")
                    elif esc in ['\\', '"']:
                        out.append(esc)
                    else:
                        out.append(esc)
                    i += 1
                    continue
                out.append(ch)
                i += 1
            else:
                raise ParseError(f"unterminated string at {start}")
            continue

        if c.isalpha() or c == "_":
            start = i
            i += 1
            while i < n and (source[i].isalnum() or source[i] == "_"):
                i += 1
            tokens.append(Token("ident", source[start:i], start))
            continue

        raise ParseError(f"unexpected character '{c}' at {i}")

    tokens.append(Token("eof", "", n))
    return tokens


class Parser:
    def __init__(self, tokens: List[Token]) -> None:
        self.tokens = tokens
        self.index = 0

    def current(self) -> Token:
        return self.tokens[self.index]

    def accept(self, kind: str, value: Optional[str] = None) -> Optional[Token]:
        tok = self.current()
        if tok.kind != kind:
            return None
        if value is not None and tok.value != value:
            return None
        self.index += 1
        return tok

    def expect(self, kind: str, value: Optional[str] = None) -> Token:
        tok = self.accept(kind, value)
        if tok is None:
            want = f"{kind}:{value}" if value is not None else kind
            got = self.current()
            raise ParseError(f"expected {want}, got {got.kind}:{got.value} at {got.pos}")
        return tok

    def parse(self) -> Node:
        node = self.parse_node()
        self.expect("eof")
        return node

    def parse_node(self) -> Node:
        type_tok = self.expect("ident")
        self.expect("symbol", "{")

        props: dict[str, str] = {}
        children: List[Node] = []

        while not self.accept("symbol", "}"):
            tok = self.current()
            if tok.kind != "ident":
                raise ParseError(f"expected property or child node at {tok.pos}")

            # property: ident ":" string [";"]
            if self._looks_like_property():
                key = self.expect("ident").value
                self.expect("symbol", ":")
                value = self.expect("string").value
                self.accept("symbol", ";")
                props[key] = value
            else:
                children.append(self.parse_node())

        return Node(type_tok.value, props, children)

    def _looks_like_property(self) -> bool:
        if self.index + 1 >= len(self.tokens):
            return False
        a = self.tokens[self.index]
        b = self.tokens[self.index + 1]
        return a.kind == "ident" and b.kind == "symbol" and b.value == ":"


KIND_MAP = {
    "Container": "WidgetKind::Container",
    "Text": "WidgetKind::Text",
    "PushButton": "WidgetKind::PushButton",
}


def cpp_string(value: str) -> str:
    escaped = (
        value.replace("\\", "\\\\")
        .replace('"', '\\"')
        .replace("\n", "\\n")
        .replace("\r", "\\r")
        .replace("\t", "\\t")
    )
    return f'"{escaped}"'


def emit_node_initializer(node: Node, path: List[int], decls: List[str]) -> str:
    if node.type_name not in KIND_MAP:
        raise ParseError(f"unsupported node type: {node.type_name}")

    child_inits: List[str] = []
    for idx, child in enumerate(node.children):
        child_inits.append(emit_node_initializer(child, [*path, idx], decls))

    children_expr = "{}"
    if child_inits:
        arr_name = "node_" + "_".join(str(v) for v in path) + "_children"
        decls.append(
            f"inline constexpr std::array<StaticWidgetNode, {len(child_inits)}> {arr_name}{{{{"
        )
        for init in child_inits:
            for line in init.splitlines():
                decls.append(f"  {line}")
            decls.append("  ,")
        decls.append("}};")
        decls.append("")
        children_expr = f"std::span<const StaticWidgetNode>{{{arr_name}}}"

    pid = node.props.get("id", "")
    pclass = node.props.get("class", "")
    ptext = node.props.get("text", "")
    paction = node.props.get("action", "")

    lines = [
        "StaticWidgetNode{",
        f"  .kind = {KIND_MAP[node.type_name]},",
        "  .props = WidgetProps{",
        f"    .id = {cpp_string(pid)},",
        f"    .cssClass = {cpp_string(pclass)},",
        f"    .text = {cpp_string(ptext)},",
        f"    .action = {cpp_string(paction)},",
        "  },",
        f"  .children = {children_expr},",
        "}",
    ]
    return "\n".join(lines)


def generate_header(root: Node, symbol: str) -> str:
    decls: List[str] = []
    root_init = emit_node_initializer(root, [0], decls)

    out: List[str] = []
    out.append("// Generated file. Do not edit by hand.")
    out.append("#pragma once")
    out.append("")
    out.append("#include <array>")
    out.append("#include <span>")
    out.append("")
    out.append("#include <Wt/cpp26/qml_static_bootstrap_poc.hpp>")
    out.append("")
    out.append("namespace Wt::cpp26::qml_static::generated {")
    out.append("")
    out.extend(decls)
    out.append(f"inline constexpr StaticWidgetNode {symbol}{{")
    for line in root_init.splitlines()[1:-1]:
        out.append(line)
    out.append("};")
    out.append("")
    out.append("} // namespace Wt::cpp26::qml_static::generated")
    out.append("")
    return "\n".join(out)


def run(input_path: Path, output_path: Path, symbol: str) -> None:
    source = input_path.read_text(encoding="utf-8")
    tokens = tokenize(source)
    parser = Parser(tokens)
    root = parser.parse()
    header = generate_header(root, symbol)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(header, encoding="utf-8")


def main() -> None:
    ap = argparse.ArgumentParser(description="Generate StaticWidgetNode C++ header from QML-like DSL")
    ap.add_argument("input", type=Path, help="Input QML-like file")
    ap.add_argument("output", type=Path, help="Output C++ header")
    ap.add_argument("--symbol", default="compiled_tree", help="Root symbol name in generated header")
    args = ap.parse_args()

    run(args.input, args.output, args.symbol)


if __name__ == "__main__":
    main()
