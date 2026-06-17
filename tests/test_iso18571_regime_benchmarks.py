from __future__ import annotations

import math
import os
from functools import lru_cache

import numpy as np
import pytest

from iso18571_native import (
    DtwLayout,
    ParallelMode,
    ReductionMode,
    SimdLevel,
    SimdTargetMode,
    _resolve_simd_level,
    score_components,
    score_variant_function,
)
from tests.iso18571_annex import fixed_signal_annex_case, phase_shift_annex_case

REGIME_LENGTHS = (4096, 8192, 12288, 16384, 32768, 65536)
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
SIMD_LEVELS = ("scalar", "sse2", "avx2", "avx2_fma")
THREAD_COUNTS = (1, 2, 4, 8, 12, 16, 24)
SIMD_TARGETS = ("gradient_only", "phase_products", "dtw_local_cost", "slope_smoothing", "magnitude_path", "all")

DTW_LAYOUT_MAP = {
    "dtw_current": DtwLayout.Current,
    "dtw_range_precompute": DtwLayout.RangePrecompute,
    "dtw_index_incremental": DtwLayout.IndexIncremental,
    "dtw_compact_direction": DtwLayout.CompactDirection,
}
REDUCTION_MAP = {
    "reduce_none": ReductionMode.NoReduction,
    "phase_dual_product": ReductionMode.PhaseDualProduct,
    "fused_slope": ReductionMode.FusedSlope,
    "shared_shift_workspace": ReductionMode.SharedShiftWorkspace,
    "all_reductions": ReductionMode.All,
}
SIMD_LEVEL_MAP = {
    "scalar": SimdLevel.Scalar,
    "sse2": SimdLevel.Sse2,
    "avx2": SimdLevel.Avx2,
    "avx2_fma": SimdLevel.Avx2Fma,
}
SIMD_TARGET_MAP = {
    "gradient_only": SimdTargetMode.GradientOnly,
    "phase_products": SimdTargetMode.PhaseProducts,
    "dtw_local_cost": SimdTargetMode.DtwLocalCost,
    "slope_smoothing": SimdTargetMode.SlopeSmoothing,
    "magnitude_path": SimdTargetMode.MagnitudePath,
    "all": SimdTargetMode.All,
}
SIMD_NAME_MAP = {
    "scalar": SimdLevel.Scalar,
    "sse2": SimdLevel.Sse2,
    "avx2": SimdLevel.Avx2,
    "avx2_fma": SimdLevel.Avx2Fma,
}


def _env_tuple(name: str, default: tuple[str, ...]) -> tuple[str, ...]:
    raw = os.environ.get(name)
    if raw is None or not raw.strip():
        return default
    return tuple(part.strip() for part in raw.split(",") if part.strip())


def _env_int_tuple(name: str, default: tuple[int, ...]) -> tuple[int, ...]:
    raw = os.environ.get(name)
    if raw is None or not raw.strip():
        return default
    return tuple(int(part.strip()) for part in raw.split(",") if part.strip())


def _active_thread_counts() -> tuple[int, ...]:
    cpu_count = os.cpu_count() or 1
    requested = _env_int_tuple("ISO18571_REGIME_THREADS", THREAD_COUNTS)
    return tuple(thread_count for thread_count in requested if thread_count <= cpu_count)


def _parallel_spec(parallel_form: str) -> tuple[ParallelMode, int]:
    if parallel_form == "parallel_none":
        return ParallelMode.NoParallel, 0
    if parallel_form == "diagonal_parallel":
        return ParallelMode.Diagonal, 0
    if parallel_form.startswith("blocked"):
        return ParallelMode.Blocked, int(parallel_form.removeprefix("blocked"))
    raise AssertionError(f"unknown parallel form {parallel_form}")


def _split_variant(variant: str) -> tuple[str, str, str, str | None, str | None]:
    dtw_layout = "dtw_current"
    reduction = "reduce_none"
    parallel_form = "parallel_none"
    simd_level = None
    simd_target = None
    for token in variant.split("+"):
        if token in DTW_LAYOUT_MAP:
            dtw_layout = token
        elif token in REDUCTION_MAP:
            reduction = token
        elif token == "diagonal_parallel" or token == "parallel_none" or token.startswith("blocked"):
            parallel_form = token
        elif token.startswith("simd_"):
            simd_level = token.removeprefix("simd_")
        elif token.startswith("target_"):
            simd_target = token.removeprefix("target_")
        elif token:
            raise AssertionError(f"unknown variant token {token}")
    return dtw_layout, reduction, parallel_form, simd_level, simd_target


def _simd_level_from_label(simd_level: str) -> SimdLevel:
    if simd_level == "auto":
        raise AssertionError("simd_auto is dispatch behavior, not a benchmark matrix level")
    try:
        return SIMD_LEVEL_MAP[simd_level]
    except KeyError as exc:
        raise AssertionError(f"unknown SIMD level {simd_level}") from exc


def _simd_target_from_label(simd_target: str) -> SimdTargetMode:
    try:
        return SIMD_TARGET_MAP[simd_target]
    except KeyError as exc:
        raise AssertionError(f"unknown SIMD target {simd_target}") from exc


def _selected_simd_level(simd_level: SimdLevel) -> SimdLevel:
    selected_name = _resolve_simd_level(simd_level)["selected_simd_level"]
    return SIMD_NAME_MAP[selected_name]


@lru_cache(maxsize=None)
def _case(family: str, n: int):
    if family.startswith("phase_"):
        return phase_shift_annex_case(family, n)
    return fixed_signal_annex_case(family, n)


