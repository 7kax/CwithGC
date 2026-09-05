#!/usr/bin/env python3

import sys
from pathlib import Path


TEXT_SUFFIXES = {
    ".c",
    ".cc",
    ".cmake",
    ".cpp",
    ".h",
    ".hh",
    ".hpp",
    ".json",
    ".md",
    ".py",
    ".toml",
    ".txt",
    ".yaml",
    ".yml",
}
EXCLUDED_DIRECTORIES = {".git", "build"}


def is_cjk_character(character: str) -> bool:
    code_point = ord(character)
    return (
        0x3400 <= code_point <= 0x4DBF
        or 0x4E00 <= code_point <= 0x9FFF
        or 0xF900 <= code_point <= 0xFAFF
    )


def source_files(root: Path):
    for path in root.rglob("*"):
        if not path.is_file() or (
            path.suffix.lower() not in TEXT_SUFFIXES and path.name != ".clang-format"
        ):
            continue
        if any(part in EXCLUDED_DIRECTORIES for part in path.relative_to(root).parts):
            continue
        yield path


def main() -> int:
    root = Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
    violations = []

    for path in source_files(root):
        for line_number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            if any(is_cjk_character(character) for character in line):
                violations.append(f"{path.relative_to(root)}:{line_number}: {line.strip()}")

    if not violations:
        return 0

    print("CJK characters found in project text:", file=sys.stderr)
    print("\n".join(violations), file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
