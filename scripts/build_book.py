#!/usr/bin/env python3
"""Build one canonical single-file Markdown manuscript edition.

Editable chapter sources live under cn/ and en/. Each edition owns a local
BOOK_MANIFEST.txt containing stable ch-NN.md filenames.

The Chinese edition remains the default so release tooling can call this
script without arguments. Its default manuscript copy is also written to
dist/Advanced-C11-Programming.md.
"""

from __future__ import annotations

import argparse
from pathlib import Path

from book_structure import ROOT, read_manifest, render_toc_body

BOOK_NAME = "Advanced-C11-Programming.md"

EDITION_META = {
    "cn": {
        "lang": "zh-CN",
        "title": "C11 高级编程",
        "subtitle": "从宏到现代编程模型",
        "tagline": "元编程、反射、流式与响应式编程",
    },
    "en": {
        "lang": "en-US",
        "title": "Advanced C11 Programming",
        "subtitle": "From Macros to Modern Programming Models",
        "tagline": "Metaprogramming, Reflection, Streaming, and Reactive Programming",
    },
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--edition",
        choices=sorted(EDITION_META),
        default="cn",
        help="manuscript edition to build (default: cn)",
    )
    return parser.parse_args()


def chapter_paths(edition: str) -> list[Path]:
    edition_dir = ROOT / edition
    return [edition_dir / entry for entry in read_manifest(edition)]


def front_matter(edition: str) -> str:
    meta = EDITION_META[edition]
    toc_heading = "目录" if edition == "cn" else "Table of Contents"
    toc = render_toc_body(edition, link_prefix=None)
    return f"""---
title: "{meta['title']}"
subtitle: "{meta['subtitle']}"
description: "{meta['tagline']}"
lang: "{meta['lang']}"
rights: "Apache-2.0"
---

# {meta['title']}

**{meta['subtitle']}**

*{meta['tagline']}*

> This file is generated from the ordered chapter sources listed in
> {edition}/BOOK_MANIFEST.txt. Edit the chapter files, not this artifact.

## {toc_heading}

{toc}
"""


def main() -> None:
    args = parse_args()
    edition = args.edition

    pieces = [front_matter(edition).rstrip()]
    for path in chapter_paths(edition):
        text = path.read_text(encoding="utf-8").lstrip("\ufeff").rstrip()
        pieces.append(text)

    output_text = "\n\n<!-- chapter-break -->\n\n".join(pieces) + "\n"
    output = ROOT / "dist" / edition / BOOK_NAME
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(output_text, encoding="utf-8", newline="\n")

    # Also expose the default Chinese edition at the top-level manuscript path.
    if edition == "cn":
        default_output = ROOT / "dist" / BOOK_NAME
        default_output.parent.mkdir(parents=True, exist_ok=True)
        default_output.write_text(output_text, encoding="utf-8", newline="\n")

    print(
        f"built {edition} edition: {output.relative_to(ROOT)} "
        f"({len(output_text.splitlines()):,} lines)"
    )


if __name__ == "__main__":
    main()
