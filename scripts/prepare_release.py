#!/usr/bin/env python3
"""Prepare one canonical manuscript edition for HTML/EPUB/PDF rendering."""

from __future__ import annotations

import argparse
import re
import shutil
from pathlib import Path

from build_book import EDITION_META

ROOT = Path(__file__).resolve().parents[1]
BOOK_NAME = "Advanced-C11-Programming"
FENCE_RE = re.compile(
    r"(?ms)^(?P<fence>`{3}|~{3})mermaid\s*\n(?P<body>.*?)(?:\n)(?P=fence)\s*$"
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--edition", choices=("cn", "en"), required=True)
    return parser.parse_args()


def normalize_body_horizontal_rules(text: str) -> str:
    """Prevent Pandoc from treating body horizontal rules as YAML metadata.

    Keep the initial YAML front matter intact. After it, rewrite standalone
    '---' thematic breaks only when they are outside fenced code blocks.
    """
    if not text.startswith("---\n"):
        raise SystemExit("canonical manuscript is missing initial YAML front matter")

    closing = text.find("\n---\n", 4)
    if closing < 0:
        raise SystemExit("canonical manuscript YAML front matter is not closed")

    split = closing + len("\n---\n")
    front = text[:split]
    body = text[split:]
    lines = body.splitlines()
    normalized: list[str] = []
    fence: str | None = None

    for line in lines:
        fence_match = re.match(r"^\\s*([~]{3,}|[\\x60]{3,})", line)
        if fence_match:
            marker = fence_match.group(1)[0]
            if fence is None:
                fence = marker
            elif fence == marker:
                fence = None
            normalized.append(line)
            continue

        if fence is None and line.strip() == "---":
            normalized.append("***")
        else:
            normalized.append(line)

    suffix = "\n" if body.endswith("\n") else ""
    return front + "\n".join(normalized) + suffix


def main() -> None:
    args = parse_args()
    edition = args.edition
    source = ROOT / "dist" / edition / f"{BOOK_NAME}.md"
    out_dir = ROOT / "dist" / "release" / edition
    diagram_dir = out_dir / "diagrams"
    output = out_dir / f"{BOOK_NAME}.release.md"
    epub_output = out_dir / f"{BOOK_NAME}.epub.md"

    if not source.is_file():
        raise SystemExit(
            f"missing canonical manuscript for {edition}; "
            f"run scripts/build_book.py --edition {edition} first"
        )

    text = normalize_body_horizontal_rules(source.read_text(encoding="utf-8"))
    if out_dir.exists():
        shutil.rmtree(out_dir)
    diagram_dir.mkdir(parents=True)
    diagrams: list[str] = []

    def replace_mermaid(match: re.Match[str]) -> str:
        index = len(diagrams) + 1
        stem = f"diagram-{index:03d}"
        body = match.group("body").rstrip() + "\n"
        (diagram_dir / f"{stem}.mmd").write_text(
            body, encoding="utf-8", newline="\n"
        )
        diagrams.append(stem)
        return f"![Diagram {index}](diagrams/{stem}.svg)"

    prepared = FENCE_RE.sub(replace_mermaid, text)
    prepared = prepared.replace(
        "<!-- chapter-break -->", "::: {.chapter-break}\n:::"
    )
    # Pandoc renders the title, subtitle and short abstract on its title page.
    # Remove the Markdown-only display block to avoid repeating it after the TOC.
    meta = EDITION_META[edition]
    prepared, replacements = re.subn(
        rf"(?m)^# {re.escape(meta['title'])}\n\n"
        rf"\*\*{re.escape(meta['subtitle'])}\*\*\n\n"
        rf"\*{re.escape(meta['tagline'])}\*\n",
        "", prepared, count=1,
    )
    if replacements != 1:
        raise SystemExit(
            f"{edition}: canonical display-title block not found exactly once"
        )
    if FENCE_RE.search(prepared):
        raise SystemExit(f"{edition}: Mermaid fence remained after preparation")

    output.write_text(prepared, encoding="utf-8", newline="\n")
    epub_prepared = re.sub(
        r"(diagrams/diagram-\d{3})\.svg", r"\1.png", prepared
    )
    if "diagrams/diagram-" in epub_prepared and ".svg" in epub_prepared:
        raise SystemExit(f"{edition}: EPUB retained SVG diagram references")
    if epub_prepared.count(".png)") != len(diagrams):
        raise SystemExit(f"{edition}: EPUB PNG diagram reference count mismatch")
    epub_output.write_text(epub_prepared, encoding="utf-8", newline="\n")
    print(
        f"prepared {edition}: {output.relative_to(ROOT)} and "
        f"{epub_output.relative_to(ROOT)} with {len(diagrams)} Mermaid diagrams"
    )


if __name__ == "__main__":
    main()
