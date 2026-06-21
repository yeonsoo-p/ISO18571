from __future__ import annotations

from collections.abc import Sequence
import warnings

from tests.iso18571_annex import AnnexCase
from tests.iso18571_test_helpers import (
    AnnexParityResult,
    assert_only_expected_numeric_warnings,
    assert_downloaded_expected_scores,
    assert_downloaded_expected_shifted_values,
    assert_scores_close,
    scores_for_case,
    with_expected_numeric_warnings,
    without_warnings,
)

PARITY_BACKENDS = ("dtwalign", "native", "dtw_python", "librosa")
DEGENERATE_GENERATED_PREFIXES = ("generated__zero__", "generated__constant__")
GeneratedOutcome = AnnexParityResult | type[Exception]


def is_degenerate_generated_case(case: AnnexCase) -> bool:
    return case.name.startswith(DEGENERATE_GENERATED_PREFIXES)


def generated_degenerate_outcome(case: AnnexCase, backend: str) -> GeneratedOutcome:
    with warnings.catch_warnings(record=True) as records:
        warnings.simplefilter("always")
        try:
            result = scores_for_case(case, backend)
        except Exception as exc:
            assert_only_expected_numeric_warnings(records, f"{case.name} {backend}")
            return type(exc)
    assert_only_expected_numeric_warnings(records, f"{case.name} {backend}")
    return result


def assert_generated_degenerate_outcomes_match(
    outcomes: dict[str, GeneratedOutcome], case_name: str
) -> None:
    expected = outcomes["dtwalign"]
    if isinstance(expected, type):
        for backend, observed in outcomes.items():
            assert isinstance(observed, type) and observed is expected, (
                f"{case_name} {backend} exception type"
            )
        return

    for backend, observed in outcomes.items():
        assert not isinstance(observed, type), (
            f"{case_name} {backend} raised {observed}"
        )
        if backend != "dtwalign":
            assert_scores_close(observed, expected, case_name, backend)


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

    assert len(generated_annex_cases) == 116
    assert any(name.startswith("generated__zero__") for name in case_names)
    assert any(name.startswith("generated__constant__") for name in case_names)
    assert "generated__impulse__n9" not in case_names
    assert "generated__sparse_spikes__n9" not in case_names


def test_generated_annex_scores_match_together(
    generated_annex_cases: Sequence[AnnexCase],
) -> None:
    assert generated_annex_cases, "generated Annex parity cases were not found"
    for case in generated_annex_cases:
        if is_degenerate_generated_case(case):
            outcomes = {
                backend: generated_degenerate_outcome(case, backend)
                for backend in PARITY_BACKENDS
            }
            assert_generated_degenerate_outcomes_match(outcomes, case.name)
            continue

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
