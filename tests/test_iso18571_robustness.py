from __future__ import annotations

import math
import warnings
from collections.abc import Sequence
from importlib.metadata import version
from typing import Final

import numpy as np
from numpy.typing import NDArray

import iso18571
import iso18571._core as native_core
from iso18571 import ISO18571
from tests.iso18571_annex import AnnexCase
from tests.iso18571_test_helpers import (
    MAGNITUDE_ZERO_WARNING,
    PHASE_CLAMP_WARNING,
    PHASE_UNDEFINED_WARNING,
    SLOPE_ZERO_WARNING,
)

SCORE_KEYS: Final = ("Z", "EP", "EM", "ES", "R")
PARITY_ATOL = 0.001
ROBUSTNESS_EDGE_LENGTHS = (9, 10, 17, 64, 129, 512, 1430)


def curve_from_values(
    values: NDArray[np.float64], dt: float = 0.0001
) -> NDArray[np.float64]:
    time = np.arange(values.shape[0], dtype=np.float64) * dt
    return np.column_stack((time, values)).astype(np.float64, copy=False)


def float32_curve_from_values(
    values: NDArray[np.float64], dt: float = 0.0001
) -> NDArray[np.float32]:
    time = np.arange(values.shape[0], dtype=np.float32) * np.float32(dt)
    curve = np.column_stack((time, values.astype(np.float32))).astype(
        np.float32, copy=False
    )
    return curve


def sine_values(n: int) -> NDArray[np.float64]:
    t = np.linspace(0.0, 1.0, n, endpoint=False, dtype=np.float64)
    return np.sin(2.0 * np.pi * 5.0 * t)


def noisy_ramp_shifted_pair() -> tuple[NDArray[np.float64], NDArray[np.float64]]:
    n = 9
    rng = np.random.default_rng(0)
    reference_values = np.linspace(-1.0, 1.0, n, dtype=np.float64)
    reference_values = reference_values + rng.normal(0.0, 0.02, n)
    comparison_values = np.roll(reference_values, 1)
    return curve_from_values(reference_values), curve_from_values(comparison_values)


def constant_offset_pair(n: int) -> tuple[NDArray[np.float64], NDArray[np.float64]]:
    reference_values = np.full(n, 2.0, dtype=np.float64)
    comparison_values = np.full(n, 2.1, dtype=np.float64)
    return curve_from_values(reference_values), curve_from_values(comparison_values)


