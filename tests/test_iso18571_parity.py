from __future__ import annotations

import numpy as np

from iso18571 import ISO18571, backend_info, magnitude_ratio, score_components, warp_path
from tests.iso18571_test_helpers import (
    assert_downloaded_expected_scores,
    assert_scores_close,
    score_result,
)

PARITY_BACKENDS = ("dtwalign", "native", "dtw_python", "librosa")


def test_downloaded_annex_scores_match_official_and_parity(downloaded_annex_cases) -> None:
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


def test_generated_annex_scores_match_or_raise_together(generated_annex_cases) -> None:
    for case in generated_annex_cases:
        results = {backend: score_result(case, backend) for backend in PARITY_BACKENDS}
        exception_types = {result for result in results.values() if isinstance(result, type)}
        score_values = {backend: result for backend, result in results.items() if not isinstance(result, type)}
        if exception_types:
            assert len(exception_types) == 1 and not score_values, f"{case.name}: mixed parity results {results}"
            continue

        expected = score_values["dtwalign"]
        for backend, observed in score_values.items():
            if backend != "dtwalign":
                assert_scores_close(observed, expected, case.name, backend)


def test_native_surface_is_small_and_accepts_numpy_arrays(generated_annex_cases) -> None:
    case = next(case for case in generated_annex_cases if "sine_amp_offset" in case.name)
    scores = score_components(case.reference_curve, case.comparison_curve, {"dt": case.dt})
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
    assert ISO18571(case.reference_curve, case.comparison_curve, dt=case.dt).scores == scores
    assert backend_info()["dtw_layout"] == "index_incremental"
    assert backend_info()["reduction_mode"] == "all"
    assert backend_info()["name"] == "iso18571"
    assert str(backend_info()["selected_x86_64_level"]).startswith("x86-64-v")
    path = warp_path(case.comparison_curve[:, 1], case.reference_curve[:, 1], 0.1)
    assert path.ndim == 2 and path.shape[1] == 2
    assert magnitude_ratio(case.comparison_curve[:, 1], case.reference_curve[:, 1], 0.1) >= 0.0


def test_native_rejects_invalid_params(generated_annex_cases) -> None:
    case = next(case for case in generated_annex_cases if "sine_amp_offset" in case.name)
    invalid_params = (
        {"dt": 0.0},
        {"dt": np.inf},
        {"eps_m": 0.0},
        {"e_s": 0.0},
        {"init_min": -0.1},
        {"init_min": 1.0},
        {"a_0": -0.1},
        {"a_0": 0.05, "b_0": 0.05},
        {"w_z": -0.1, "w_p": 0.3, "w_m": 0.4, "w_s": 0.4},
        {"w_z": 0.5, "w_p": 0.2, "w_m": 0.2, "w_s": 0.2},
    )
    for params in invalid_params:
        try:
            score_components(case.reference_curve, case.comparison_curve, params)
        except ValueError:
            continue
        raise AssertionError(f"invalid params accepted: {params}")


def test_native_short_curves_fail_clearly() -> None:
    curve = np.column_stack((np.arange(8, dtype=np.float64), np.ones(8, dtype=np.float64)))
    try:
        score_components(curve, curve, {})
    except ValueError as exc:
        assert "at least 9 samples" in str(exc)
        return
    raise AssertionError("short curve accepted")


def test_native_full_window_caps_to_useful_radius() -> None:
    x = np.asarray([0.0, 1.0, 0.2, 1.3, 0.7, 1.5], dtype=np.float64)
    y = np.asarray([0.1, 0.9, 0.4, 1.1, 0.8, 1.4], dtype=np.float64)
    path_full = warp_path(x, y, 1.0)
    path_oversized = warp_path(x, y, 10.0)
    ratio_full = magnitude_ratio(x, y, 1.0)
    ratio_oversized = magnitude_ratio(x, y, 10.0)
    np.testing.assert_array_equal(path_oversized, path_full)
    np.testing.assert_allclose(ratio_oversized, ratio_full, rtol=0.0, atol=0.0)
