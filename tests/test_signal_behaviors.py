from __future__ import annotations

import math
import warnings
from collections.abc import Callable, Sequence
from typing import Any

import numpy as np
import pytest
from numpy.typing import NDArray

from iso18571 import ISO18571
from tools import signals
from tools.signals import SignalGenerator


def test_nonzero_identical_signals_score_one_for_all_components() -> None:
    curve = SignalGenerator(64, 0.01).add(signals.sine, frequency=3.0).curve()

    scorer = ISO18571(curve, curve)

    assert scorer.scores["R"] == 1.0
    assert scorer.scores["Z"] == 1.0
    assert scorer.scores["EP"] == 1.0
    assert scorer.scores["EM"] == 1.0
    assert scorer.scores["ES"] == 1.0


def test_constant_offset_expectations_are_score_specific() -> None:
    reference = _curve(np.full(16, 10.0, dtype=np.float64))
    within_inner = _curve(np.full(16, 10.25, dtype=np.float64))
    beyond_outer = _curve(np.full(16, 16.0, dtype=np.float64))

    scorer_inner, inner_warnings = _score_with_warnings(
        reference, within_inner, init_min=0.99
    )
    scorer_outer, outer_warnings = _score_with_warnings(
        reference, beyond_outer, init_min=0.99
    )

    assert inner_warnings == [
        "ISO18571 phase correlation is undefined; using finite fallback rho_e",
        "ISO18571 slope reference denominator is zero; using fallback slope score",
    ]
    assert outer_warnings == inner_warnings
    assert scorer_inner.scores["Z"] == 1.0
    assert scorer_inner.scores["EM"] > 0.0
    assert scorer_inner.scores["ES"] == 1.0
    assert scorer_outer.scores["Z"] == 0.0
    assert scorer_outer.scores["EM"] == 0.0
    assert scorer_outer.scores["ES"] == 1.0
    assert math.isnan(scorer_inner.scores["slope_error"])
    assert math.isnan(scorer_outer.scores["slope_error"])


def test_nonconstant_offset_expectations_are_score_specific() -> None:
    reference_values = (
        SignalGenerator(64, 0.01)
        .add(signals.sine, frequency=2.0, amplitude=1.0)
        .values()
    )
    comparison_values = reference_values + 2.0

    scorer = ISO18571(_curve(reference_values), _curve(comparison_values))

    assert scorer.scores["Z"] == 0.0
    assert scorer.scores["EM"] == 0.0
    assert scorer.scores["EP"] == 1.0
    assert scorer.scores["ES"] == 1.0


@pytest.mark.parametrize(
    ("function", "params"),
    [
        (signals.ramp, {"slope": 1.0}),
        (
            signals.piecewise_ramp,
            {"breakpoints": [0.2, 0.4], "slopes": [1.0, -0.5, 0.75]},
        ),
        (signals.impulse, {"at": 0.25, "amplitude": 2.0}),
        (signals.sparse_spikes, {"count": 5, "amplitude": 2.0}),
        (signals.sine, {"frequency": 3.0}),
        (signals.chirp, {"start_frequency": 1.0, "end_frequency": 8.0}),
        (signals.square_step, {"at": 0.32}),
        (signals.gaussian_noise, {"std": 0.1}),
        (signals.ramp_impulses, {"slope": 0.5, "impulse_times": [0.15, 0.45]}),
        (
            signals.piecewise_discontinuous,
            {"breakpoints": [0.2, 0.5], "values": [0.0, 1.0, -0.25]},
        ),
        (signals.sine_noise, {"frequency": 2.0, "noise_std": 0.03}),
    ],
)
def test_generated_signal_classes_score_with_finite_outputs(
    function: Callable[
        ...,
        NDArray[np.float64] | Sequence[float | int] | float | int,
    ],
    params: dict[str, Any],
) -> None:
    reference = SignalGenerator(96, 0.01, seed=7).add(function, **params).curve()
    comparison = reference.copy()
    comparison[:, 1] = 0.95 * comparison[:, 1] + 0.02

    scorer = ISO18571(reference, comparison)

    for key in ("R", "Z", "EP", "EM", "ES"):
        assert np.isfinite(scorer.scores[key])


@pytest.mark.parametrize("amplitude", [1.0e-150, 1.0e150])
def test_finite_small_and_large_amplitudes_do_not_overflow(amplitude: float) -> None:
    reference_values = (
        SignalGenerator(32, 1.0)
        .add(signals.sine, amplitude=amplitude, frequency=0.05)
        .values()
    )
    comparison_values = reference_values * 0.99

    scorer = ISO18571(_curve(reference_values), _curve(comparison_values))

    for key in ("R", "Z", "EP", "EM", "ES"):
        assert np.isfinite(scorer.scores[key])


def test_tiny_dt_identical_ramp_has_finite_slope_score() -> None:
    values = 10.0 * np.arange(16, dtype=np.float64)
    curve = _curve(values, dt=1.0e-307)

    scorer, messages = _score_with_warnings(curve, curve)

    assert (
        "ISO18571 slope reference denominator is zero; using fallback slope score"
        not in messages
    )
    assert scorer.scores["ES"] == 1.0
    assert scorer.scores["R"] == 1.0
    assert scorer.scores["slope_error"] == 0.0


