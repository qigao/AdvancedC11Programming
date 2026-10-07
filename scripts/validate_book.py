#!/usr/bin/env python3
"""Zero-dependency publication QA for the Markdown manuscript."""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path
from urllib.parse import unquote, urlsplit

from book_structure import (
    PUBLICATION_ORDER,
    ROOT,
    chapter_number_from_h1,
    read_manifest,
    render_toc_block,
)

SHARED_MARKDOWN = [
    ROOT / "README.md",
    ROOT / "README_CN.md",
    ROOT / "BOOK_ARCHITECTURE.md",
    ROOT / "CHAPTER_TEMPLATE.md",
    ROOT / "SOURCE_SNAPSHOTS.md",
]

STALE_PATH_RE = re.compile(r"salts/(?:book|docs)", re.IGNORECASE)
MOVING_MASTER_LINK_RE = re.compile(
    r"(?:github\.com/[^)\s]+/(?:blob|tree)/master\b"
    r"|raw\.githubusercontent\.com/[^/\s]+/[^/\s]+/master/)",
    re.IGNORECASE,
)
STALE_IDL_PUBLIC_SURFACE_RE = re.compile(
    r"(?:\bdatabind_target\s*\(|\.dbidl\b)",
    re.IGNORECASE,
)
STALE_CMETA_PUBLIC_SURFACE_RE = re.compile(
    r"(?:\bcmeta_type_id\s*\(|\bcmeta_function_contract_equal\s*\()"
)
STALE_PLUGIN_PUBLIC_SURFACE_RE = re.compile(
    r"(?:\bsalts_plugin_[A-Za-z0-9_]*|\bSALTS_PLUGIN_[A-Za-z0-9_]*)"
)
LINK_RE = re.compile(r"!?\[[^\]]*\]\(([^)]+)\)")
PART_RE = re.compile(r"^#{1,3}\s+(Part\s+[IVX]+\s+—\s+.+?)\s*$")


class ValidationError(RuntimeError):
    pass


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--edition",
        choices=("cn", "en"),
        default="cn",
        help="edition to validate (default: cn)",
    )
    return parser.parse_args()


def manifest_entries(edition: str) -> list[str]:
    edition_dir = ROOT / edition
    entries = read_manifest(edition)
    if entries != PUBLICATION_ORDER:
        raise ValidationError(
            f"{edition}/BOOK_MANIFEST.txt does not match canonical publication "
            f"order: {entries}"
        )
    for entry in entries:
        if not (edition_dir / entry).is_file():
            raise ValidationError(
                f"{edition} manifest chapter does not exist: {entry}"
            )
    return entries


def validate_legacy_chinese_manifest() -> None:
    manifest = ROOT / "BOOK_MANIFEST.txt"
    entries = [
        line.strip()
        for line in manifest.read_text(encoding="utf-8").splitlines()
        if line.strip() and not line.lstrip().startswith("#")
    ]
    expected = [f"cn/{entry}" for entry in PUBLICATION_ORDER]
    if entries != expected:
        raise ValidationError(
            "root BOOK_MANIFEST.txt must mirror the canonical Chinese "
            "publication order"
        )


def outside_fences(lines: list[str]):
    fence: str | None = None
    for number, line in enumerate(lines, start=1):
        match = re.match(r"^\s*([~]{3,}|[\x60]{3,})", line)
        if match:
            marker = match.group(1)[0]
            if fence is None:
                fence = marker
            elif fence == marker:
                fence = None
            continue
        if fence is None:
            yield number, line


