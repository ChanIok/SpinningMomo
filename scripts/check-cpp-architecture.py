#!/usr/bin/env python3
"""Validate the Headers/PCH architecture without compiling the project."""

from __future__ import annotations

import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"
TESTS = ROOT / "tests"
CPP_SUFFIXES = {".hpp", ".cpp"}

FORBIDDEN_TEXT = {
    r"\b(?:Core|Features|UI|Utils|Extensions|Vendor)::": "旧的大驼峰命名空间",
    r"\b(?:State|Types|UseCase)::": "已移除的职责命名空间",
    r"^\s*export\s+module\b": "C++ 命名模块声明",
    r"^\s*import\s+(?:std|[A-Za-z_])": "C++ 模块导入",
    r"\bbuild\.c\+\+\.modules\b": "Xmake 模块策略",
    r"\bnamespace\s*\{": "匿名命名空间",
    r"\b(?:web_view|d3_d|power_shell)\b": "非规范的复合命名空间拼写",
}

EXTERNAL_INCLUDE = re.compile(r"^\s*#include\s*<[^>]+>")

NAMESPACE_DECLARATION = re.compile(
    r"^\s*namespace\s+([A-Za-z_][A-Za-z0-9_:]*)\s*\{", re.MULTILINE
)

TYPE_DECLARATION = re.compile(
    r"\b(?:struct|class|enum(?:\s+class)?)\s+([A-Za-z_][A-Za-z0-9_]*)"
)

LOWERCASE_TYPE_EXCEPTIONS = {
    "promise_type",
    "shared_state",
    "timeout_error",
    "ui_delay",
    "ui_task",
}

REQUIRED_SYMBOL_INCLUDES = {
    "utils::hash::": "utils/hash/xxhash.hpp",
}

CXX_KEYWORDS = {
    "alignas", "alignof", "and", "and_eq", "asm", "atomic_cancel", "atomic_commit",
    "atomic_noexcept", "auto", "bitand", "bitor", "bool", "break", "case", "catch",
    "char", "char8_t", "char16_t", "char32_t", "class", "compl", "concept", "const",
    "consteval", "constexpr", "constinit", "const_cast", "continue", "co_await",
    "co_return", "co_yield", "decltype", "default", "delete", "do", "double",
    "dynamic_cast", "else", "enum", "explicit", "export", "extern", "false", "float",
    "for", "friend", "goto", "if", "inline", "int", "long", "mutable", "namespace",
    "new", "noexcept", "not", "not_eq", "nullptr", "operator", "or", "or_eq", "private",
    "protected", "public", "reflexpr", "register", "reinterpret_cast", "requires",
    "return", "short", "signed", "sizeof", "static", "static_assert", "static_cast",
    "struct", "switch", "synchronized", "template", "this", "thread_local", "throw",
    "true", "try", "typedef", "typeid", "typename", "union", "unsigned", "using",
    "virtual", "void", "volatile", "wchar_t", "while", "xor", "xor_eq",
}


def cpp_files(directory: Path) -> list[Path]:
    if not directory.exists():
        return []
    return sorted(
        path
        for path in directory.rglob("*")
        if path.is_file() and path.suffix.lower() in CPP_SUFFIXES
    )


def report(errors: list[str], path: Path, line: int, message: str) -> None:
    errors.append(f"{path.relative_to(ROOT)}:{line}: {message}")


def validate_file(path: Path, errors: list[str]) -> None:
    text = path.read_text(encoding="utf-8-sig", errors="strict")
    lines = text.splitlines()
    vendor_root = SRC / "vendor"
    is_vendor_facade = vendor_root in path.parents

    if not is_vendor_facade and '#include "vendor/std.hpp"' not in text:
        report(errors, path, 1, '缺少显式 #include "vendor/std.hpp"')
    if is_vendor_facade:
        facade_include = path.relative_to(SRC).as_posix()
        if f'#include "{facade_include}"' in text:
            report(errors, path, 1, "vendor 门面不能包含自身")

    for symbol, required_header in REQUIRED_SYMBOL_INCLUDES.items():
        if symbol in text and f'#include "{required_header}"' not in text:
            report(errors, path, 1, f"使用 {symbol} 时必须包含 {required_header}")

    for pattern, description in FORBIDDEN_TEXT.items():
        regex = re.compile(pattern, re.MULTILINE)
        for match in regex.finditer(text):
            line = text.count("\n", 0, match.start()) + 1
            report(errors, path, line, f"仍包含{description}: {match.group(0).strip()}")

    for match in NAMESPACE_DECLARATION.finditer(text):
        namespace = match.group(1)
        bad_parts = [
            part
            for part in namespace.split("::")
            if part and (part != part.lower() or re.search(r"[A-Z]", part))
        ]
        if bad_parts:
            line = text.count("\n", 0, match.start()) + 1
            report(errors, path, line, f"命名空间必须使用 lower_snake_case: {namespace}")
        keyword_parts = [part for part in namespace.split("::") if part in CXX_KEYWORDS]
        if keyword_parts:
            line = text.count("\n", 0, match.start()) + 1
            report(errors, path, line, f"命名空间使用了 C++ 关键字: {namespace}")

    for match in TYPE_DECLARATION.finditer(text):
        type_name = match.group(1)
        if type_name[0].islower() and type_name not in LOWERCASE_TYPE_EXCEPTIONS:
            line = text.count("\n", 0, match.start()) + 1
            report(errors, path, line, f"项目类型必须使用 PascalCase: {type_name}")

    if not is_vendor_facade:
        for line_number, line in enumerate(lines, start=1):
            if EXTERNAL_INCLUDE.match(line):
                report(
                    errors,
                    path,
                    line_number,
                    f"外部头应通过精确的 vendor 门面引入: {line.strip()}",
                )


def main() -> int:
    errors: list[str] = []

    module_interfaces = sorted(SRC.rglob("*.ixx")) + sorted(TESTS.rglob("*.ixx"))
    for path in module_interfaces:
        report(errors, path, 1, "仓库中仍存在 .ixx 模块接口")

    for path in cpp_files(SRC) + cpp_files(TESTS):
        validate_file(path, errors)

    xmake_text = (ROOT / "xmake.lua").read_text(encoding="utf-8-sig")
    for pattern, description in FORBIDDEN_TEXT.items():
        if re.search(pattern, xmake_text, re.MULTILINE):
            errors.append(f"xmake.lua: 仍包含{description}")

    if errors:
        print("C++ architecture check failed:")
        for error in errors:
            print(f"  {error}")
        return 1

    checked = len(cpp_files(SRC)) + len(cpp_files(TESTS))
    print(f"C++ architecture check passed ({checked} files).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
