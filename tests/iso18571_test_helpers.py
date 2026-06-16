from __future__ import annotations

import importlib
import warnings
from collections.abc import Callable
from typing import Any

import numpy as np
from iso18571_native._core import (
    DtwLayout,
    ParallelMode,
    ReductionMode,
    SimdLevel,
    _magnitude_ratio_variant_spec,
    _score_components_variant_spec,
    _simd_info,
)

from iso18571_native import (
    magnitude_ratio as _native_magnitude_ratio,
)
from iso18571_native import (
    score_components,
)
from iso18571_native import (
    warp_path as _native_warp_path,
)
from tests.iso18571_annex import AnnexCase, fixed_signal_annex_case, phase_shift_annex_case

NATIVE_SCORE_KEYS = ("Z", "EP", "EM", "ES", "R", "n_eps", "rho_e")
NATIVE_VARIANT_SCORE_KEYS = (
    "Z",
    "EP",
    "EM",
    "ES",
    "R",
    "n_eps",
    "rho_e",
    "reference_start",
    "comparison_start",
    "shift_length",
)
EXPECTED_NUMERIC_WARNING_PATTERNS = (
    "invalid value encountered in divide",
    "invalid value encountered in scalar divide",
)


def native_variant_magnitude_ratio(
    x: np.ndarray,
    y: np.ndarray,
    dtw_layout: DtwLayout,
    parallel_mode: ParallelMode,
    block_size: int,
    simd_level: SimdLevel,
    max_threads: int,
    window_size: float = 0.1,
) -> float:
    return _magnitude_ratio_variant_spec(
        x,
        y,
        window_size,
        dtw_layout,
        parallel_mode,
        block_size,
        simd_level,
        max_threads,
    )


def native_magnitude_ratio(x: np.ndarray, y: np.ndarray, window_size: float) -> float:
    return _native_magnitude_ratio(x, y, window_size)


def native_warp_path(x: np.ndarray, y: np.ndarray, window_size: float) -> np.ndarray:
    return _native_warp_path(x, y, window_size)


def native_score_variant(
    case: AnnexCase,
    dtw_layout: DtwLayout,
    reduction_mode: ReductionMode,
    parallel_mode: ParallelMode,
    block_size: int,
    simd_level: SimdLevel,
    max_threads: int,
) -> dict[str, Any]:
    return _score_components_variant_spec(
        case.reference_curve,
        case.comparison_curve,
        {"dt": case.dt},
        dtw_layout,
        reduction_mode,
        parallel_mode,
        block_size,
        simd_level,
        max_threads,
    )


def public_native_score(case: AnnexCase) -> dict[str, Any]:
    return score_components(case.reference_curve, case.comparison_curve, {"dt": case.dt})


def assert_native_scores_match(
    observed: dict[str, Any],
    expected: dict[str, Any],
    case_name: str,
    variant_label: str,
    keys: tuple[str, ...] = NATIVE_VARIANT_SCORE_KEYS,
) -> None:
    for key in keys:
        np.testing.assert_allclose(
            observed[key],
            expected[key],
            rtol=1e-12,
            atol=1e-12,
            equal_nan=True,
            err_msg=f"{case_name} {variant_label} {key}",
        )


def fixed_case(family: str, n: int) -> AnnexCase:
    return fixed_signal_annex_case(family, n)


def phase_case(family: str, n: int) -> AnnexCase:
    return phase_shift_annex_case(family, n)


def simd_info() -> dict[str, Any]:
    return _simd_info()


def expect_value_error(fn: Callable[[], Any], message: str) -> None:
    try:
        fn()
    except ValueError:
        return
    raise AssertionError(message)


def rating_original_iso(reference_curve: np.ndarray, comparison_curve: np.ndarray) -> Any:
    module = importlib.import_module("rating_original")
    return module.ISO18571(reference_curve, comparison_curve)


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