def validate_chapter(path: Path, edition: str, expected_number: int) -> None:
    lines = path.read_text(encoding="utf-8").splitlines()
    h1 = []
    main_h2: list[int] = []
    numbered_h3: dict[int, list[int]] = {}
    unnumbered_h2 = []
    decimal_h2 = []
    invalid_h3 = []
    current_main: int | None = None

    for number, line in outside_fences(lines):
        if re.match(r"^#\s+", line):
            h1.append((number, line))

        if re.match(r"^##\s+", line):
            nested = re.match(r"^##\s+(\d+)\.(\d+)\b", line)
            if nested:
                decimal_h2.append((number, line))
                current_main = None
                continue

            section = re.match(r"^##\s+(\d+)\.\s+", line)
            if section:
                current_main = int(section.group(1))
                main_h2.append(current_main)
            else:
                unnumbered_h2.append((number, line))
                current_main = None
            continue

        if re.match(r"^###\s+", line):
            subsection = re.match(r"^###\s+(\d+)\.(\d+)\s+", line)
            if subsection:
                main = int(subsection.group(1))
                child = int(subsection.group(2))
                if current_main is None or main != current_main:
                    invalid_h3.append((number, line, current_main))
                else:
                    numbered_h3.setdefault(main, []).append(child)

    if len(h1) != 1:
        raise ValidationError(f"{path.name}: expected exactly one H1, got {len(h1)}")

    visible_number = chapter_number_from_h1(
        edition,
        h1[0][1][2:].strip(),
    )
    if visible_number != expected_number:
        raise ValidationError(
            f"{path.name}: H1 says chapter {visible_number}, "
            f"publication position is {expected_number}"
        )

    if decimal_h2:
        where = ", ".join(str(number) for number, _ in decimal_h2[:5])
        raise ValidationError(
            f"{path.name}: numbered subsections must use H3 (### N.M), "
            f"not H2, at line(s) {where}"
        )

    if unnumbered_h2:
        where = ", ".join(str(number) for number, _ in unnumbered_h2[:5])
        raise ValidationError(
            f"{path.name}: unnumbered/label H2 at line(s) {where}"
        )

    unique_sections = list(dict.fromkeys(main_h2))
    if len(unique_sections) != len(main_h2):
        duplicates = [
            section
            for index, section in enumerate(main_h2)
            if section in main_h2[:index]
        ]
        raise ValidationError(
            f"{path.name}: duplicate numbered H2 section(s): {duplicates}"
        )

    expected = list(range(1, len(unique_sections) + 1))
    if unique_sections != expected:
        raise ValidationError(
            f"{path.name}: numbered H2 sections are not continuous: "
            f"{unique_sections}"
        )

    if invalid_h3:
        number, line, parent = invalid_h3[0]
        raise ValidationError(
            f"{path.name}: H3 numbering does not match parent H2 at line "
            f"{number}: {line!r}; current parent={parent}"
        )

    for main, children in numbered_h3.items():
        unique_children = list(dict.fromkeys(children))
        if len(unique_children) != len(children):
            duplicates = [
                child
                for index, child in enumerate(children)
                if child in children[:index]
            ]
            raise ValidationError(
                f"{path.name}: duplicate numbered H3 subsection(s) under "
                f"{main}: {duplicates}"
            )

        child_expected = list(range(1, len(unique_children) + 1))
        if unique_children != child_expected:
            raise ValidationError(
                f"{path.name}: numbered H3 subsections under {main} are not "
                f"continuous: {unique_children}"
            )


def validate_text_hygiene(path: Path) -> None:
    text = path.read_text(encoding="utf-8")
    if STALE_PATH_RE.search(text):
        raise ValidationError(
            f"{path.name}: contains removed salts/book or salts/docs path"
        )
    if MOVING_MASTER_LINK_RE.search(text):
        raise ValidationError(
            f"{path.name}: contains a moving GitHub master source link"
        )
    if STALE_IDL_PUBLIC_SURFACE_RE.search(text):
        raise ValidationError(
            f"{path.name}: contains retired DataBind public build spelling "
            "(databind_target/.dbidl)"
        )
    if STALE_CMETA_PUBLIC_SURFACE_RE.search(text):
        raise ValidationError(
            f"{path.name}: contains retired/nonexistent CMeta equality API"
        )
    if STALE_PLUGIN_PUBLIC_SURFACE_RE.search(text):
        raise ValidationError(
            f"{path.name}: contains retired pre-CMeta Plugin public prefix"
        )


def validate_internal_links(path: Path) -> None:
    lines = path.read_text(encoding="utf-8").splitlines()
    for _, line in outside_fences(lines):
        for raw_target in LINK_RE.findall(line):
            target = raw_target.strip().strip("<>")
            if not target or target.startswith("#"):
                continue

            parsed = urlsplit(target)
            if parsed.scheme or parsed.netloc:
                continue

            local = unquote(parsed.path)
            if not local:
                continue

            destination = (path.parent / local).resolve()
            try:
                destination.relative_to(ROOT.resolve())
            except ValueError as exc:
                raise ValidationError(
                    f"{path.name}: internal link escapes repository: {raw_target}"
                ) from exc

            if not destination.exists():
                raise ValidationError(
                    f"{path.name}: broken internal link {raw_target} -> "
                    f"{destination.relative_to(ROOT)}"
                )


