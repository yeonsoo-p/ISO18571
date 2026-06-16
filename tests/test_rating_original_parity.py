from __future__ import annotations

import numpy as np
import pytest

import rating
from tests.iso18571_annex import AnnexCase
from tests.iso18571_signals import signal_case


SCORE_KEYS = ("Z", "EP", "EM", "ES", "R")


def _scores_from_original(case: AnnexCase) -> dict[str, float | int]:
    import rating_original

    iso = rating_original.ISO18571(case.reference_curve, case.comparison_curve)
    return {
        "n_eps": iso._n_eps,
        "rho_e": iso._rho_e,
        "Z": iso.corridor_rating(ndigits=-1),
        "EP": iso.phase_rating(ndigits=-1),
        "EM": iso.magnitude_rating(ndigits=-1),
        "ES": iso.slope_rating(ndigits=-1),
        "R": iso.overall_rating(ndigits=-1),
        "Z_round": iso.corridor_rating(ndigits=3),
        "EP_round": iso.phase_rating(ndigits=3),
        "EM_round": iso.magnitude_rating(ndigits=3),
        "ES_round": iso.slope_rating(ndigits=3),
        "R_round": iso.overall_rating(ndigits=3),
    }


def _scores_from_native(case: AnnexCase) -> dict[str, float | int]:
    iso = rating.ISO18571(case.reference_curve, case.comparison_curve, dtw_backend="local_iso_native")
    return {
        "n_eps": iso._n_eps,
        "rho_e": iso._rho_e,
        "Z": iso.corridor_rating(ndigits=-1),
        "EP": iso.phase_rating(ndigits=-1),
        "EM": iso.magnitude_rating(ndigits=-1),
        "ES": iso.slope_rating(ndigits=-1),
        "R": iso.overall_rating(ndigits=-1),
        "Z_round": iso.corridor_rating(ndigits=3),
        "EP_round": iso.phase_rating(ndigits=3),
        "EM_round": iso.magnitude_rating(ndigits=3),
        "ES_round": iso.slope_rating(ndigits=3),
        "R_round": iso.overall_rating(ndigits=3),
    }


def _assert_scores_close(observed: dict[str, float | int], expected: dict[str, float | int]) -> None:
    assert observed["n_eps"] == expected["n_eps"]
    np.testing.assert_allclose(observed["rho_e"], expected["rho_e"], rtol=1e-10, atol=1e-10, equal_nan=True)
    for key in SCORE_KEYS:
        np.testing.assert_allclose(observed[key], expected[key], rtol=1e-9, atol=1e-9, equal_nan=True)
        np.testing.assert_allclose(
            observed[f"{key}_round"],
            expected[f"{key}_round"],
            rtol=0.0,
            atol=0.0,
            equal_nan=True,
        )


@pytest.mark.oracle
def test_native_scorer_matches_original_for_fixed_signal_annexes(fixed_signal_annex_cases) -> None:
    for case in fixed_signal_annex_cases:
        _assert_native_matches_original(case)


def _assert_native_matches_original(case: AnnexCase) -> None:
    try:
        expected = _scores_from_original(case)
    except Exception as exc:
        try:
            _scores_from_native(case)
        except Exception as native_exc:
            assert isinstance(native_exc, type(exc)), (
                f"{case.name}: "
                f"native raised {type(native_exc).__name__}, "
                f"expected {type(exc).__name__}"
            )
        else:
            raise AssertionError(f"{case.name}: native scorer did not raise {type(exc).__name__}")
    else:
        observed = _scores_from_native(case)
        _assert_scores_close(observed, expected)


def test_native_score_components_accepts_strided_and_float32_curves() -> None:
    from iso18571_native import score_components

    case = signal_case("sine_noise", 129)
    wide_reference = np.column_stack((case.reference, case.reference[:, 1] * 2.0)).astype(np.float32)
    wide_comparison = np.column_stack((case.comparison, case.comparison[:, 1] * 2.0)).astype(np.float32)
    reference_view = wide_reference[:, :2]
    comparison_view = wide_comparison[:, :2]

    scores = score_components(reference_view, comparison_view)
    assert set(["Z", "EP", "EM", "ES", "R", "n_eps", "rho_e"]).issubset(scores)
    assert isinstance(scores["R"], float)


@pytest.mark.stress
def test_native_scorer_handles_long_fixed_signal_annexes(fixed_signal_stress_annex_cases) -> None:
    for case in fixed_signal_stress_annex_cases:
        scores = _scores_from_native(case)
        assert set(SCORE_KEYS).issubset(scores), case.name
