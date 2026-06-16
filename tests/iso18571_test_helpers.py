from __future__ import annotations

import importlib.util
import warnings
from collections.abc import Callable
from pathlib import Path
from types import ModuleType
from typing import Any

import numpy as np

EXPECTED_NUMERIC_WARNING_PATTERNS = (
    "invalid value encountered in divide",
    "invalid value encountered in scalar divide",
)
RATING_ORIGINAL_PATH = Path(__file__).resolve().parents[1] / "ref" / "rating_original.py"
_RATING_ORIGINAL_MODULE: ModuleType | None = None


def rating_original_iso(reference_curve: np.ndarray, comparison_curve: np.ndarray) -> Any:
    module = rating_original_module()
    return module.ISO18571(reference_curve, comparison_curve)


def rating_original_module() -> ModuleType:
    global _RATING_ORIGINAL_MODULE

    if _RATING_ORIGINAL_MODULE is not None:
        return _RATING_ORIGINAL_MODULE
    if not RATING_ORIGINAL_PATH.exists():
        raise AssertionError(f"rating_original oracle is missing: {RATING_ORIGINAL_PATH}")

    spec = importlib.util.spec_from_file_location("rating_original_ref", RATING_ORIGINAL_PATH)
    if spec is None or spec.loader is None:
        raise AssertionError(f"rating_original oracle cannot be loaded: {RATING_ORIGINAL_PATH}")

    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    _RATING_ORIGINAL_MODULE = module
    return module


def assert_only_expected_numeric_warnings(records: list[warnings.WarningMessage], context: str) -> None:
    for record in records:
        message = str(record.message)
        expected_message = any(pattern in message for pattern in EXPECTED_NUMERIC_WARNING_PATTERNS)
        assert record.category is RuntimeWarning and expected_message, (
            f"{context}: unexpected warning {record.category.__name__}: {message}"
        )


def score_with_expected_numeric_warnings(
    fn: Callable[[], dict[str, float | int]], context: str
) -> dict[str, float | int]:
    with warnings.catch_warnings(record=True) as records:
        warnings.simplefilter("always", RuntimeWarning)
        scores = fn()
    assert_only_expected_numeric_warnings(records, context)
    return scores
