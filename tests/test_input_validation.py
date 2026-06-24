from __future__ import annotations

import math
import warnings
from collections.abc import Iterable, Mapping
from typing import Any, Final, cast

import numpy as np
import pytest

from iso18571 import ISO18571, ScoreComponents


ScoreValue = float | int
FloatCurve = np.ndarray[tuple[int, int], np.dtype[np.float64]]
SCORE_FIELD_TOLERANCE: Final = 1.0e-12


def test_shorter_than_nine_samples_is_rejected_by_slope_rating() -> None:
    curve = _curve(np.arange(8, dtype=np.float64))

    with pytest.raises(ValueError, match="at least 9 samples"):
        ISO18571(curve, curve)


@pytest.mark.parametrize(
    "bad_shape", [np.arange(20.0), np.zeros((10, 3), dtype=np.float64)]
)
def test_curve_shape_contract_requires_n_by_two_array(bad_shape: np.ndarray) -> None:
    good = _curve(np.arange(10, dtype=np.float64))

    with pytest.raises(ValueError, match="2D array|shape"):
        ISO18571(bad_shape, good)


def test_masked_arrays_are_rejected_before_materialization() -> None:
    curve = _curve(np.arange(10, dtype=np.float64))
    masked = np.ma.array(curve, mask=np.zeros(curve.shape, dtype=bool))

    with pytest.raises(ValueError, match="masked array"):
        ISO18571(masked, curve)


@pytest.mark.parametrize("dtype", [object, bool, np.str_])
def test_unsupported_data_types_are_rejected(dtype: Any) -> None:
    curve = _curve(np.arange(10, dtype=np.float64)).astype(dtype)

    with pytest.raises(ValueError, match="unsupported dtype"):
        ISO18571(curve, curve)


@pytest.mark.parametrize("bad_value", [math.nan, math.inf, -math.inf])
def test_nan_and_inf_values_are_rejected(bad_value: float) -> None:
    reference = _curve(np.arange(10, dtype=np.float64))
    comparison = reference.copy()
    comparison[3, 1] = bad_value

    with pytest.raises(ValueError, match="must be finite"):
        ISO18571(reference, comparison)


def test_non_uniform_time_values_are_rejected() -> None:
    reference = _curve(np.arange(10, dtype=np.float64))
    comparison = reference.copy()
    comparison[5, 0] += 0.25

    with pytest.raises(ValueError, match="constant interval"):
        ISO18571(reference, comparison)


@pytest.mark.parametrize("dt", [1.0e-6, 1.0e6])
def test_uniform_small_and_large_dt_values_are_accepted(dt: float) -> None:
    values = np.array([1.0, 3.0, 2.0, 5.0, 4.0, 7.0, 3.0, 6.0, 5.0, 8.0])
    time = np.arange(values.shape[0], dtype=np.float64) * dt
    reference = np.column_stack((time, values))
    comparison = np.column_stack((time, values + 0.1))

    scorer = ISO18571(reference, comparison)

    assert scorer.dt == dt
    for key in ("R", "Z", "EP", "EM", "ES"):
        assert np.isfinite(scorer.scores[key])


def test_non_native_byte_order_is_rejected() -> None:
    byte_order = ">" if np.little_endian else "<"
    curve = _curve(np.arange(10, dtype=np.float64)).astype(np.dtype(f"{byte_order}f8"))

    with pytest.raises(ValueError, match="native byte order"):
        ISO18571(curve, curve)


def test_strided_arrays_are_accepted_and_match_materialized_arrays() -> None:
    values = np.array([0.0, 2.0, 1.0, 4.0, 3.0, 6.0, 2.0, 5.0, 4.0, 7.0, 5.0, 8.0])
    base_reference = _curve(np.repeat(values, 2))
    base_comparison = _curve(np.repeat(values + 0.25, 2))
    reference = base_reference[::2]
    comparison = base_comparison[::2]

    strided = ISO18571(reference, comparison).scores
    materialized = ISO18571(
        np.asarray(reference, dtype=np.float64),
        np.asarray(comparison, dtype=np.float64),
    ).scores

    _assert_scores_match(strided, materialized)


@pytest.mark.parametrize(
    "dtype",
    [
        np.uint8,
        np.uint16,
        np.uint32,
        np.uint64,
        np.int8,
        np.int16,
        np.int32,
        np.int64,
        np.float16,
        np.float32,
        np.float64,
    ],
)
def test_accepted_integer_and_float_dtypes_match_float64_materialization(
    dtype: Any,
) -> None:
    reference, comparison = _parity_curves(dtype)

    actual = ISO18571(reference, comparison).scores
    expected = ISO18571(
        reference.astype(np.float64), comparison.astype(np.float64)
    ).scores

    _assert_scores_match(actual, expected)


