from __future__ import annotations

import numpy as np

from iso18571 import ISO18571
from iso18571.rating import _iso_accumulated_cost_matrix, _iso_backtrack, _local_cost_matrix, _shifted_correlations
from iso18571_native import (
    DtwLayout,
    ParallelMode,
    ReductionMode,
    SimdLevel,
    SimdTargetMode,
    _magnitude_ratio_variant_spec,
    _score_components_variant_spec,
    _simd_info,
    magnitude_ratio,
    score_components,
    warp_path,
)
from tests.iso18571_annex import (
    SCORE_NAMES,
    THEORETICAL_INTEGRITY,
    fixed_signal_annex_case,
    phase_shift_annex_case,
)


def _scores(case, backend: str) -> dict[str, float]:
    iso = ISO18571(
        reference_curve=case.reference_curve,
        comparison_curve=case.comparison_curve,
        dt=case.dt,
        dtw_backend=backend,
    )
    return {
        "R": iso.overall_rating(ndigits=-1),
        "Z": iso.corridor_rating(ndigits=-1),
        "EP": iso.phase_rating(ndigits=-1),
        "EM": iso.magnitude_rating(ndigits=-1),
        "ES": iso.slope_rating(ndigits=-1),
    }


def test_local_iso_backtrack_uses_required_tie_order() -> None:
    path = _iso_backtrack(np.zeros((3, 3), dtype=float))
    expected = np.asarray([[0, 0], [0, 1], [0, 2], [1, 2], [2, 2]], dtype=np.int64)
    np.testing.assert_array_equal(path, expected)


def test_local_cost_matrix_uses_iso_window_rule() -> None:
    x = np.arange(10, dtype=float)
    cost = _local_cost_matrix(x, x, window_size=0.2)
    finite = np.argwhere(np.isfinite(cost))
    assert np.max(np.abs(finite[:, 0] - finite[:, 1])) == 1
    assert np.isinf(cost[0, 2])


def test_shifted_correlations_match_corrcoef_reference() -> None:
    rng = np.random.default_rng(18571)
    for n in (5, 17, 64, 129):
        reference = rng.normal(size=n)
        comparison = rng.normal(size=n)
        window_size = int(np.floor(n * 0.2) + 1)
        left, right = _shifted_correlations(reference, comparison, window_size)

        expected_left = [np.corrcoef(reference, comparison)[0][-1]]
        expected_right = [expected_left[0]]
        for idx in range(1, window_size):
            expected_left.append(np.corrcoef(reference[:-idx], comparison[idx:])[0][-1])
            expected_right.append(np.corrcoef(reference[idx:], comparison[:-idx])[0][-1])

        np.testing.assert_allclose(left, expected_left, rtol=1e-12, atol=1e-12, equal_nan=True)
        np.testing.assert_allclose(right, expected_right, rtol=1e-12, atol=1e-12, equal_nan=True)


def test_backend_theoretical_integrity_documented(dtw_backend: str) -> None:
    passed, note = THEORETICAL_INTEGRITY[dtw_backend]
    assert note
    assert passed, note


def test_backend_matches_iso_annex_reference_scores(annex_cases, dtw_backend: str) -> None:
    passed, note = THEORETICAL_INTEGRITY[dtw_backend]
    assert passed, note

    worst = ("", "", 0.0, 0.0, -1.0)
    errors = []
    for case in annex_cases:
        result = _scores(case, dtw_backend)
        for name in SCORE_NAMES:
            error = abs(result[name] - case.expected[name])
            errors.append(error)
            if error > worst[4]:
                worst = (case.name, name, result[name], case.expected[name], error)

    assert max(errors) <= 0.001, (
        f"worst={worst[0]} {worst[1]} got={worst[2]:.8f} expected={worst[3]:.8f} error={worst[4]:.8f}"
    )


