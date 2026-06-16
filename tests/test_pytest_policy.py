from __future__ import annotations

import re
from pathlib import Path

FORBIDDEN_PATTERNS = (
    "pytest." + "skip",
    "pytest." + "fail",
    "pytest." + "importorskip",
    "pytest." + "raises",
    "pytest." + "xfail",
    "@pytest.mark." + "skip",
    "@pytest.mark." + "xfail",
)
INLINE_IMPORT_PATTERN = re.compile(r"^[ \t]+(import|from)\s", re.MULTILINE)
REMOVED_NATIVE_STRING_HOOKS = (
    "_score_components_" + "variant(",
    "_magnitude_ratio_" + "variant(",
)
INLINE_IMPORT_SCAN_ROOTS = ("iso18571", "iso18571_native", "tests", "tools", "main.py")


def test_tests_use_assert_raise_and_marker_deselection_only() -> None:
    root = Path(__file__).resolve().parent
    violations = []
    for path in sorted(root.rglob("*.py")):
        text = path.read_text()
        for pattern in FORBIDDEN_PATTERNS:
            if pattern in text:
                violations.append(f"{path.relative_to(root.parent)} contains {pattern}")

    assert not violations, "\n".join(violations)


def test_project_python_does_not_use_inline_imports() -> None:
    root = Path(__file__).resolve().parents[1]
    violations = []
    for scan_root in INLINE_IMPORT_SCAN_ROOTS:
        path = root / scan_root
        candidates = [path] if path.is_file() else sorted(path.rglob("*.py"))
        for candidate in candidates:
            text = candidate.read_text()
            if INLINE_IMPORT_PATTERN.search(text):
                violations.append(f"{candidate.relative_to(root)} contains an inline import")

    assert not violations, "\n".join(violations)


def test_native_string_variant_hooks_are_not_used_by_tests_or_tools() -> None:
    root = Path(__file__).resolve().parents[1]
    search_roots = (root / "tests", root / "tools")
    violations = []
    for search_root in search_roots:
        for path in sorted(search_root.rglob("*.py")):
            text = path.read_text()
            for pattern in REMOVED_NATIVE_STRING_HOOKS:
                if pattern in text:
                    violations.append(f"{path.relative_to(root)} contains {pattern}")

    assert not violations, "\n".join(violations)
