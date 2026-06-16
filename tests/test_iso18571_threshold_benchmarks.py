from __future__ import annotations

import os

import numpy as np
import pytest
from iso18571_native._core import (
    DtwLayout,
    ParallelMode,
    SimdLevel,
    _magnitude_ratio_variant_spec,
    _parallel_barrier_overhead,
)

from iso18571_native import magnitude_ratio
from tests.iso18571_signals import signal_case

LAYOUT_VARIANTS = (
    ("dtw_current", DtwLayout.Current),
    ("dtw_range_precompute", DtwLayout.RangePrecompute),
    ("dtw_index_incremental", DtwLayout.IndexIncremental),
    ("dtw_compact_direction", DtwLayout.CompactDirection),
)
LAYOUT_CASES = (
    pytest.param("chirp", 8192, id="chirp_8192"),
    pytest.param("sparse_spikes", 8192, id="sparse_spikes_8192"),
    pytest.param("gaussian_noise", 8192, id="gaussian_noise_8192"),
)
THRESHOLD_FAMILIES = ("sine_amp_offset", "chirp", "sparse_spikes", "gaussian_noise")
THRESHOLD_LENGTHS = (1430, 4096, 8192, 12288, 16384, 24576, 32768)
THREAD_COUNTS = (1, 2, 4, 8)


def _values(family: str, n: int) -> tuple[np.ndarray, np.ndarray]:
    case = signal_case(family, n)
    return case.comparison[:, 1], case.reference[:, 1]


def _active_thread_counts() -> tuple[int, ...]:
    cpu_count = os.cpu_count() or 1
    return tuple(thread_count for thread_count in THREAD_COUNTS if thread_count <= cpu_count)


@pytest.mark.benchmark
@pytest.mark.parametrize(("variant", "dtw_layout"), LAYOUT_VARIANTS)
@pytest.mark.parametrize(("family", "n"), LAYOUT_CASES)
def test_magnitude_layout_variant_calculation_speed(
    benchmark, variant: str, dtw_layout: DtwLayout, family: str, n: int
) -> None:
    x, y = _values(family, n)
    expected = magnitude_ratio(x, y, 0.1)
    observed = _magnitude_ratio_variant_spec(x, y, 0.1, dtw_layout, ParallelMode.NoParallel, 0, SimdLevel.Scalar, 1)
    np.testing.assert_allclose(observed, expected, rtol=1e-12, atol=1e-12, equal_nan=True)
    benchmark(
        lambda: _magnitude_ratio_variant_spec(x, y, 0.1, dtw_layout, ParallelMode.NoParallel, 0, SimdLevel.Scalar, 1)
    )


@pytest.mark.threshold
@pytest.mark.benchmark
@pytest.mark.parametrize("family", THRESHOLD_FAMILIES)
@pytest.mark.parametrize("n", THRESHOLD_LENGTHS)
@pytest.mark.parametrize("max_threads", _active_thread_counts())
def test_diagonal_parallel_threshold_speed(benchmark, family: str, n: int, max_threads: int) -> None:
    x, y = _values(family, n)
    expected = magnitude_ratio(x, y, 0.1)
    observed = _magnitude_ratio_variant_spec(
        x,
        y,
        0.1,
        DtwLayout.Current,
        ParallelMode.Diagonal,
        0,
        SimdLevel.Scalar,
        max_threads,
    )
    np.testing.assert_allclose(observed, expected, rtol=1e-12, atol=1e-12, equal_nan=True)
    benchmark(
        lambda: _magnitude_ratio_variant_spec(
            x,
            y,
            0.1,
            DtwLayout.Current,
            ParallelMode.Diagonal,
            0,
            SimdLevel.Scalar,
            max_threads,
        )
    )


@pytest.mark.threshold
@pytest.mark.benchmark
@pytest.mark.parametrize("n", THRESHOLD_LENGTHS)
@pytest.mark.parametrize("max_threads", _active_thread_counts())
def test_parallel_barrier_overhead(benchmark, n: int, max_threads: int) -> None:
    iterations = 2 * n - 1
    benchmark(lambda: _parallel_barrier_overhead(iterations, max_threads))
