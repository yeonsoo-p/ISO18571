from __future__ import annotations

import warnings
from collections.abc import Callable
from dataclasses import dataclass
from typing import Protocol, TypeVar

import numpy as np

import iso18571
from reference import rating_dtwalign, rating_dtw_python, rating_librosa
from tests.iso18571_annex import SCORE_NAMES, AnnexCase

PHASE_UNDEFINED_WARNING = (
    "ISO18571 phase correlation is undefined; using finite fallback rho_e"
)
PHASE_CLAMP_WARNING = (
    "ISO18571 phase alignment left fewer than 9 samples; using unshifted alignment"
)
MAGNITUDE_ZERO_WARNING = (
    "ISO18571 magnitude reference denominator is zero; using fallback magnitude score"
)
SLOPE_ZERO_WARNING = (
    "ISO18571 slope reference denominator is zero; using fallback slope score"
)
EXPECTED_NATIVE_RUNTIME_WARNING_MESSAGES = (
    PHASE_UNDEFINED_WARNING,
    PHASE_CLAMP_WARNING,
    MAGNITUDE_ZERO_WARNING,
    SLOPE_ZERO_WARNING,
)
EXPECTED_NUMERIC_WARNING_PATTERNS = (
    "invalid value encountered in divide",
    "invalid value encountered in scalar divide",
    *EXPECTED_NATIVE_RUNTIME_WARNING_MESSAGES,
)
SCORE_KEYS = ("Z", "EP", "EM", "ES", "R")
ROUND_SCORE_KEYS = tuple(f"{key}_round" for key in SCORE_KEYS)
T = TypeVar("T")


@dataclass(frozen=True)
class AnnexParityResult:
    scores: dict[str, float]
    n_eps: int
    rho_e: float
    reference_start: int
    comparison_start: int
    shift_length: int


class Scorer(Protocol):
    @property
    def n_eps(self) -> int | None: ...

    @property
    def rho_e(self) -> float | None: ...

    @property
    def reference_start(self) -> int | None: ...

    @property
    def comparison_start(self) -> int | None: ...

    @property
    def shift_length(self) -> int | None: ...

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


def without_warnings(fn: Callable[[], T], context: str) -> T:
    with warnings.catch_warnings(record=True) as records:
        warnings.simplefilter("always")
        value = fn()
    assert not records, (
        f"{context}: unexpected warnings "
        f"{[(record.category.__name__, str(record.message)) for record in records]}"
    )
    return value


def scores_for_case(case: AnnexCase, backend: str) -> AnnexParityResult:
    iso: Scorer
    if backend == "native":
        iso = iso18571.ISO18571(
            case.reference_curve,
            case.comparison_curve,
            store_validation=True,
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

    scores = {
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
    n_eps = iso.n_eps
    rho_e = iso.rho_e
    reference_start = iso.reference_start
    comparison_start = iso.comparison_start
    shift_length = iso.shift_length
    assert n_eps is not None
    assert rho_e is not None
    assert reference_start is not None
    assert comparison_start is not None
    assert shift_length is not None
    return AnnexParityResult(
        scores=scores,
        n_eps=n_eps,
        rho_e=rho_e,
        reference_start=reference_start,
        comparison_start=comparison_start,
        shift_length=shift_length,
    )


def assert_scores_close(
    observed: AnnexParityResult,
    expected: AnnexParityResult,
    case_name: str,
    backend: str,
) -> None:
    assert observed.n_eps == expected.n_eps, f"{case_name} {backend} n_eps"
    assert observed.reference_start == expected.reference_start, (
        f"{case_name} {backend} reference_start"
    )
    assert observed.comparison_start == expected.comparison_start, (
        f"{case_name} {backend} comparison_start"
    )
    assert observed.shift_length == expected.shift_length, (
        f"{case_name} {backend} shift_length"
    )
    np.testing.assert_allclose(
        observed.rho_e,
        expected.rho_e,
        rtol=1e-10,
        atol=1e-10,
        equal_nan=True,
        err_msg=f"{case_name} {backend} rho_e",
    )
    for key in SCORE_KEYS:
        np.testing.assert_allclose(
            observed.scores[key],
            expected.scores[key],
            rtol=1e-9,
            atol=1e-9,
            equal_nan=True,
            err_msg=f"{case_name} {backend} {key}",
        )
    for key in ROUND_SCORE_KEYS:
        np.testing.assert_allclose(
            observed.scores[key],
            expected.scores[key],
            rtol=0.0,
            atol=0.0,
            equal_nan=True,
            err_msg=f"{case_name} {backend} {key}",
        )


def assert_downloaded_expected_scores(
    result: AnnexParityResult, case: AnnexCase, backend: str
) -> None:
    assert case.expected is not None
    for key in SCORE_NAMES:
        np.testing.assert_allclose(
            result.scores[key],
            case.expected[key],
            rtol=0.0,
            atol=0.001,
            equal_nan=True,
            err_msg=f"{case.name} {backend} official {key}",
        )


def assert_downloaded_expected_shifted_values(
    result: AnnexParityResult, case: AnnexCase, backend: str
) -> None:
    assert case.expected_shifted_reference_values is not None
    assert case.expected_shifted_comparison_values is not None
    reference_stop = result.reference_start + result.shift_length
    comparison_stop = result.comparison_start + result.shift_length
    np.testing.assert_allclose(
        case.reference_curve[result.reference_start : reference_stop, 1],
        case.expected_shifted_reference_values,
        rtol=0.0,
        atol=0.001,
        equal_nan=True,
        err_msg=f"{case.name} {backend} official shifted reference",
    )
    np.testing.assert_allclose(
        case.comparison_curve[result.comparison_start : comparison_stop, 1],
        case.expected_shifted_comparison_values,
        rtol=0.0,
        atol=0.001,
        equal_nan=True,
        err_msg=f"{case.name} {backend} official shifted comparison",
    )
