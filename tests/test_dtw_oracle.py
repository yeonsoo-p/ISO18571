from __future__ import annotations

from typing import Final

import numpy as np
import pytest

from iso18571 import ISO18571
from tests.oracles import DtwFields, dtw_magnitude_fields


FloatArray = np.ndarray[tuple[int], np.dtype[np.float64]]
FIELD_TOLERANCE: Final = 1.0e-12


@pytest.mark.parametrize(
    ("reference", "comparison", "variant"),
    [
        (
            np.array([5.0, 5.0, 5.0, -4.0, 2.0, 2.0, 4.0, 1.0, -4.0, -2.0, -2.0]),
            np.array([-1.0, 4.0, -3.0, 3.0, -1.0, 0.0, 1.0, -1.0, -4.0, 1.0, -3.0]),
            "absolute local cost",
        ),
        (
            np.array(
                [
                    -2.0,
                    0.0,
                    0.0,
                    -1.0,
                    -5.0,
                    -4.0,
                    4.0,
                    3.0,
                    1.0,
                    5.0,
                    -5.0,
                    -5.0,
                    -2.0,
                    -2.0,
                    2.0,
                    3.0,
                    2.0,
                    -1.0,
                    0.0,
                    5.0,
                ]
            ),
            np.array(
                [
                    -3.0,
                    -3.0,
                    0.0,
                    1.0,
                    0.0,
                    0.0,
                    -1.0,
                    -2.0,
                    0.0,
                    -3.0,
                    -3.0,
                    1.0,
                    4.0,
                    -2.0,
                    2.0,
                    3.0,
                    -4.0,
                    -1.0,
                    -4.0,
                    -4.0,
                ]
            ),
            "full window",
        ),
        (
            np.array([1.0, -4.0, 4.0, -1.0, 4.0, -5.0, -5.0, 3.0, 1.0, -3.0]),
            np.array([2.0, -1.0, -2.0, -2.0, 5.0, 0.0, -1.0, 5.0, -4.0, 1.0]),
            "inclusive boundary",
        ),
        (
            np.array([2.0, -5.0, 4.0, 5.0, -4.0, 1.0, -5.0, 2.0, 2.0, 5.0, -5.0]),
            np.array([-1.0, 2.0, -4.0, 5.0, 4.0, -5.0, 1.0, 2.0, -3.0, -2.0, -1.0]),
            "diagonal-first tie",
        ),
    ],
)
def test_native_magnitude_matches_clean_room_dtw_contract(
    reference: FloatArray,
    comparison: FloatArray,
    variant: str,
) -> None:
    expected = dtw_magnitude_fields(comparison, reference)
    actual = ISO18571(_curve(reference), _curve(comparison), init_min=0.99).scores

    assert (
        abs(actual["magnitude_numerator"] - expected["magnitude_numerator"])
        < FIELD_TOLERANCE
    )
    assert (
        abs(actual["magnitude_denominator"] - expected["magnitude_denominator"])
        < FIELD_TOLERANCE
    )
    assert (
        abs(actual["magnitude_error"] - expected["magnitude_error"]) < FIELD_TOLERANCE
    )

    alternative = _alternative_fields(reference, comparison, variant)
    assert (
        abs(alternative["magnitude_numerator"] - expected["magnitude_numerator"])
        > FIELD_TOLERANCE
        or abs(alternative["magnitude_denominator"] - expected["magnitude_denominator"])
        > FIELD_TOLERANCE
        or abs(alternative["magnitude_error"] - expected["magnitude_error"])
        > FIELD_TOLERANCE
    )


def _alternative_fields(
    reference: FloatArray, comparison: FloatArray, variant: str
) -> DtwFields:
    if variant == "absolute local cost":
        return dtw_magnitude_fields(comparison, reference, squared_local_cost=False)
    if variant == "full window":
        return dtw_magnitude_fields(comparison, reference, window_size=1.0)
    if variant == "inclusive boundary":
        return dtw_magnitude_fields(comparison, reference, inclusive_boundary=True)
    if variant == "diagonal-first tie":
        return dtw_magnitude_fields(
            comparison,
            reference,
            tie_order=("diagonal", "horizontal", "vertical"),
        )
    raise AssertionError(f"unknown DTW variant {variant!r}")


def _curve(values: FloatArray) -> np.ndarray[tuple[int, int], np.dtype[np.float64]]:
    time = np.arange(values.shape[0], dtype=np.float64)
    return np.column_stack((time, values)).astype(np.float64, copy=False)
