from __future__ import annotations

from collections.abc import Sequence
from importlib.metadata import version

import numpy as np

import iso18571
import iso18571._core as native_core
from iso18571 import ISO18571, backend_info
from iso18571._core import _score_components
from tests.iso18571_annex import AnnexCase
from tests.iso18571_test_helpers import (
    assert_downloaded_expected_scores,
    assert_scores_close,
    score_result,
)

PARITY_BACKENDS = ("dtwalign", "native", "dtw_python", "librosa")


def test_downloaded_annex_scores_match_official_and_parity(
    downloaded_annex_cases: Sequence[AnnexCase],
) -> None:
    for case in downloaded_annex_cases:
        expected = None
        for backend in PARITY_BACKENDS:
            result = score_result(case, backend)
            if isinstance(result, type):
                raise AssertionError(f"{case.name} {backend}: raised {result.__name__}")
            assert_downloaded_expected_scores(result, case, backend)
            if expected is None:
                expected = result
            else:
                assert_scores_close(result, expected, case.name, backend)


def test_generated_annex_scores_match_or_raise_together(
    generated_annex_cases: Sequence[AnnexCase],
) -> None:
    for case in generated_annex_cases:
        results = {backend: score_result(case, backend) for backend in PARITY_BACKENDS}
        exception_types = {
            result for result in results.values() if isinstance(result, type)
        }
        score_values = {
            backend: result
            for backend, result in results.items()
            if not isinstance(result, type)
        }
        if exception_types:
            assert len(exception_types) == 1 and not score_values, (
                f"{case.name}: mixed parity results {results}"
            )
            continue

        expected = score_values["dtwalign"]
        for backend, observed in score_values.items():
            if backend != "dtwalign":
                assert_scores_close(observed, expected, case.name, backend)


def test_native_surface_is_small_and_accepts_numpy_arrays(
    generated_annex_cases: Sequence[AnnexCase],
) -> None:
    case = next(
        case for case in generated_annex_cases if "sine_amp_offset" in case.name
    )
    scores = _score_components(
        case.reference_curve, case.comparison_curve, {"dt": case.dt}
    )
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
    assert (
        ISO18571(case.reference_curve, case.comparison_curve, dt=case.dt).scores
        == scores
    )
    assert iso18571.__all__ == ["ISO18571", "backend_info"]
    assert not hasattr(iso18571, "score_components")
    assert not hasattr(iso18571, "magnitude_ratio")
    assert not hasattr(iso18571, "warp_path")
    assert not hasattr(native_core, "score_components")
    assert not hasattr(native_core, "magnitude_ratio")
    assert not hasattr(native_core, "warp_path")
    info = backend_info()
    assert set(info) == {"name", "implementation", "version", "optimization"}
    assert info["name"] == "iso18571"
    assert info["implementation"] == "C++17"
    assert info["version"] == version("iso18571")
    assert info["optimization"].startswith("x86-64-v")
    assert native_core.backend_info() == {
        "implementation": "C++17",
        "optimization": info["optimization"],
    }


def test_native_short_curves_fail_clearly() -> None:
    curve = np.column_stack(
        (np.arange(8, dtype=np.float64), np.ones(8, dtype=np.float64))
    )
    try:
        _score_components(curve, curve, {})
    except ValueError as exc:
        assert "at least 9 samples" in str(exc)
        return
    raise AssertionError("short curve accepted")