def impulse_pair(n: int) -> tuple[NDArray[np.float64], NDArray[np.float64]]:
    reference_values = np.zeros(n, dtype=np.float64)
    comparison_values = np.zeros(n, dtype=np.float64)
    reference_values[n // 3] = 1.0
    comparison_values[min(n - 1, n // 3 + max(1, n // 80))] = 0.92
    return curve_from_values(reference_values), curve_from_values(comparison_values)


def sparse_spikes_pair(n: int) -> tuple[NDArray[np.float64], NDArray[np.float64]]:
    reference_values = np.zeros(n, dtype=np.float64)
    comparison_values = np.zeros(n, dtype=np.float64)
    indices = np.linspace(1, n - 2, min(9, max(2, n // 16)), dtype=np.int64)
    reference_values[indices] = np.linspace(0.4, 1.2, indices.shape[0])
    comparison_values[np.minimum(n - 1, indices + 1)] = reference_values[indices] * 0.95
    return curve_from_values(reference_values), curve_from_values(comparison_values)


def score_with_warnings(
    reference_curve: NDArray[np.float32 | np.float64],
    comparison_curve: NDArray[np.float32 | np.float64],
) -> tuple[ISO18571, list[str]]:
    with warnings.catch_warnings(record=True) as records:
        warnings.simplefilter("always", RuntimeWarning)
        iso = ISO18571(reference_curve, comparison_curve)
    messages = []
    for record in records:
        assert record.category is RuntimeWarning, (
            f"unexpected warning category {record.category.__name__}"
        )
        messages.append(str(record.message))
    return iso, messages


def assert_scores_close(
    iso: ISO18571, expected: dict[str, float], context: str
) -> None:
    for key in SCORE_KEYS:
        value = iso.scores[key]
        assert math.isfinite(float(value)), f"{context} {key} is non-finite"
        np.testing.assert_allclose(
            value,
            expected[key],
            rtol=0.0,
            atol=PARITY_ATOL,
            err_msg=f"{context} {key}",
        )


def test_native_surface_is_small_and_accepts_numpy_arrays(
    generated_annex_cases: Sequence[AnnexCase],
) -> None:
    case = next(
        case for case in generated_annex_cases if "sine_amp_offset" in case.name
    )
    iso = ISO18571(case.reference_curve, case.comparison_curve)
    scores = iso.scores
    assert set(scores) == {
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
    }
    assert iso18571.__all__ == ["ISO18571", "backend_info", "ScoreComponents"]
    assert not hasattr(iso18571, "score_components")
    assert not hasattr(iso18571, "magnitude_ratio")
    assert not hasattr(iso18571, "warp_path")
    assert not hasattr(iso, "shifted_reference_curve")
    assert not hasattr(iso, "shifted_comparison_curve")
    assert not hasattr(native_core, "score_components")
    assert not hasattr(native_core, "magnitude_ratio")
    assert not hasattr(native_core, "warp_path")
    info = iso18571.backend_info()
    assert set(info) == {"name", "implementation", "version", "optimization"}
    assert info["name"] == "iso18571"
    assert info["implementation"] == "C++20"
    assert info["version"] == version("iso18571")
    assert info["optimization"].startswith("x86-64-v")
    assert native_core.backend_info() == {
        "implementation": "C++20",
        "optimization": info["optimization"],
    }


def test_native_short_curves_fail_clearly() -> None:
    curve = np.column_stack(
        (np.arange(8, dtype=np.float64), np.ones(8, dtype=np.float64))
    )
    try:
        ISO18571(curve, curve)
    except ValueError as exc:
        assert "at least 9 samples" in str(exc)
        return
    raise AssertionError("short curve accepted")


def test_zero_identical_edge_lengths_use_finite_fallback_scores() -> None:
    for n in ROBUSTNESS_EDGE_LENGTHS:
        curve = curve_from_values(np.zeros(n, dtype=np.float64))
        iso, messages = score_with_warnings(curve, curve.copy())

        assert messages == [
            PHASE_UNDEFINED_WARNING,
            MAGNITUDE_ZERO_WARNING,
            SLOPE_ZERO_WARNING,
        ]
        assert_scores_close(
            iso,
            {"Z": 1.0, "EP": 1.0, "EM": 1.0, "ES": 1.0, "R": 1.0},
            f"zero identical n={n}",
        )
        assert iso.n_eps == 0
        assert iso.rho_e == 1.0
        assert iso.reference_start == 0
        assert iso.comparison_start == 0
        assert iso.shift_length == n


def test_zero_reference_nonzero_comparison_uses_finite_fallback_scores() -> None:
    reference = curve_from_values(np.zeros(9, dtype=np.float64))
    comparison = curve_from_values(np.ones(9, dtype=np.float64))
    iso, messages = score_with_warnings(reference, comparison)

    assert messages == [
        PHASE_UNDEFINED_WARNING,
        MAGNITUDE_ZERO_WARNING,
        SLOPE_ZERO_WARNING,
    ]
    assert_scores_close(
        iso,
        {"Z": 0.0, "EP": 1.0, "EM": 0.0, "ES": 1.0, "R": 0.4},
        "zero reference nonzero comparison",
    )
    assert iso.rho_e == 0.0


def test_constant_identical_uses_finite_slope_fallback_score() -> None:
    curve = curve_from_values(np.full(9, 2.0, dtype=np.float64))
    iso, messages = score_with_warnings(curve, curve.copy())

    assert messages == [PHASE_UNDEFINED_WARNING, SLOPE_ZERO_WARNING]
    assert_scores_close(
        iso,
        {"Z": 1.0, "EP": 1.0, "EM": 1.0, "ES": 1.0, "R": 1.0},
        "constant identical",
    )
    assert iso.rho_e == 1.0


def test_constant_offset_edge_lengths_use_finite_fallback_scores() -> None:
    for n in ROBUSTNESS_EDGE_LENGTHS:
        reference, comparison = constant_offset_pair(n)
        iso, messages = score_with_warnings(reference, comparison)

        assert messages == [PHASE_UNDEFINED_WARNING, SLOPE_ZERO_WARNING]
        assert_scores_close(
            iso,
            {"Z": 1.0, "EP": 1.0, "EM": 0.9, "ES": 1.0, "R": 0.98},
            f"constant offset n={n}",
        )
        assert iso.n_eps == 0
        assert iso.rho_e == 0.0
        assert iso.reference_start == 0
        assert iso.comparison_start == 0
        assert iso.shift_length == n


def test_short_shift_candidate_clamps_to_unshifted_alignment() -> None:
    reference, comparison = noisy_ramp_shifted_pair()
    iso, messages = score_with_warnings(reference, comparison)

    assert messages == [PHASE_CLAMP_WARNING]
    assert_scores_close(
        iso,
        {
            "Z": 0.27882098058123556,
            "EP": 1.0,
            "EM": 0.0,
            "ES": 0.0,
            "R": 0.31152839223249423,
        },
        "short shifted noisy ramp",
    )
    assert iso.n_eps == 0
    np.testing.assert_allclose(iso.rho_e, 0.41217625773989897, atol=PARITY_ATOL)
    assert iso.reference_start == 0
    assert iso.comparison_start == 0
    assert iso.shift_length == 9


def test_nonfinite_signal_values_fail_clearly_before_dtw() -> None:
    reference, comparison = noisy_ramp_shifted_pair()
    reference[4, 1] = np.nan

    with warnings.catch_warnings(record=True) as records:
        warnings.simplefilter("always", RuntimeWarning)
        try:
            ISO18571(reference, comparison)
        except ValueError as exc:
            assert "reference_curve signal values must be finite" in str(exc)
            assert records == []
            return
    raise AssertionError("nonfinite signal values were accepted")


def test_periodic_identical_sine_prefers_unshifted_native_alignment() -> None:
    curve = curve_from_values(sine_values(65))
    iso, messages = score_with_warnings(curve, curve.copy())

    assert messages == []
    assert_scores_close(
        iso,
        {"Z": 1.0, "EP": 1.0, "EM": 1.0, "ES": 1.0, "R": 1.0},
        "periodic sine identical",
    )
    assert iso.n_eps == 0
    np.testing.assert_allclose(iso.rho_e, 1.0, atol=1e-12)
    assert iso.reference_start == 0
    assert iso.comparison_start == 0
    assert iso.shift_length == 65


def test_float32_uniform_time_grid_is_accepted_by_native_validation() -> None:
    curve = float32_curve_from_values(np.linspace(-1.0, 1.0, 32, dtype=np.float64))
    iso, messages = score_with_warnings(curve, curve.copy())

    assert messages == []
    assert_scores_close(
        iso,
        {"Z": 1.0, "EP": 1.0, "EM": 1.0, "ES": 1.0, "R": 1.0},
        "float32 uniform grid",
    )
    assert iso.reference_start == 0
    assert iso.comparison_start == 0
    assert iso.shift_length == 32


def test_mixed_float32_float64_time_grids_are_accepted() -> None:
    values = np.linspace(-1.0, 1.0, 32, dtype=np.float64)
    curve32 = float32_curve_from_values(values)
    curve64 = curve_from_values(values)

    for context, reference, comparison in (
        ("float32 reference float64 comparison", curve32, curve64),
        ("float64 reference float32 comparison", curve64, curve32),
    ):
        iso, messages = score_with_warnings(reference, comparison)

        assert messages == []
        assert_scores_close(
            iso,
            {"Z": 1.0, "EP": 1.0, "EM": 1.0, "ES": 1.0, "R": 1.0},
            context,
        )
        assert iso.reference_start == 0
        assert iso.comparison_start == 0
        assert iso.shift_length == 32


def test_visibly_nonuniform_float32_time_grid_still_fails() -> None:
    reference = float32_curve_from_values(np.linspace(-1.0, 1.0, 32, dtype=np.float64))
    comparison = reference.copy()
    reference[5, 0] += np.float32(1.0e-5)

    with warnings.catch_warnings(record=True) as records:
        warnings.simplefilter("always", RuntimeWarning)
        try:
            ISO18571(reference, comparison)
        except ValueError as exc:
            assert "reference_curve time values must have a constant interval" in str(
                exc
            )
            assert records == []
            return
    raise AssertionError("visibly nonuniform float32 time grid was accepted")


def test_aliased_sine_scaling_oracle_limit_is_hard_coded() -> None:
    reference = curve_from_values(sine_values(10))
    comparison = reference.copy()
    comparison[:, 1] += 0.05
    base, base_messages = score_with_warnings(reference, comparison)

    reference_scaled = reference.copy()
    comparison_scaled = comparison.copy()
    reference_scaled[:, 1] *= 2.5
    comparison_scaled[:, 1] *= 2.5
    scaled, scaled_messages = score_with_warnings(reference_scaled, comparison_scaled)

    assert base_messages == []
    assert scaled_messages == []
    assert_scores_close(
        base,
        {
            "Z": 0.0,
            "EP": 1.0,
            "EM": 0.0,
            "ES": 0.9994058352973331,
            "R": 0.39988116705946664,
        },
        "aliased sine offset base",
    )
    assert_scores_close(
        scaled,
        {
            "Z": 0.0,
            "EP": 1.0,
            "EM": 0.0,
            "ES": 0.9966430794571737,
            "R": 0.39932861589143476,
        },
        "aliased sine offset scaled",
    )
    assert base.n_eps == 0
    assert scaled.n_eps == 0
    assert base.reference_start == 0
    assert scaled.reference_start == 0
    assert base.comparison_start == 0
    assert scaled.comparison_start == 0
    assert base.shift_length == 10
    assert scaled.shift_length == 10


def test_short_sparse_edge_cases_clamp_to_unshifted_alignment() -> None:
    edge_cases = (
        (
            "impulse n=9",
            impulse_pair(9),
            {
                "Z": 0.7777777777777778,
                "EP": 1.0,
                "EM": 0.0,
                "ES": 0.5,
                "R": 0.6111111111111112,
            },
            -0.125,
        ),
        (
            "sparse spikes n=9",
            sparse_spikes_pair(9),
            {
                "Z": 0.5892394452072538,
                "EP": 1.0,
                "EM": 0.0,
                "ES": 0.0,
                "R": 0.43569577808290155,
            },
            -0.21621621621621623,
        ),
    )

    for context, (reference, comparison), expected_scores, expected_rho_e in edge_cases:
        iso, messages = score_with_warnings(reference, comparison)

        assert messages == [PHASE_CLAMP_WARNING]
        assert_scores_close(
            iso,
            expected_scores,
            context,
        )
        assert iso.n_eps == 0
        np.testing.assert_allclose(iso.rho_e, expected_rho_e, atol=PARITY_ATOL)
        assert iso.reference_start == 0
        assert iso.comparison_start == 0
        assert iso.shift_length == 9