@lru_cache(maxsize=None)
def _expected_scores(family: str, n: int):
    case = _case(family, n)
    return score_components(case.reference_curve, case.comparison_curve, {"dt": case.dt})


def _variant_params():
    active_threads = _active_thread_counts()
    families = _env_tuple("ISO18571_REGIME_FAMILIES", REGIME_FAMILIES)
    lengths = _env_int_tuple("ISO18571_REGIME_LENGTHS", REGIME_LENGTHS)
    dtw_layouts = _env_tuple("ISO18571_REGIME_DTW_LAYOUTS", DTW_LAYOUTS)
    reductions = _env_tuple("ISO18571_REGIME_REDUCTIONS", REDUCTIONS)
    parallel_forms = _env_tuple("ISO18571_REGIME_PARALLEL_FORMS", PARALLEL_FORMS)
    explicit_variants = _env_tuple("ISO18571_REGIME_VARIANTS", ())
    simd_levels = _env_tuple("ISO18571_REGIME_SIMD_LEVELS", ("scalar",) if explicit_variants else SIMD_LEVELS)
    simd_targets = _env_tuple("ISO18571_REGIME_SIMD_TARGETS", ("gradient_only",))
    if explicit_variants:
        for family in families:
            for n in lengths:
                for variant in explicit_variants:
                    dtw_layout, reduction, parallel_form, variant_simd_level, variant_simd_target = _split_variant(
                        variant
                    )
                    parallel_mode, block_size = _parallel_spec(parallel_form)
                    selected_simd_levels = (variant_simd_level,) if variant_simd_level is not None else simd_levels
                    selected_simd_targets = (variant_simd_target,) if variant_simd_target is not None else simd_targets
                    thread_counts = (1,) if parallel_form == "parallel_none" else active_threads
                    for simd_level in selected_simd_levels:
                        for simd_target in selected_simd_targets:
                            variant_label = (
                                f"{dtw_layout}+{reduction}+{parallel_form}+simd_{simd_level}+target_{simd_target}"
                            )
                            for max_threads in thread_counts:
                                yield pytest.param(
                                    family,
                                    n,
                                    variant_label,
                                    DTW_LAYOUT_MAP[dtw_layout],
                                    REDUCTION_MAP[reduction],
                                    parallel_mode,
                                    block_size,
                                    simd_level,
                                    _simd_level_from_label(simd_level),
                                    simd_target,
                                    _simd_target_from_label(simd_target),
                                    max_threads,
                                    id=f"{family}__n{n}__{variant_label}__t{max_threads}",
                                )
    else:
        for family in families:
            for n in lengths:
                for dtw_layout in dtw_layouts:
                    for reduction in reductions:
                        for parallel_form in parallel_forms:
                            parallel_mode, block_size = _parallel_spec(parallel_form)
                            thread_counts = (1,) if parallel_form == "parallel_none" else active_threads
                            for simd_level in simd_levels:
                                for simd_target in simd_targets:
                                    variant = f"{dtw_layout}+{reduction}+{parallel_form}+simd_{simd_level}+target_{simd_target}"
                                    for max_threads in thread_counts:
                                        yield pytest.param(
                                            family,
                                            n,
                                            variant,
                                            DTW_LAYOUT_MAP[dtw_layout],
                                            REDUCTION_MAP[reduction],
                                            parallel_mode,
                                            block_size,
                                            simd_level,
                                            _simd_level_from_label(simd_level),
                                            simd_target,
                                            _simd_target_from_label(simd_target),
                                            max_threads,
                                            id=f"{family}__n{n}__{variant}__t{max_threads}",
                                        )


def _cells(effective_n: int) -> int:
    radius = max(1, math.ceil(0.1 * effective_n))
    return effective_n * (2 * radius - 1)


@pytest.mark.regime
@pytest.mark.benchmark
@pytest.mark.parametrize(
    (
        "family",
        "n",
        "variant",
        "dtw_layout",
        "reduction_mode",
        "parallel_mode",
        "block_size",
        "simd_label",
        "simd_level",
        "simd_target_label",
        "simd_target_mode",
        "max_threads",
    ),
    tuple(_variant_params()),
)
def test_native_score_component_variant_regime_speed(
    benchmark,
    family: str,
    n: int,
    variant: str,
    dtw_layout: DtwLayout,
    reduction_mode: ReductionMode,
    parallel_mode: ParallelMode,
    block_size: int,
    simd_label: str,
    simd_level: SimdLevel,
    simd_target_label: str,
    simd_target_mode: SimdTargetMode,
    max_threads: int,
) -> None:
    case = _case(family, n)
    params = {"dt": case.dt}
    expected = _expected_scores(family, n)
    selected_simd_level = _selected_simd_level(simd_level)
    variant_function = score_variant_function(selected_simd_level, simd_target_mode)
    observed = variant_function(
        case.reference_curve,
        case.comparison_curve,
        params,
        dtw_layout,
        reduction_mode,
        parallel_mode,
        block_size,
        max_threads,
    )
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
    benchmark.extra_info["requested_simd_level"] = simd_label
    benchmark.extra_info["selected_simd_level"] = str(observed["selected_simd_level"])
    benchmark.extra_info["simd_fallback"] = bool(observed["simd_fallback"])
    benchmark.extra_info["simd_target_mode"] = simd_target_label
    benchmark.extra_info["max_threads"] = max_threads
    benchmark(
        lambda: variant_function(
            case.reference_curve,
            case.comparison_curve,
            params,
            dtw_layout,
            reduction_mode,
            parallel_mode,
            block_size,
            max_threads,
        )
    )