def test_native_path_matches_local_reference_for_annex_cases(annex_cases) -> None:
    for case in annex_cases:
        iso = ISO18571(
            reference_curve=case.reference_curve,
            comparison_curve=case.comparison_curve,
            dt=case.dt,
            dtw_backend="local_iso_numpy",
        )
        native_path = warp_path(iso._cae_ts[:, 1], iso._t_ts[:, 1], 0.1)
        cost = _local_cost_matrix(iso._cae_ts[:, 1], iso._t_ts[:, 1], window_size=0.1)
        local_path = _iso_backtrack(_iso_accumulated_cost_matrix(cost))
        np.testing.assert_array_equal(native_path, local_path, err_msg=case.name)


def test_native_magnitude_ratio_matches_warped_curve_formula(annex_cases) -> None:
    for case in annex_cases:
        iso = ISO18571(
            reference_curve=case.reference_curve,
            comparison_curve=case.comparison_curve,
            dt=case.dt,
            dtw_backend="local_iso_numpy",
        )
        cost = _local_cost_matrix(iso._cae_ts[:, 1], iso._t_ts[:, 1], window_size=0.1)
        path = _iso_backtrack(_iso_accumulated_cost_matrix(cost))
        x_w = iso._cae_ts[:, 1][path[:, 0]]
        y_w = iso._t_ts[:, 1][path[:, 1]]
        expected = np.linalg.norm(x_w - y_w, ord=1) / np.linalg.norm(y_w, ord=1)
        observed = magnitude_ratio(iso._cae_ts[:, 1], iso._t_ts[:, 1], 0.1)
        assert abs(observed - expected) <= 1e-12, case.name


def test_native_path_matches_local_reference_for_random_curves() -> None:
    rng = np.random.default_rng(18571)
    for n in (2, 5, 17, 64, 129):
        for window_size in (0.1, 0.2, 1.0):
            x = rng.normal(size=n)
            y = rng.normal(size=n)
            cost = _local_cost_matrix(x, y, window_size=window_size)
            expected = _iso_backtrack(_iso_accumulated_cost_matrix(cost))
            observed = warp_path(x, y, window_size)
            np.testing.assert_array_equal(observed, expected, err_msg=f"n={n} window={window_size}")


def test_native_preserves_iso_tie_order_for_zero_curves() -> None:
    x = np.zeros(5, dtype=np.float64)
    path = warp_path(x, x, 1.0)
    expected = np.asarray(
        [[0, 0], [0, 1], [0, 2], [0, 3], [0, 4], [1, 4], [2, 4], [3, 4], [4, 4]],
        dtype=np.int64,
    )
    np.testing.assert_array_equal(path, expected)
    assert np.isnan(magnitude_ratio(x, x, 1.0))


def test_native_experimental_variants_match_public_magnitude_ratio() -> None:
    rng = np.random.default_rng(18572)
    variants = (
        (DtwLayout.Current, ParallelMode.NoParallel, 0, SimdLevel.Scalar, SimdTargetMode.GradientOnly, 1),
        (DtwLayout.RangePrecompute, ParallelMode.NoParallel, 0, SimdLevel.Scalar, SimdTargetMode.GradientOnly, 1),
        (DtwLayout.IndexIncremental, ParallelMode.NoParallel, 0, SimdLevel.Scalar, SimdTargetMode.GradientOnly, 1),
        (DtwLayout.CompactDirection, ParallelMode.NoParallel, 0, SimdLevel.Scalar, SimdTargetMode.GradientOnly, 1),
        (DtwLayout.Current, ParallelMode.Diagonal, 0, SimdLevel.Scalar, SimdTargetMode.GradientOnly, 2),
    )
    for n in (17, 64, 129):
        x = rng.normal(size=n)
        y = 0.9 * x + rng.normal(scale=0.2, size=n)
        expected = magnitude_ratio(x, y, 0.1)
        for dtw_layout, parallel_mode, block_size, simd_level, simd_target_mode, max_threads in variants:
            observed = _magnitude_ratio_variant_spec(
                x,
                y,
                0.1,
                dtw_layout,
                parallel_mode,
                block_size,
                simd_level,
                simd_target_mode,
                max_threads,
            )
            np.testing.assert_allclose(
                observed,
                expected,
                rtol=1e-12,
                atol=1e-12,
                err_msg=f"{dtw_layout} {parallel_mode} {simd_level} n={n}",
            )


