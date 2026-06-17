from __future__ import annotations

from iso18571_native import backend_info, magnitude_ratio, score_components, warp_path
from tests.iso18571_test_helpers import (
    assert_downloaded_expected_scores,
    assert_scores_close,
    score_result,
)

PARITY_BACKENDS = ("original", "native", "dtw_python", "librosa")


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

        expected = score_values["original"]
        for backend, observed in score_values.items():
            if backend != "original":
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
    assert backend_info()["dtw_layout"] == "index_incremental"
    assert backend_info()["reduction_mode"] == "all"
    assert str(backend_info()["selected_x86_64_level"]).startswith("x86-64-v")
    path = warp_path(case.comparison_curve[:, 1], case.reference_curve[:, 1], 0.1)
    assert path.ndim == 2 and path.shape[1] == 2
    assert magnitude_ratio(case.comparison_curve[:, 1], case.reference_curve[:, 1], 0.1) >= 0.0
