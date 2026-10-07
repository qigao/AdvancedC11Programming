#!/usr/bin/env python3
"""Shared publication structure and generated table-of-contents helpers."""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# File names are stable source IDs. Publication order lives here and in each
# edition's BOOK_MANIFEST.txt.
PUBLICATION_ORDER = [
    "ch-01.md",
    "ch-02.md",
    "ch-03.md",
    "ch-04.md",
    "ch-05.md",
    "ch-06.md",
    "ch-07.md",
    "ch-13.md",  # Contract Compiler moves before live-runtime chapters.
    "ch-08.md",
    "ch-09.md",
    "ch-10.md",
    "ch-11.md",
    "ch-12.md",
    "ch-14.md",
    "ch-15.md",
]

PARTS = [
    (
        "I",
        {"cn": "CMeta：宏、类型与函数", "en": "CMeta: Macros, Types, and Functions"},
        ["ch-01.md", "ch-02.md", "ch-03.md"],
    ),
    (
        "II",
        {"cn": "CMeta 应用：计算图、Stream 与证明", "en": "CMeta Applications: Graphs, Streams, and Proofs"},
        ["ch-04.md", "ch-05.md", "ch-06.md", "ch-07.md"],
    ),
    (
        "III",
        {"cn": "CMeta 应用：契约绑定与代码生成", "en": "CMeta Applications: Contract Binding and Code Generation"},
        ["ch-13.md"],
    ),
    (
        "IV",
        {
            "cn": "CMeta 应用：响应式执行与工程边界",
            "en": "CMeta Applications: Reactive Execution and Engineering Boundaries",
        },
        ["ch-08.md", "ch-09.md", "ch-10.md", "ch-11.md", "ch-12.md"],
    ),
]

CLOSING = {
    "cn": "收束 — 技巧取舍与综合应用",
    "en": "Closing — Trade-offs and Integration",
}
CLOSING_FILES = ["ch-14.md", "ch-15.md"]

TOC_START = "<!-- book-toc:start -->"
TOC_END = "<!-- book-toc:end -->"

CN_CHAPTER_NUMBERS = {
    "一": 1,
    "二": 2,
    "三": 3,
    "四": 4,
    "五": 5,
    "六": 6,
    "七": 7,
    "八": 8,
    "九": 9,
    "十": 10,
    "十一": 11,
    "十二": 12,
    "十三": 13,
    "十四": 14,
    "十五": 15,
}


def read_manifest(edition: str) -> list[str]:
    manifest = ROOT / edition / "BOOK_MANIFEST.txt"
    return [
        line.strip()
        for line in manifest.read_text(encoding="utf-8").splitlines()
        if line.strip() and not line.lstrip().startswith("#")
    ]


def chapter_path(edition: str, source_id: str) -> Path:
    return ROOT / edition / source_id


def chapter_h1(edition: str, source_id: str) -> str:
    path = chapter_path(edition, source_id)
    for line in path.read_text(encoding="utf-8").splitlines():
        if line.startswith("# "):
            return line[2:].strip()
    raise RuntimeError(f"{path}: missing H1")


def chapter_number_from_h1(edition: str, h1: str) -> int:
    if edition == "cn":
        match = re.match(r"^第(十五|十四|十三|十二|十一|十|九|八|七|六|五|四|三|二|一)章[：:]", h1)
        if match is None:
            raise RuntimeError(f"malformed Chinese chapter title: {h1}")
        return CN_CHAPTER_NUMBERS[match.group(1)]

    match = re.match(r"^Chapter\s+(\d+):", h1)
    if match is None:
        raise RuntimeError(f"malformed English chapter title: {h1}")
    return int(match.group(1))


def publication_number(source_id: str) -> int:
    return PUBLICATION_ORDER.index(source_id) + 1


def _entry_line(
    edition: str,
    source_id: str,
    *,
    link_prefix: str | None,
) -> str:
    title = chapter_h1(edition, source_id)
    if link_prefix is None:
        return f"- {title}"
    return f"- [{title}]({link_prefix}{source_id})"


def render_toc_body(
    edition: str,
    *,
    link_prefix: str | None,
) -> str:
    lines: list[str] = []

    for roman, titles, source_ids in PARTS:
        lines.append(f"**Part {roman} — {titles[edition]}**")
        lines.extend(
            _entry_line(edition, source_id, link_prefix=link_prefix)
            for source_id in source_ids
        )
        lines.append("")

    lines.append(f"**{CLOSING[edition]}**")
    lines.extend(
        _entry_line(edition, source_id, link_prefix=link_prefix)
        for source_id in CLOSING_FILES
    )

    return "\n".join(lines)


def render_toc_block(
    edition: str,
    *,
    link_prefix: str | None,
) -> str:
    return (
        f"{TOC_START}\n"
        f"{render_toc_body(edition, link_prefix=link_prefix)}\n"
        f"{TOC_END}"
    )


def replace_toc_block(
    text: str,
    edition: str,
    *,
    link_prefix: str | None,
) -> str:
    block = render_toc_block(edition, link_prefix=link_prefix)
    pattern = re.compile(
        re.escape(TOC_START) + r".*?" + re.escape(TOC_END),
        re.DOTALL,
    )
    if pattern.search(text) is None:
        raise RuntimeError("TOC markers missing")
    return pattern.sub(block, text, count=1)
