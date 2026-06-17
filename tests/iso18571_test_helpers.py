from __future__ import annotations

import importlib.util
import warnings
from collections.abc import Callable
from pathlib import Path
from types import ModuleType
from typing import Any

import dtw
import librosa
import numpy as np

import iso18571
from tests.iso18571_annex import SCORE_NAMES, AnnexCase

EXPECTED_NUMERIC_WARNING_PATTERNS = (
    "invalid value encountered in divide",
    "invalid value encountered in scalar divide",
)
RATING_ORIGINAL_PATH = Path(__file__).resolve().parents[1] / "ref" / "rating_original.py"
_RATING_ORIGINAL_MODULE: ModuleType | None = None
SCORE_KEYS = ("Z", "EP", "EM", "ES", "R")
ROUND_SCORE_KEYS = tuple(f"{key}_round" for key in SCORE_KEYS)


def rating_original_module() -> ModuleType:
    global _RATING_ORIGINAL_MODULE

    if _RATING_ORIGINAL_MODULE is not None:
        return _RATING_ORIGINAL_MODULE
    if not RATING_ORIGINAL_PATH.exists():
        raise AssertionError(f"rating_original oracle is missing: {RATING_ORIGINAL_PATH}")

    spec = importlib.util.spec_from_file_location("rating_original_ref", RATING_ORIGINAL_PATH)
    if spec is None or spec.loader is None:
        raise AssertionError(f"rating_original oracle cannot be loaded: {RATING_ORIGINAL_PATH}")

    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    _RATING_ORIGINAL_MODULE = module
    return module


def assert_only_expected_numeric_warnings(records: list[warnings.WarningMessage], context: str) -> None:
    for record in records:
        message = str(record.message)
        expected_message = any(pattern in message for pattern in EXPECTED_NUMERIC_WARNING_PATTERNS)
        assert record.category is RuntimeWarning and expected_message, (
            f"{context}: unexpected warning {record.category.__name__}: {message}"
        )


def with_expected_numeric_warnings(fn: Callable[[], Any], context: str) -> Any:
    with warnings.catch_warnings(record=True) as records:
        warnings.simplefilter("always", RuntimeWarning)
        value = fn()
    assert_only_expected_numeric_warnings(records, context)
    return value


def scores_for_case(case: AnnexCase, backend: str) -> dict[str, float | int]:
    if backend == "native":
        iso = iso18571.ISO18571(case.reference_curve, case.comparison_curve, dt=case.dt)
    elif backend == "original":
        iso = rating_original_module().ISO18571(case.reference_curve, case.comparison_curve, dt=case.dt)
    elif backend in {"dtw_python", "librosa"}:
        return _scores_with_reference_shift_and_backend(case, backend)
    else:
        raise AssertionError(f"unknown parity backend {backend}")

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


def _scores_with_reference_shift_and_backend(case: AnnexCase, backend: str) -> dict[str, float | int]:
    reference_iso = rating_original_module().ISO18571(case.reference_curve, case.comparison_curve, dt=case.dt)
    x = reference_iso._cae_ts[:, 1]
    y = reference_iso._t_ts[:, 1]
    x_warped, y_warped = _compute_magnitude_backend(x, y, backend)
    e_mag = np.linalg.norm(x_warped - y_warped, ord=1) / np.linalg.norm(y_warped, ord=1)
    if e_mag == 0:
        em = 1
    elif e_mag > reference_iso._eps_m:
        em = 0
    else:
        em = ((reference_iso._eps_m - e_mag) / reference_iso._eps_m) ** reference_iso._k_m

    z = reference_iso.corridor_rating(ndigits=-1)
    ep = reference_iso.phase_rating(ndigits=-1)
    es = reference_iso.slope_rating(ndigits=-1)
    r = reference_iso._w_z * z + reference_iso._w_p * ep + reference_iso._w_m * em + reference_iso._w_s * es
    return {
        "n_eps": reference_iso._n_eps,
        "rho_e": reference_iso._rho_e,
        "Z": z,
        "EP": ep,
        "EM": em,
        "ES": es,
        "R": r,
        "Z_round": round(z, ndigits=3),
        "EP_round": round(ep, ndigits=3),
        "EM_round": round(em, ndigits=3),
        "ES_round": round(es, ndigits=3),
        "R_round": round(r, ndigits=3),
    }


