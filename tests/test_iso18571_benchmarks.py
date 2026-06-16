from __future__ import annotations

import pytest

from rating import ISO18571
from tests.iso18571_annex import THEORETICAL_INTEGRITY


def _run_annex_pass(annex_cases, backend: str) -> None:
    for case in annex_cases:
        iso = ISO18571(
            reference_curve=case.reference_curve,
            comparison_curve=case.comparison_curve,
            dt=case.dt,
            dtw_backend=backend,
        )
        iso.overall_rating(ndigits=-1)


@pytest.mark.benchmark
def test_backend_first_use_end_to_end(benchmark, annex_cases, dtw_backend: str) -> None:
    passed, note = THEORETICAL_INTEGRITY[dtw_backend]
    assert passed, note
    benchmark.pedantic(lambda: _run_annex_pass(annex_cases, dtw_backend), rounds=1, iterations=1)


@pytest.mark.benchmark
def test_backend_steady_state_calculation(benchmark, annex_cases, dtw_backend: str) -> None:
    passed, note = THEORETICAL_INTEGRITY[dtw_backend]
    assert passed, note
    _run_annex_pass(annex_cases, dtw_backend)
    benchmark(lambda: _run_annex_pass(annex_cases, dtw_backend))