def test_native_score_component_variants_match_public_scorer() -> None:
    cases = (
        fixed_signal_annex_case("sine_noise", 129),
        fixed_signal_annex_case("sparse_spikes", 129),
        phase_shift_annex_case("phase_multitone_shift_020", 129),
    )
    variants = (
        (
            DtwLayout.Current,
            ReductionMode.NoReduction,
            ParallelMode.NoParallel,
            0,
            SimdLevel.Scalar,
            SimdTargetMode.GradientOnly,
            1,
        ),
        (
            DtwLayout.RangePrecompute,
            ReductionMode.NoReduction,
            ParallelMode.NoParallel,
            0,
            SimdLevel.Scalar,
            SimdTargetMode.GradientOnly,
            1,
        ),
        (
            DtwLayout.IndexIncremental,
            ReductionMode.PhaseDualProduct,
            ParallelMode.NoParallel,
            0,
            SimdLevel.Scalar,
            SimdTargetMode.GradientOnly,
            1,
        ),
        (
            DtwLayout.CompactDirection,
            ReductionMode.SharedShiftWorkspace,
            ParallelMode.NoParallel,
            0,
            SimdLevel.Scalar,
            SimdTargetMode.GradientOnly,
            1,
        ),
        (
            DtwLayout.IndexIncremental,
            ReductionMode.All,
            ParallelMode.NoParallel,
            0,
            SimdLevel.Scalar,
            SimdTargetMode.GradientOnly,
            1,
        ),
        (
            DtwLayout.Current,
            ReductionMode.All,
            ParallelMode.Blocked,
            64,
            SimdLevel.Scalar,
            SimdTargetMode.GradientOnly,
            2,
        ),
    )
    keys = ("Z", "EP", "EM", "ES", "R", "n_eps", "rho_e", "reference_start", "comparison_start", "shift_length")

    for case in cases:
        expected = score_components(case.reference_curve, case.comparison_curve, {"dt": case.dt})
        for (
            dtw_layout,
            reduction_mode,
            parallel_mode,
            block_size,
            simd_level,
            simd_target_mode,
            max_threads,
        ) in variants:
            observed = _score_components_variant_spec(
                case.reference_curve,
                case.comparison_curve,
                {"dt": case.dt},
                dtw_layout,
                reduction_mode,
                parallel_mode,
                block_size,
                simd_level,
                simd_target_mode,
                max_threads,
            )
            variant_label = f"{dtw_layout} {reduction_mode} {parallel_mode} {simd_level} {simd_target_mode}"
            for key in keys:
                np.testing.assert_allclose(
                    observed[key],
                    expected[key],
                    rtol=1e-12,
                    atol=1e-12,
                    equal_nan=True,
                    err_msg=f"{case.name} {variant_label} {key}",
                )


