from __future__ import annotations

import numpy as np
import pytest

import rating
from tests.iso18571_annex import AnnexCase
from tests.iso18571_signals import signal_case


SCORE_KEYS = ("Z", "EP", "EM", "ES", "R")
ORACLE_BACKEND = "rating_original"
PARITY_BACKENDS = ("local_iso_native", "dtw_python", "librosa")


def _scores_from_backend(case: AnnexCase, backend: str) -> dict[str, float | int]:
    if backend == ORACLE_BACKEND:
        import rating_original

        iso = rating_original.ISO18571(case.reference_curve, case.comparison_curve)
    else:
        iso = rating.ISO18571(
            case.reference_curve,
            case.comparison_curve,
            dt=case.dt,
            dtw_backend=backend,
        )
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


def _assert_scores_close(
    observed: dict[str, float | int],
    expected: dict[str, float | int],
    case_name: str,
    backend: str,
) -> None:
    assert observed["n_eps"] == expected["n_eps"], f"{case_name} {backend} n_eps"
    np.testing.assert_allclose(
        observed["rho_e"],
        expected["rho_e"],
        rtol=1e-10,
        atol=1e-10,
        equal_nan=True,
        err_msg=f"{case_name} {backend} rho_e",
    )
    for key in SCORE_KEYS:
        np.testing.assert_allclose(
            observed[key],
            expected[key],
            rtol=1e-9,
            atol=1e-9,
            equal_nan=True,
            err_msg=f"{case_name} {backend} {key}",
        )
        np.testing.assert_allclose(
            observed[f"{key}_round"],
            expected[f"{key}_round"],
            rtol=0.0,
            atol=0.0,
            equal_nan=True,
            err_msg=f"{case_name} {backend} {key}_round",
        )


@pytest.mark.oracle
def test_generated_annexes_match_across_original_native_dtw_python_and_librosa(generated_annex_cases) -> None:
    for case in generated_annex_cases:
        _assert_backends_match_original(case)


def _assert_backends_match_original(case: AnnexCase) -> None:
    try:
        expected = _scores_from_backend(case, ORACLE_BACKEND)
    except Exception as exc:
        for backend in PARITY_BACKENDS:
            try:
                _scores_from_backend(case, backend)
            except Exception as backend_exc:
                assert isinstance(backend_exc, type(exc)), (
                    f"{case.name} {backend}: "
                    f"raised {type(backend_exc).__name__}, "
                    f"expected {type(exc).__name__}"
                )
            else:
                raise AssertionError(f"{case.name} {backend}: scorer did not raise {type(exc).__name__}")
    else:
        for backend in PARITY_BACKENDS:
            observed = _scores_from_backend(case, backend)
            _assert_scores_close(observed, expected, case.name, backend)


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
def test_native_scorer_handles_long_generated_annexes(generated_stress_annex_cases) -> None:
    for case in generated_stress_annex_cases:
        scores = _scores_from_backend(case, "local_iso_native")
        assert set(SCORE_KEYS).issubset(scores), case.name
