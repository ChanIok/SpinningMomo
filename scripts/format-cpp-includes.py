#!/usr/bin/env python3
"""Normalize the leading include section of C++ source files."""

from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"
TESTS = ROOT / "tests"
CPP_SUFFIXES = {".h", ".hpp", ".cpp"}

INCLUDE_LINE = re.compile(
    r'^\s*#\s*include\s*(?P<open>[<"])(?P<target>[^>"]+)[>"]'
    r"(?P<comment>\s*(?://.*|/\*.*\*/))?\s*$"
)


@dataclass(frozen=True)
class Include:
    target: str
    opening: str
    comment: str

    @property
    def closing(self) -> str:
        return ">" if self.opening == "<" else '"'

    def render(self) -> str:
        line = f"#include {self.opening}{self.target}{self.closing}"
        if self.comment:
            line += f"  {self.comment}"
        return line


def default_files() -> list[Path]:
    files: list[Path] = []
    for directory in (SRC, TESTS):
        if not directory.exists():
            continue
        files.extend(
            path
            for path in directory.rglob("*")
            if path.is_file() and path.suffix.lower() in CPP_SUFFIXES
        )
    return sorted(files)


def should_skip(path: Path) -> bool:
    try:
        relative = path.resolve().relative_to(ROOT)
    except ValueError:
        return True

    parts = relative.parts
    return (
        not parts
        or parts[0] not in {"src", "tests"}
        or (parts[0] == "src" and len(parts) > 1 and parts[1] == "vendor")
        or "generated" in parts
    )


def matching_header(path: Path) -> str | None:
    if path.suffix.lower() != ".cpp":
        return None
    try:
        relative = path.resolve().relative_to(SRC)
    except ValueError:
        return None
    return relative.with_suffix(".hpp").as_posix()


def include_group(include: Include, own_header: str | None) -> int:
    if own_header is not None and include.target == own_header:
        return 0
    if include.target == "vendor/std.hpp":
        return 1
    if include.target.startswith("vendor/"):
        return 2
    return 3


def format_include_section(path: Path, text: str) -> str:
    lines = text.splitlines()
    start = next((index for index, line in enumerate(lines) if INCLUDE_LINE.match(line)), None)
    if start is None:
        return text

    end = start
    includes: list[Include] = []
    while end < len(lines):
        line = lines[end]
        match = INCLUDE_LINE.match(line)
        if match:
            includes.append(
                Include(
                    target=match.group("target"),
                    opening=match.group("open"),
                    comment=(match.group("comment") or "").strip(),
                )
            )
            end += 1
            continue
        if not line.strip():
            end += 1
            continue
        break

    if not includes:
        return text

    own_header = matching_header(path)
    unique: dict[tuple[str, str], Include] = {}
    for include in includes:
        unique.setdefault((include.opening, include.target), include)

    groups: list[list[Include]] = [[], [], [], []]
    for include in unique.values():
        groups[include_group(include, own_header)].append(include)

    rendered_groups: list[str] = []
    for group in groups:
        if not group:
            continue
        group.sort(key=lambda include: (include.target.casefold(), include.target))
        rendered_groups.append("\n".join(include.render() for include in group))

    replacement = "\n\n".join(rendered_groups)
    before = lines[:start]
    last_content = next(
        (index for index in range(len(before) - 1, -1, -1) if before[index].strip()),
        None,
    )
    if last_content is not None and before[last_content].strip() == "#pragma once":
        before = before[: last_content + 1] + [""]

    after = lines[end:]
    if after:
        replacement += "\n"
        if after[0].strip():
            replacement += "\n"

    updated_lines = before + replacement.splitlines() + after
    updated = "\n".join(updated_lines)
    if text.endswith(("\n", "\r")):
        updated += "\n"
    return updated


def read_text(path: Path) -> tuple[str, bool]:
    data = path.read_bytes()
    has_bom = data.startswith(b"\xef\xbb\xbf")
    if has_bom:
        data = data[3:]
    return data.decode("utf-8"), has_bom


def write_text(path: Path, text: str, has_bom: bool) -> None:
    data = text.encode("utf-8")
    if has_bom:
        data = b"\xef\xbb\xbf" + data
    path.write_bytes(data)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="report files that need formatting")
    parser.add_argument("files", nargs="*", type=Path)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    paths = args.files or default_files()
    changed: list[Path] = []

    for candidate in paths:
        path = candidate if candidate.is_absolute() else ROOT / candidate
        if not path.is_file() or path.suffix.lower() not in CPP_SUFFIXES or should_skip(path):
            continue

        text, has_bom = read_text(path)
        updated = format_include_section(path, text)
        if updated == text:
            continue
        changed.append(path)
        if not args.check:
            write_text(path, updated, has_bom)

    if args.check and changed:
        print("C++ include formatting required:")
        for path in changed:
            print(f"  {path.relative_to(ROOT)}")
        return 1

    if not args.check:
        print(f"Formatted C++ includes in {len(changed)} file(s).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