def test_native_simd_target_modes_match_public_scorer() -> None:
    cases = (
        fixed_signal_annex_case("chirp", 129),
        fixed_signal_annex_case("sparse_spikes", 129),
        phase_shift_annex_case("phase_multitone_shift_020", 129),
    )
    simd_target_modes = (
        SimdTargetMode.PhaseProducts,
        SimdTargetMode.DtwLocalCost,
        SimdTargetMode.SlopeSmoothing,
        SimdTargetMode.MagnitudePath,
        SimdTargetMode.All,
    )
    simd_levels = (SimdLevel.Scalar, SimdLevel.Sse2, SimdLevel.Avx2, SimdLevel.Avx2Fma)
    keys = ("Z", "EP", "EM", "ES", "R", "n_eps", "rho_e", "reference_start", "comparison_start", "shift_length")

    for case in cases:
        expected = score_components(case.reference_curve, case.comparison_curve, {"dt": case.dt})
        for simd_target_mode in simd_target_modes:
            for simd_level in simd_levels:
                observed = _score_components_variant_spec(
                    case.reference_curve,
                    case.comparison_curve,
                    {"dt": case.dt},
                    DtwLayout.Current,
                    ReductionMode.All,
                    ParallelMode.Blocked,
                    64,
                    simd_level,
                    simd_target_mode,
                    2,
                )
                variant_label = f"{simd_target_mode} {simd_level}"
                assert observed["simd_target_mode"] in {
                    "phase_products",
                    "dtw_local_cost",
                    "slope_smoothing",
                    "magnitude_path",
                    "all",
                }
                for key in keys:
                    np.testing.assert_allclose(
                        observed[key],
                        expected[key],
                        rtol=1e-12,
                        atol=1e-12,
                        equal_nan=True,
                        err_msg=f"{case.name} {variant_label} {key}",
                    )


def test_native_phase_product_simd_preserves_near_tie_shift_decision() -> None:
    n = 257
    t = np.linspace(0.0, 1.0, n)
    reference_values = (
        np.sin(2.0 * np.pi * 7.0 * t) + 0.35 * np.sin(2.0 * np.pi * 13.0 * t) + 1e-11 * np.linspace(-1.0, 1.0, n)
    )
    comparison_values = np.roll(reference_values, 1)
    comparison_values[0] = reference_values[0] + 1e-11
    reference_curve = np.column_stack((t, reference_values))
    comparison_curve = np.column_stack((t, comparison_values))
    expected = score_components(reference_curve, comparison_curve, {"dt": 1.0 / (n - 1)})

    for simd_level in (SimdLevel.Scalar, SimdLevel.Sse2, SimdLevel.Avx2, SimdLevel.Avx2Fma):
        observed = _score_components_variant_spec(
            reference_curve,
            comparison_curve,
            {"dt": 1.0 / (n - 1)},
            DtwLayout.Current,
            ReductionMode.All,
            ParallelMode.NoParallel,
            0,
            simd_level,
            SimdTargetMode.PhaseProducts,
            1,
        )
        for key in ("n_eps", "rho_e", "reference_start", "comparison_start", "shift_length"):
            np.testing.assert_allclose(
                observed[key],
                expected[key],
                rtol=1e-12,
                atol=1e-12,
                equal_nan=True,
                err_msg=f"{simd_level} {key}",
            )


def test_native_enum_variant_api_reports_simd_metadata_and_auto_dispatches() -> None:
    info = _simd_info()
    assert "compiled_avx2_fma" in info
    assert "auto_level" in info
    assert "compiled_avx512" not in info

    case = fixed_signal_annex_case("chirp", 64)
    observed = _score_components_variant_spec(
        case.reference_curve,
        case.comparison_curve,
        {"dt": case.dt},
        DtwLayout.Current,
        ReductionMode.All,
        ParallelMode.NoParallel,
        0,
        SimdLevel.Auto,
        SimdTargetMode.All,
        1,
    )
    assert observed["requested_simd_level"] == "auto"
    assert observed["selected_simd_level"] in {"scalar", "sse2", "avx2", "avx2_fma"}
    assert isinstance(observed["simd_fallback"], bool)


def test_native_enum_variant_api_rejects_invalid_blocked_specs() -> None:
    case = fixed_signal_annex_case("chirp", 64)
    try:
        _score_components_variant_spec(
            case.reference_curve,
            case.comparison_curve,
            {"dt": case.dt},
            DtwLayout.Current,
            ReductionMode.NoReduction,
            ParallelMode.Blocked,
            0,
            SimdLevel.Scalar,
            SimdTargetMode.GradientOnly,
            1,
        )
    except ValueError:
        return
    raise AssertionError("blocked variant with block_size=0 should fail")