def test_huge_alternating_identical_signal_has_finite_slope_score() -> None:
    values = np.array([1.0, -1.0] * 8, dtype=np.float64) * np.finfo(np.float64).max
    curve = _curve(values)

    scorer, messages = _score_with_warnings(curve, curve)

    assert (
        "ISO18571 slope reference denominator is zero; using fallback slope score"
        not in messages
    )
    assert scorer.scores["ES"] == 1.0
    assert scorer.scores["R"] == 1.0
    assert scorer.scores["slope_error"] == 0.0


def test_infinite_slope_error_scores_zero_without_nan() -> None:
    reference_values = np.nextafter(0.0, 1.0) * np.arange(16, dtype=np.float64)
    comparison_values = np.arange(16, dtype=np.float64) / 15.0

    scorer, messages = _score_with_warnings(
        _curve(reference_values), _curve(comparison_values)
    )

    assert (
        "ISO18571 slope reference denominator is zero; using fallback slope score"
        not in messages
    )
    assert math.isinf(scorer.scores["slope_error"])
    assert scorer.scores["ES"] == 0.0
    assert np.isfinite(scorer.scores["R"])


def test_smallest_subnormal_identical_signal_scores_one() -> None:
    amplitude = np.nextafter(0.0, 1.0)
    curve = _curve(np.full(16, amplitude, dtype=np.float64))

    scorer, _messages = _score_with_warnings(curve, curve)

    scores = scorer.scores
    for key in ("R", "Z", "EP", "EM", "ES"):
        assert np.isfinite(scores[key])
    assert scores["Z"] == 1.0
    assert scores["R"] == 1.0
    assert scores["corridor_t_norm"] == amplitude
    assert scores["corridor_inner_half_width"] == 0.0
    assert scores["corridor_outer_half_width"] == 0.0


def test_smallest_subnormal_one_ulp_difference_scores_outside_corridor() -> None:
    amplitude = np.nextafter(0.0, 1.0)
    reference = _curve(np.full(16, amplitude, dtype=np.float64))
    comparison = _curve(np.full(16, 2.0 * amplitude, dtype=np.float64))

    scorer, _messages = _score_with_warnings(reference, comparison)

    scores = scorer.scores
    for key in ("R", "Z", "EP", "EM", "ES"):
        assert np.isfinite(scores[key])
    assert scores["Z"] == 0.0


def test_subnormal_corridor_scan_matches_normalized_score() -> None:
    amplitude_unit = np.nextafter(0.0, 1.0)

    for scale in range(1, 33):
        reference_values = np.full(16, scale * amplitude_unit, dtype=np.float64)
        comparison_values = np.full(16, (scale + 1) * amplitude_unit, dtype=np.float64)
        expected = _expected_normalized_corridor_score(
            reference_values, comparison_values
        )

        scorer, _messages = _score_with_warnings(
            _curve(reference_values), _curve(comparison_values)
        )

        scores = scorer.scores
        for key in ("R", "Z", "EP", "EM", "ES"):
            assert np.isfinite(scores[key])
        assert scores["Z"] == pytest.approx(expected)


def test_subnormal_equal_rounded_corridor_widths_score_finitely() -> None:
    amplitude_unit = np.nextafter(0.0, 1.0)
    reference_values = np.full(16, 2.0 * amplitude_unit, dtype=np.float64)
    comparison_values = np.full(16, 3.0 * amplitude_unit, dtype=np.float64)
    expected = _expected_normalized_corridor_score(
        reference_values, comparison_values, a_0=0.49, b_0=0.5
    )

    scorer, _messages = _score_with_warnings(
        _curve(reference_values), _curve(comparison_values), a_0=0.49, b_0=0.5
    )

    scores = scorer.scores
    for key in ("R", "Z", "EP", "EM", "ES"):
        assert np.isfinite(scores[key])
    assert scores["corridor_inner_half_width"] == scores["corridor_outer_half_width"]
    assert scores["corridor_inner_half_width"] > 0.0
    assert scores["Z"] == pytest.approx(expected)


def _curve(values: np.ndarray, *, dt: float = 1.0) -> np.ndarray:
    time = dt * np.arange(values.shape[0], dtype=np.float64)
    return np.column_stack((time, values)).astype(np.float64, copy=False)


def _expected_normalized_corridor_score(
    reference_values: np.ndarray,
    comparison_values: np.ndarray,
    *,
    a_0: float = 0.05,
    b_0: float = 0.5,
    k_z: int = 2,
) -> float:
    t_norm = float(np.max(np.abs(reference_values)))
    if t_norm == 0.0:
        return float(np.mean(reference_values == comparison_values))

    total = 0.0
    for reference_value, comparison_value in zip(
        reference_values, comparison_values, strict=True
    ):
        relative_diff = abs(float(reference_value) - float(comparison_value)) / t_norm
        if relative_diff < a_0:
            c_i = 1.0
        elif not math.isfinite(relative_diff) or relative_diff > b_0:
            c_i = 0.0
        else:
            c_i = ((b_0 - relative_diff) / (b_0 - a_0)) ** k_z
        total += c_i

    return total / float(reference_values.shape[0])


def _score_with_warnings(
    reference: np.ndarray,
    comparison: np.ndarray,
    **params: float,
) -> tuple[ISO18571, list[str]]:
    with warnings.catch_warnings(record=True) as caught:
        warnings.simplefilter("always", RuntimeWarning)
        scorer = ISO18571(reference, comparison, **params)
    messages = []
    for warning in caught:
        assert issubclass(warning.category, RuntimeWarning)
        messages.append(str(warning.message))
    return scorer, messages