def extract_parts(path: Path) -> list[str]:
    parts = []
    for line in path.read_text(encoding="utf-8").splitlines():
        match = PART_RE.match(line)
        if match:
            parts.append(match.group(1))
    return parts


def validate_part_structure() -> None:
    english = extract_parts(ROOT / "README.md")
    chinese = extract_parts(ROOT / "README_CN.md")
    architecture = extract_parts(ROOT / "BOOK_ARCHITECTURE.md")

    if len(english) != 4 or len(chinese) != 4 or len(architecture) != 4:
        raise ValidationError(
            "README, README_CN and BOOK_ARCHITECTURE must each contain "
            "exactly four Parts"
        )
    if chinese != architecture:
        raise ValidationError(
            "README_CN and BOOK_ARCHITECTURE four-Part titles/order do not match"
        )

    expected_roman = ["I", "II", "III", "IV"]
    for label, parts in (("README", english), ("README_CN", chinese)):
        roman = []
        for part in parts:
            match = re.match(r"Part\s+([IVX]+)", part)
            if match is None:
                raise ValidationError(f"{label}: malformed Part heading: {part}")
            roman.append(match.group(1))
        if roman != expected_roman:
            raise ValidationError(f"{label}: unexpected Part order: {roman}")



def chapter_numbering_shape(path: Path) -> tuple[tuple[int, tuple[int, ...]], ...]:
    lines = path.read_text(encoding="utf-8").splitlines()
    shape: list[tuple[int, list[int]]] = []
    current_index: int | None = None

    for _, line in outside_fences(lines):
        h2 = re.match(r"^##\s+(\d+)\.\s+", line)
        if h2:
            shape.append((int(h2.group(1)), []))
            current_index = len(shape) - 1
            continue

        h3 = re.match(r"^###\s+(\d+)\.(\d+)\s+", line)
        if h3 and current_index is not None:
            shape[current_index][1].append(int(h3.group(2)))

    return tuple(
        (main, tuple(children))
        for main, children in shape
    )


def validate_edition_structure_parity() -> None:
    for source_id in PUBLICATION_ORDER:
        cn_path = ROOT / "cn" / source_id
        en_path = ROOT / "en" / source_id

        cn_shape = chapter_numbering_shape(cn_path)
        en_shape = chapter_numbering_shape(en_path)

        if cn_shape != en_shape:
            raise ValidationError(
                f"{source_id}: Chinese/English section-numbering trees differ; "
                f"cn={cn_shape}, en={en_shape}"
            )


def validate_generated_tocs() -> None:
    targets = [
        (ROOT / "README_CN.md", "cn", "./cn/"),
        (ROOT / "README.md", "en", "./en/"),
        (ROOT / "cn" / "README.md", "cn", "./"),
        (ROOT / "en" / "README.md", "en", "./"),
    ]

    for path, edition, prefix in targets:
        expected = render_toc_block(
            edition,
            link_prefix=prefix,
        )
        text = path.read_text(encoding="utf-8")
        if expected not in text:
            raise ValidationError(
                f"{path.relative_to(ROOT)} generated TOC is stale; "
                "run python scripts/update_toc.py"
            )


def main() -> int:
    args = parse_args()
    edition = args.edition
    try:
        entries = manifest_entries(edition)
        edition_dir = ROOT / edition
        chapters = [edition_dir / entry for entry in entries]
        markdown = chapters + SHARED_MARKDOWN + [edition_dir / "README.md"]

        for number, chapter in enumerate(chapters, start=1):
            validate_chapter(chapter, edition, number)

        for path in markdown:
            validate_text_hygiene(path)
            validate_internal_links(path)

        validate_legacy_chinese_manifest()
        validate_part_structure()
        validate_edition_structure_parity()
        validate_generated_tocs()
    except ValidationError as exc:
        print(f"publication QA failed ({edition}): {exc}", file=sys.stderr)
        return 1

    lines = sum(
        len(path.read_text(encoding="utf-8").splitlines())
        for path in chapters
    )
    print(
        f"publication QA passed ({edition}): {len(chapters)} chapters, "
        f"{lines:,} chapter Markdown lines"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
