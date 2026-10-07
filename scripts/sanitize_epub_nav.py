#!/usr/bin/env python3
"""Normalize one edition EPUB navigation document to a text-only TOC."""

from __future__ import annotations

import argparse
import html
import re
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BOOK_NAME = "Advanced-C11-Programming"

IMG_RE = re.compile(rb'<img\b(?P<attrs>[^>]*)/?>', re.IGNORECASE)
ALT_RE = re.compile(rb'\balt="(?P<alt>[^"]*)"', re.IGNORECASE)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--edition", choices=("cn", "en"), required=True)
    return parser.parse_args()


def replace_img(match: re.Match[bytes]) -> bytes:
    attrs = match.group("attrs")
    alt_match = ALT_RE.search(attrs)
    if not alt_match:
        return b""
    alt = html.unescape(alt_match.group("alt").decode("utf-8"))
    return html.escape(alt, quote=False).encode("utf-8")


def main() -> None:
    args = parse_args()
    epub = ROOT / "dist" / "release" / args.edition / f"{BOOK_NAME}.epub"
    tmp = epub.with_suffix(".epub.tmp")

    if not epub.is_file():
        raise SystemExit(f"missing EPUB: {epub}")

    changed = 0
    with zipfile.ZipFile(epub, "r") as src, zipfile.ZipFile(tmp, "w") as dst:
        for info in src.infolist():
            data = src.read(info.filename)
            if info.filename.endswith("/nav.xhtml"):
                new_data, count = IMG_RE.subn(replace_img, data)
                data = new_data
                changed += count
            dst.writestr(info, data)

    if changed == 0:
        tmp.unlink(missing_ok=True)
        print(f"{args.edition}: EPUB nav already contains no image nodes")
        return

    tmp.replace(epub)
    print(
        f"{args.edition}: normalized EPUB nav; "
        f"replaced {changed} image node(s) with text"
    )


if __name__ == "__main__":
    main()
