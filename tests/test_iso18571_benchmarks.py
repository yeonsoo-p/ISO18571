from __future__ import annotations

import pytest

from iso18571 import ISO18571
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


def _run_native_fixed_signal_annex_pass(fixed_signal_benchmark_annex_cases) -> None:
    _run_annex_pass(fixed_signal_benchmark_annex_cases, "local_iso_native")


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


@pytest.mark.benchmark
def test_native_fixed_signal_annex_calculation(benchmark, fixed_signal_benchmark_annex_cases) -> None:
    _run_native_fixed_signal_annex_pass(fixed_signal_benchmark_annex_cases)
    benchmark(lambda: _run_native_fixed_signal_annex_pass(fixed_signal_benchmark_annex_cases))
