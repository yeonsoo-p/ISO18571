from __future__ import annotations

import pytest

from rating import ISO18571
from tests.iso18571_signals import SignalCase, signal_case


SIGNAL_BENCHMARK_CASES = (
    pytest.param("short_sine_noise", "sine_noise", 129, id="short_sine_noise_129"),
    pytest.param("annex_like_sine_amp_offset", "sine_amp_offset", 1430, id="annex_like_sine_amp_offset_1430"),
    pytest.param("long_smooth_chirp", "chirp", 8192, id="long_smooth_chirp_8192"),
    pytest.param("long_noisy_gaussian", "gaussian_noise", 8192, id="long_noisy_gaussian_8192"),
    pytest.param("long_sparse_spikes", "sparse_spikes", 8192, id="long_sparse_spikes_8192"),
)


def _score_once(case: SignalCase) -> float:
    iso = ISO18571(
        reference_curve=case.reference,
        comparison_curve=case.comparison,
        dtw_backend="local_iso_native",
    )
    return iso.overall_rating(ndigits=-1)


@pytest.mark.stress
@pytest.mark.benchmark
@pytest.mark.parametrize(("label", "family", "n"), SIGNAL_BENCHMARK_CASES)
def test_native_signal_family_calculation_speed(benchmark, label: str, family: str, n: int) -> None:
    case = signal_case(family, n)
    result = benchmark(lambda: _score_once(case))
    assert label
    assert isinstance(result, float)
