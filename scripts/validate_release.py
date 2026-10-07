#!/usr/bin/env python3
"""Validate rendered release artifacts for one manuscript edition."""

from __future__ import annotations

import argparse
import re
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BOOK_NAME = "Advanced-C11-Programming"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--edition", choices=("cn", "en"), required=True)
    return parser.parse_args()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(message)


def main() -> None:
    args = parse_args()
    edition = args.edition
    release = ROOT / "dist" / "release" / edition
    html_path = release / f"{BOOK_NAME}.html"
    epub_path = release / f"{BOOK_NAME}.epub"
    pdf_path = release / f"{BOOK_NAME}.pdf"
    prepared_path = release / f"{BOOK_NAME}.release.md"
    epub_prepared_path = release / f"{BOOK_NAME}.epub.md"
    diagrams = release / "diagrams"

    require(prepared_path.is_file(), f"{edition}: prepared release Markdown missing")
    prepared = prepared_path.read_text(encoding="utf-8")
    require(epub_prepared_path.is_file(), f"{edition}: prepared EPUB Markdown missing")
    epub_prepared = epub_prepared_path.read_text(encoding="utf-8")
    require("```mermaid" not in prepared, f"{edition}: backtick Mermaid fence remains")
    require("~~~mermaid" not in prepared, f"{edition}: tilde Mermaid fence remains")

    sources = sorted(diagrams.glob("diagram-*.mmd"))
    svgs = sorted(diagrams.glob("diagram-*.svg"))
    pngs = sorted(diagrams.glob("diagram-*.png"))
    require(
        len(sources) == len(svgs),
        f"{edition}: Mermaid SVG count mismatch: {len(sources)} != {len(svgs)}",
    )
    require(
        len(sources) == len(pngs),
        f"{edition}: Mermaid PNG count mismatch: {len(sources)} != {len(pngs)}",
    )
    for svg in svgs:
        require(svg.stat().st_size > 100, f"{edition}: suspicious SVG: {svg.name}")
        require(
            "<svg" in svg.read_text(encoding="utf-8"),
            f"{edition}: not an SVG document: {svg.name}",
        )

    for png in pngs:
        require(png.stat().st_size > 100, f"{edition}: suspicious PNG: {png.name}")
        with png.open("rb") as handle:
            require(
                handle.read(8) == b"\x89PNG\r\n\x1a\n",
                f"{edition}: not a PNG document: {png.name}",
            )

    expected_refs = set(
        re.findall(r"diagrams/(diagram-\d{3}\.svg)", prepared)
    )
    actual_refs = {p.name for p in svgs}
    require(
        expected_refs == actual_refs,
        f"{edition}: prepared Markdown diagram references mismatch SVG set",
    )
    expected_epub_refs = set(
        re.findall(r"diagrams/(diagram-\d{3}\.png)", epub_prepared)
    )
    actual_png_refs = {p.name for p in pngs}
    require(
        expected_epub_refs == actual_png_refs,
        f"{edition}: EPUB diagram references mismatch PNG set",
    )

    require(
        html_path.is_file() and html_path.stat().st_size > 100_000,
        f"{edition}: HTML missing/small",
    )
    html_text = html_path.read_text(encoding="utf-8")
    require("<html" in html_text.lower(), f"{edition}: HTML root missing")
    require('id="TOC"' in html_text, f"{edition}: HTML TOC missing")
    if sources:
        require(
            "data:image/svg+xml" in html_text or "<svg" in html_text,
            f"{edition}: HTML does not embed rendered diagrams",
        )

    require(
        epub_path.is_file() and epub_path.stat().st_size > 100_000,
        f"{edition}: EPUB missing/small",
    )
    with zipfile.ZipFile(epub_path) as archive:
        names = set(archive.namelist())
        require("mimetype" in names, f"{edition}: EPUB mimetype missing")
        require(
            "META-INF/container.xml" in names,
            f"{edition}: EPUB container.xml missing",
        )
        require(
            archive.read("mimetype") == b"application/epub+zip",
            f"{edition}: EPUB mimetype invalid",
        )
        if sources:
            require(
                any(name.lower().endswith(".png") for name in names),
                f"{edition}: EPUB contains no rendered PNG diagrams",
            )

    require(
        pdf_path.is_file() and pdf_path.stat().st_size > 100_000,
        f"{edition}: PDF missing/small",
    )
    with pdf_path.open("rb") as handle:
        require(handle.read(5) == b"%PDF-", f"{edition}: PDF magic header missing")

    print(
        f"release artifacts valid ({edition}): "
        f"{len(svgs)} SVG + {len(pngs)} PNG diagrams, "
        f"HTML {html_path.stat().st_size:,} B, "
        f"EPUB {epub_path.stat().st_size:,} B, "
        f"PDF {pdf_path.stat().st_size:,} B"
    )


if __name__ == "__main__":
    main()
