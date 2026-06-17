from __future__ import annotations

import warnings
from collections.abc import Callable
from typing import Protocol, TypeVar

import numpy as np

import iso18571
from iso18571_reference import rating_dtwalign, rating_dtw_python, rating_librosa
from tests.iso18571_annex import SCORE_NAMES, AnnexCase

EXPECTED_NUMERIC_WARNING_PATTERNS = (
    "invalid value encountered in divide",
    "invalid value encountered in scalar divide",
)
SCORE_KEYS = ("Z", "EP", "EM", "ES", "R")
ROUND_SCORE_KEYS = tuple(f"{key}_round" for key in SCORE_KEYS)
ScoreDict = dict[str, float | int]
T = TypeVar("T")


class Scorer(Protocol):
    @property
    def n_eps(self) -> int: ...

    @property
    def rho_e(self) -> float: ...

    def corridor_rating(self, ndigits: int = 3) -> float: ...

    def phase_rating(self, ndigits: int = 3) -> float: ...

    def magnitude_rating(self, ndigits: int = 3) -> float: ...

    def slope_rating(self, ndigits: int = 3) -> float: ...

    def overall_rating(self, ndigits: int = 3) -> float: ...


def assert_only_expected_numeric_warnings(
    records: list[warnings.WarningMessage], context: str
) -> None:
    for record in records:
        message = str(record.message)
        expected_message = any(
            pattern in message for pattern in EXPECTED_NUMERIC_WARNING_PATTERNS
        )
        assert record.category is RuntimeWarning and expected_message, (
            f"{context}: unexpected warning {record.category.__name__}: {message}"
        )


def with_expected_numeric_warnings(fn: Callable[[], T], context: str) -> T:
    with warnings.catch_warnings(record=True) as records:
        warnings.simplefilter("always", RuntimeWarning)
        value = fn()
    assert_only_expected_numeric_warnings(records, context)
    return value


def scores_for_case(case: AnnexCase, backend: str) -> ScoreDict:
    iso: Scorer
    if backend == "native":
        iso = iso18571.ISO18571(
            case.reference_curve,
            case.comparison_curve,
            k_z=2.0,
            k_p=1.0,
            k_m=1.0,
            dt=case.dt,
        )
    elif backend == "dtwalign":
        iso = rating_dtwalign.ISO18571(
            case.reference_curve, case.comparison_curve, dt=case.dt
        )
    elif backend == "dtw_python":
        iso = rating_dtw_python.ISO18571(
            case.reference_curve, case.comparison_curve, dt=case.dt
        )
    elif backend == "librosa":
        iso = rating_librosa.ISO18571(
            case.reference_curve, case.comparison_curve, dt=case.dt
        )
    else:
        raise AssertionError(f"unknown parity backend {backend}")

    return {
        "n_eps": iso.n_eps,
        "rho_e": iso.rho_e,
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


def score_result(case: AnnexCase, backend: str) -> ScoreDict | type[BaseException]:
    try:
        return with_expected_numeric_warnings(
            lambda: scores_for_case(case, backend), f"{case.name} {backend}"
        )
    except Exception as exc:
        return type(exc)


def assert_scores_close(
    observed: ScoreDict,
    expected: ScoreDict,
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
    for key in ROUND_SCORE_KEYS:
        np.testing.assert_allclose(
            observed[key],
            expected[key],
            rtol=0.0,
            atol=0.0,
            equal_nan=True,
            err_msg=f"{case_name} {backend} {key}",
        )


def assert_downloaded_expected_scores(
    scores: ScoreDict, case: AnnexCase, backend: str
) -> None:
    assert case.expected is not None
    for key in SCORE_NAMES:
        np.testing.assert_allclose(
            scores[key],
            case.expected[key],
            rtol=0.0,
            atol=0.001,
            equal_nan=True,
            err_msg=f"{case.name} {backend} official {key}",
        )