def _compute_magnitude_backend(x: np.ndarray, y: np.ndarray, backend: str) -> tuple[np.ndarray, np.ndarray]:
    if backend == "dtw_python":
        path = _dtw_python_path(x, y)
    elif backend == "librosa":
        path = _librosa_path(x, y)
    else:
        raise AssertionError(f"unknown magnitude backend {backend}")
    return x[path[:, 0]], y[path[:, 1]]


def _dtw_window_radius(n: int, window_size: float) -> int:
    if n <= 0:
        raise ValueError("DTW input arrays must not be empty")
    return max(1, int(np.ceil(window_size * n)))


def _local_cost_matrix(x: np.ndarray, y: np.ndarray, window_size: float) -> np.ndarray:
    n = x.shape[0]
    radius = _dtw_window_radius(n, window_size)
    cost = np.square(x[:, np.newaxis] - y[np.newaxis, :])
    idx = np.arange(n)
    cost[np.abs(idx[:, np.newaxis] - idx[np.newaxis, :]) >= radius] = np.inf
    return cost


def _iso_backtrack(accumulated: np.ndarray) -> np.ndarray:
    accumulated = np.where(np.isnan(accumulated), np.inf, accumulated)
    i = accumulated.shape[0] - 1
    j = accumulated.shape[1] - 1
    path = [(i, j)]
    while i > 0 or j > 0:
        candidates = []
        if i > 0:
            candidates.append((accumulated[i - 1, j], i - 1, j))
        if j > 0:
            candidates.append((accumulated[i, j - 1], i, j - 1))
        if i > 0 and j > 0:
            candidates.append((accumulated[i - 1, j - 1], i - 1, j - 1))
        costs = [candidate[0] for candidate in candidates]
        _, i, j = candidates[int(np.argmin(costs))]
        path.append((i, j))
    path.reverse()
    return np.asarray(path, dtype=np.int64)


def _dtw_python_path(x: np.ndarray, y: np.ndarray) -> np.ndarray:
    radius = _dtw_window_radius(x.shape[0], 0.1)
    cost = np.square(x[:, np.newaxis] - y[np.newaxis, :])
    result = dtw.dtw(
        cost,
        step_pattern=dtw.symmetric1,
        window_type="sakoechiba",
        window_args={"window_size": radius - 1},
        keep_internals=True,
    )
    return _iso_backtrack(result.costMatrix)


def _librosa_path(x: np.ndarray, y: np.ndarray) -> np.ndarray:
    cost = _local_cost_matrix(x, y, 0.1)
    step_sizes = np.asarray([[1, 0], [0, 1], [1, 1]], dtype=np.uint32)
    weights = np.zeros(3, dtype=np.float64)
    _, warping_path = librosa.sequence.dtw(
        C=cost,
        step_sizes_sigma=step_sizes,
        weights_add=weights,
        weights_mul=np.ones(3, dtype=np.float64),
        global_constraints=False,
        backtrack=True,
    )
    return np.asarray(warping_path[::-1], dtype=np.int64)


def score_result(case: AnnexCase, backend: str) -> dict[str, float | int] | type[BaseException]:
    try:
        return with_expected_numeric_warnings(lambda: scores_for_case(case, backend), f"{case.name} {backend}")
    except Exception as exc:
        return type(exc)


def assert_scores_close(
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
    for key in ROUND_SCORE_KEYS:
        np.testing.assert_allclose(
            observed[key],
            expected[key],
            rtol=0.0,
            atol=0.0,
            equal_nan=True,
            err_msg=f"{case_name} {backend} {key}",
        )


def assert_downloaded_expected_scores(scores: dict[str, float | int], case: AnnexCase, backend: str) -> None:
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