@pytest.mark.parametrize("dtype", [np.complex64, np.complex128, np.clongdouble])
def test_zero_imaginary_complex_dtypes_ignore_imaginary_and_match_float64(
    dtype: Any,
) -> None:
    reference, comparison = _parity_curves(dtype)

    actual = ISO18571(reference, comparison).scores
    expected = ISO18571(
        reference.real.astype(np.float64), comparison.real.astype(np.float64)
    ).scores

    _assert_scores_match(actual, expected)


@pytest.mark.parametrize("dtype", [np.complex64, np.complex128, np.clongdouble])
def test_nonzero_imaginary_complex_dtypes_are_rejected(dtype: Any) -> None:
    reference, comparison = _parity_curves(dtype)
    comparison = comparison.copy()
    comparison[3, 1] += dtype(1j)

    with pytest.raises(ValueError, match="zero imaginary"):
        ISO18571(reference, comparison)


def test_zero_signals_emit_expected_fallback_warnings_and_fields() -> None:
    curve = _curve(np.zeros(10, dtype=np.float64))

    scorer, messages = _score_with_warnings(curve, curve)

    assert messages == [
        "ISO18571 phase correlation is undefined; using finite fallback rho_e",
        "ISO18571 magnitude reference denominator is zero; using fallback magnitude score",
        "ISO18571 slope reference denominator is zero; using fallback slope score",
    ]
    assert scorer.scores["R"] == 1.0
    assert scorer.scores["Z"] == 1.0
    assert scorer.scores["EP"] == 1.0
    assert scorer.scores["EM"] == 1.0
    assert scorer.scores["ES"] == 1.0
    assert scorer.scores["magnitude_numerator"] == 0.0
    assert scorer.scores["magnitude_denominator"] == 0.0
    assert math.isnan(scorer.scores["magnitude_error"])
    assert scorer.scores["slope_numerator"] == 0.0
    assert scorer.scores["slope_denominator"] == 0.0
    assert math.isnan(scorer.scores["slope_error"])


def test_constant_identical_signals_emit_expected_fallback_warnings_and_fields() -> (
    None
):
    curve = _curve(np.full(10, 2.0, dtype=np.float64))

    scorer, messages = _score_with_warnings(curve, curve)

    assert messages == [
        "ISO18571 phase correlation is undefined; using finite fallback rho_e",
        "ISO18571 slope reference denominator is zero; using fallback slope score",
    ]
    assert scorer.scores["R"] == 1.0
    assert scorer.scores["Z"] == 1.0
    assert scorer.scores["EP"] == 1.0
    assert scorer.scores["EM"] == 1.0
    assert scorer.scores["ES"] == 1.0
    assert scorer.scores["magnitude_denominator"] > 0.0
    assert scorer.scores["magnitude_error"] == 0.0
    assert scorer.scores["slope_numerator"] == 0.0
    assert scorer.scores["slope_denominator"] == 0.0
    assert math.isnan(scorer.scores["slope_error"])


def _curve(values: np.ndarray) -> FloatCurve:
    time = np.arange(values.shape[0], dtype=np.float64)
    return np.column_stack((time, values)).astype(np.float64, copy=False)


def _parity_curves(dtype: Any) -> tuple[np.ndarray, np.ndarray]:
    reference_values = np.array([1, 3, 2, 5, 4, 7, 3, 6, 5, 8, 6, 9], dtype=np.float64)
    comparison_values = np.array(
        [2, 3, 4, 5, 4, 8, 4, 6, 7, 8, 7, 10], dtype=np.float64
    )
    reference = _curve(reference_values).astype(dtype)
    comparison = _curve(comparison_values).astype(dtype)
    return reference, comparison


def _score_with_warnings(
    curve: FloatCurve, comparison: FloatCurve
) -> tuple[ISO18571, list[str]]:
    with warnings.catch_warnings(record=True) as caught:
        warnings.simplefilter("always", RuntimeWarning)
        scorer = ISO18571(curve, comparison)
    messages = []
    for warning in caught:
        assert issubclass(warning.category, RuntimeWarning)
        messages.append(str(warning.message))
    return scorer, messages


def _assert_scores_match(
    actual: ScoreComponents,
    expected: ScoreComponents,
    *,
    keys: Iterable[str] | None = None,
) -> None:
    actual_scores = cast(Mapping[str, ScoreValue], actual)
    expected_scores = cast(Mapping[str, ScoreValue], expected)
    score_keys = tuple(actual_scores) if keys is None else tuple(keys)
    for key in score_keys:
        actual_value = float(actual_scores[key])
        expected_value = float(expected_scores[key])
        if math.isnan(expected_value):
            assert math.isnan(actual_value)
        else:
            assert abs(actual_value - expected_value) < SCORE_FIELD_TOLERANCE
