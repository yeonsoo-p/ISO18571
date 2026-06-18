from __future__ import annotations

from collections.abc import Sequence

from tests.iso18571_annex import AnnexCase
from tests.iso18571_test_helpers import (
    AnnexParityResult,
    assert_downloaded_expected_scores,
    assert_downloaded_expected_shifted_values,
    assert_scores_close,
    scores_for_case,
    with_expected_numeric_warnings,
    without_warnings,
)

PARITY_BACKENDS = ("dtwalign", "native", "dtw_python", "librosa")


def test_downloaded_annex_scores_match_official_and_parity(
    downloaded_annex_cases: Sequence[AnnexCase],
) -> None:
    for case in downloaded_annex_cases:
        expected: AnnexParityResult | None = None
        for backend in PARITY_BACKENDS:
            result = with_expected_numeric_warnings(
                lambda: scores_for_case(case, backend), f"{case.name} {backend}"
            )
            assert_downloaded_expected_scores(result, case, backend)
            assert_downloaded_expected_shifted_values(result, case, backend)
            if expected is None:
                expected = result
            else:
                assert_scores_close(result, expected, case.name, backend)


def test_generated_annex_cases_are_parity_corpus(
    generated_annex_cases: Sequence[AnnexCase],
) -> None:
    case_names = {case.name for case in generated_annex_cases}

    assert len(generated_annex_cases) == 102
    assert all(not name.startswith("generated__zero__") for name in case_names)
    assert all(not name.startswith("generated__constant__") for name in case_names)
    assert "generated__impulse__n9" not in case_names
    assert "generated__sparse_spikes__n9" not in case_names


def test_generated_annex_scores_match_together(
    generated_annex_cases: Sequence[AnnexCase],
) -> None:
    assert generated_annex_cases, "generated Annex parity cases were not found"
    for case in generated_annex_cases:
        results = {
            backend: without_warnings(
                lambda: scores_for_case(case, backend), f"{case.name} {backend}"
            )
            for backend in PARITY_BACKENDS
        }
        expected = results["dtwalign"]
        for backend, observed in results.items():
            if backend != "dtwalign":
                assert_scores_close(observed, expected, case.name, backend)
