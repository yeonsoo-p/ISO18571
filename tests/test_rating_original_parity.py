from __future__ import annotations

import numpy as np
import pytest

import rating
from tests.iso18571_signals import SIGNAL_FAMILIES, SignalCase, signal_case

rating_original = pytest.importorskip("rating_original")


SCORE_KEYS = ("Z", "EP", "EM", "ES", "R")
NORMAL_LENGTHS = (9, 10, 17, 64, 129, 512, 1430)
STRESS_LENGTHS = (4096, 8192, 16384, 32768)


def _scores_from_original(case: SignalCase) -> dict[str, float | int]:
    iso = rating_original.ISO18571(case.reference, case.comparison)
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


def _scores_from_native(case: SignalCase) -> dict[str, float | int]:
    iso = rating.ISO18571(case.reference, case.comparison, dtw_backend="local_iso_native")
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


@pytest.mark.parametrize("family", SIGNAL_FAMILIES)
@pytest.mark.parametrize("n", NORMAL_LENGTHS)
def test_native_scorer_matches_original_for_signal_families(family: str, n: int) -> None:
    case = signal_case(family, n)
    try:
        expected = _scores_from_original(case)
    except Exception as exc:
        with pytest.raises(type(exc)):
            _scores_from_native(case)
        return

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
@pytest.mark.parametrize("family", SIGNAL_FAMILIES)
@pytest.mark.parametrize("n", STRESS_LENGTHS)
def test_native_scorer_handles_long_signal_families(family: str, n: int) -> None:
    case = signal_case(family, n)
    scores = _scores_from_native(case)
    assert set(SCORE_KEYS).issubset(scores)
