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


def test_tests_use_assert_raise_and_marker_deselection_only() -> None:
    root = Path(__file__).resolve().parent
    violations = []
    for path in sorted(root.rglob("*.py")):
        text = path.read_text()
        for pattern in FORBIDDEN_PATTERNS:
            if pattern in text:
                violations.append(f"{path.relative_to(root.parent)} contains {pattern}")

    assert not violations, "\n".join(violations)


def test_test_modules_do_not_use_inline_imports() -> None:
    root = Path(__file__).resolve().parent
    violations = []
    for path in sorted(root.glob("test_*.py")):
        text = path.read_text()
        if INLINE_IMPORT_PATTERN.search(text):
            violations.append(f"{path.relative_to(root.parent)} contains an inline import")

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
