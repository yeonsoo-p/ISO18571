from __future__ import annotations

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


def test_tests_use_assert_raise_and_marker_deselection_only() -> None:
    root = Path(__file__).resolve().parent
    violations = []
    for path in sorted(root.rglob("*.py")):
        text = path.read_text()
        for pattern in FORBIDDEN_PATTERNS:
            if pattern in text:
                violations.append(f"{path.relative_to(root.parent)} contains {pattern}")

    assert not violations, "\n".join(violations)
