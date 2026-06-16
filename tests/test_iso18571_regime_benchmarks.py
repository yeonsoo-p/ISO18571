from __future__ import annotations

import math
import os

import numpy as np
import pytest

from iso18571_native import score_components
from iso18571_native._core import _score_components_variant
from tests.iso18571_annex import fixed_signal_annex_case, phase_shift_annex_case


REGIME_LENGTHS = (64, 129, 512, 1430, 4096, 8192, 16384, 32768, 65536)
REGIME_FAMILIES = (
    "sine_amp_offset",
    "chirp",
    "sparse_spikes",
    "gaussian_noise",
    "phase_multitone_shift_020",
    "phase_chirp_shift_050",
    "phase_pulses_shift_100",
    "phase_smooth_step_shift_180",
)
DTW_LAYOUTS = ("dtw_current", "dtw_range_precompute", "dtw_index_incremental", "dtw_compact_direction")
REDUCTIONS = ("reduce_none", "phase_dual_product", "fused_slope", "shared_shift_workspace", "all_reductions")
PARALLEL_FORMS = ("parallel_none", "blocked64", "blocked128", "blocked256", "blocked512")
THREAD_COUNTS = (1, 2, 4, 8)


def _active_thread_counts() -> tuple[int, ...]:
    cpu_count = os.cpu_count() or 1
    return tuple(thread_count for thread_count in THREAD_COUNTS if thread_count <= cpu_count)


def _case(family: str, n: int):
    if family.startswith("phase_"):
        return phase_shift_annex_case(family, n)
    return fixed_signal_annex_case(family, n)


def _variant_params():
    active_threads = _active_thread_counts()
    for family in REGIME_FAMILIES:
        for n in REGIME_LENGTHS:
            for dtw_layout in DTW_LAYOUTS:
                for reduction in REDUCTIONS:
                    for parallel_form in PARALLEL_FORMS:
                        thread_counts = (1,) if parallel_form == "parallel_none" else active_threads
                        for max_threads in thread_counts:
                            variant = f"{dtw_layout}+{reduction}+{parallel_form}"
                            yield pytest.param(
                                family,
                                n,
                                variant,
                                max_threads,
                                id=f"{family}__n{n}__{variant}__t{max_threads}",
                            )


def _cells(effective_n: int) -> int:
    radius = max(1, math.ceil(0.1 * effective_n))
    return effective_n * (2 * radius - 1)


@pytest.mark.regime
@pytest.mark.benchmark
@pytest.mark.parametrize(("family", "n", "variant", "max_threads"), tuple(_variant_params()))
def test_native_score_component_variant_regime_speed(benchmark, family: str, n: int, variant: str, max_threads: int) -> None:
    case = _case(family, n)
    params = {"dt": case.dt}
    expected = score_components(case.reference_curve, case.comparison_curve, params)
    observed = _score_components_variant(case.reference_curve, case.comparison_curve, params, variant, max_threads)
    for key in ("Z", "EP", "EM", "ES", "R", "n_eps", "rho_e", "shift_length"):
        np.testing.assert_allclose(
            observed[key],
            expected[key],
            rtol=1e-12,
            atol=1e-12,
            equal_nan=True,
            err_msg=f"{case.name} {variant} t{max_threads} {key}",
        )

    effective_n = int(observed["shift_length"])
    benchmark.extra_info["family"] = family
    benchmark.extra_info["n"] = n
    benchmark.extra_info["effective_n"] = effective_n
    benchmark.extra_info["cells"] = _cells(effective_n)
    benchmark.extra_info["variant"] = variant
    benchmark.extra_info["max_threads"] = max_threads
    benchmark(lambda: _score_components_variant(case.reference_curve, case.comparison_curve, params, variant, max_threads))
