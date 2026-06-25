from __future__ import annotations

from collections.abc import Callable, Sequence
from typing import Any

import numpy as np
import pytest
from numpy.typing import NDArray

from tools import signals
from tools.signals import SignalGenerator


def test_signal_generator_materializes_curve_shape_and_time() -> None:
    generator = SignalGenerator(5, 0.25, start=1.0).add(signals.constant, value=3.0)

    curve = generator.curve()

    assert np.allclose(
        curve[:, 0], np.array([1.0, 1.25, 1.5, 1.75, 2.0]), rtol=1.0e-7, atol=0.0
    )
    assert np.allclose(curve[:, 1], np.full(5, 3.0), rtol=1.0e-7, atol=0.0)
    assert curve.dtype == np.float64
    assert curve.shape == (5, 2)


def test_signal_generator_affine_time_case_matches_default() -> None:
    default = SignalGenerator(5, 0.25, start=1.0).curve()
    configured = (
        SignalGenerator(5, 0.25, start=1.0).with_time_case("affine", np.float64).curve()
    )

    assert np.array_equal(configured, default)


def test_signal_generator_accepts_explicit_time_axis() -> None:
    time = np.array([10, 12, 14, 16], dtype=np.int16)

    curve = SignalGenerator(4, 1.0).with_time_axis(time).curve()

    assert curve.dtype == np.int16
    assert np.array_equal(curve[:, 0], time)
    assert np.array_equal(curve[:, 1], np.zeros(4, dtype=np.int16))


def test_signal_generator_rejects_invalid_explicit_time_axis_by_default() -> None:
    time = np.array([0, 2, 1, 3], dtype=np.int16)

    with pytest.raises(ValueError, match="strictly increasing"):
        SignalGenerator(4, 1.0).with_time_axis(time)

    curve = SignalGenerator(4, 1.0).with_time_axis(time, allow_invalid=True).curve()

    assert curve.dtype == np.int16
    assert np.array_equal(curve[:, 0], time)


def test_signal_generator_time_and_curve_dtype_controls() -> None:
    time_dtype_curve = SignalGenerator(4, 1.0).with_time_dtype(np.float32).curve()
    override_curve = (
        SignalGenerator(4, 1.0)
        .with_time_dtype(np.int16)
        .with_curve_dtype(np.float64)
        .curve()
    )

    assert time_dtype_curve.dtype == np.float32
    assert np.array_equal(time_dtype_curve[:, 0], np.arange(4, dtype=np.float32))
    assert override_curve.dtype == np.float64
    assert np.array_equal(override_curve[:, 0], np.arange(4, dtype=np.float64))


@pytest.mark.parametrize("dtype", [np.int32, np.int64])
def test_signal_generator_signed_endpoint_overflow_case(dtype: Any) -> None:
    curve = (
        SignalGenerator(9, 1.0)
        .with_time_case("signed_endpoint_overflow", dtype)
        .curve()
    )
    time = curve[:, 0]

    assert curve.dtype == np.dtype(dtype)
    assert np.all(np.diff(time) == time[1] - time[0])
    assert np.all(np.diff(time) > 0)
    assert int(time[-1]) - int(time[0]) > np.iinfo(dtype).max


def test_signal_generator_long_signed_case() -> None:
    curve = SignalGenerator(129, 1.0).with_time_case("long_signed", np.int8).curve()

    assert curve.dtype == np.int8
    assert curve[0, 0] == np.iinfo(np.int8).min
    assert curve[-1, 0] == 0
    assert np.all(np.diff(curve[:, 0]) == 1)


def test_signal_generator_wrapped_integer_case_requires_invalid_opt_in() -> None:
    with pytest.raises(ValueError, match="allow_invalid"):
        SignalGenerator(257, 1.0).with_time_case("wrapped_integer", np.int8)

    curve = (
        SignalGenerator(257, 1.0)
        .with_time_case("wrapped_integer", np.int8, allow_invalid=True)
        .curve()
    )

    assert curve.dtype == np.int8
    assert np.any(np.diff(curve[:, 0].astype(np.int16)) <= 0)


@pytest.mark.parametrize("dtype", [np.float32, np.float64])
def test_signal_generator_float_endpoint_span_overflow_case(dtype: Any) -> None:
    curve = (
        SignalGenerator(9, 1.0)
        .with_time_case("float_endpoint_span_overflow", dtype)
        .curve()
    )
    time = curve[:, 0]

    with np.errstate(over="ignore"):
        endpoint_span = dtype(time[-1] - time[0])

    assert curve.dtype == np.dtype(dtype)
    assert not np.isfinite(endpoint_span)
    assert np.all(np.isfinite(np.diff(time)))
    assert np.all(np.diff(time) > 0)


def test_signal_generator_random_time_case_is_seeded_and_repeatable() -> None:
    first = SignalGenerator(257, 1.0).with_random_time_case(seed=12).curve()
    second = SignalGenerator(257, 1.0).with_random_time_case(seed=12).curve()
    valid_only = (
        SignalGenerator(257, 1.0)
        .with_random_time_case(seed=12, include_invalid=False)
        .curve()
    )

    assert np.array_equal(first, second)
    assert valid_only.shape == (257, 2)
    assert np.all(np.diff(valid_only[:, 0].astype(np.longdouble)) > 0)


def test_signal_generator_composes_functions_and_scaling() -> None:
    generator = (
        SignalGenerator(4, 1.0)
        .add(signals.ramp, slope=2.0, intercept=1.0)
        .add(signals.constant, scale=0.5, value=4.0)
    )

    assert np.allclose(
        generator.values(), np.array([3.0, 5.0, 7.0, 9.0]), rtol=1.0e-7, atol=0.0
    )


def test_signal_generator_supports_custom_callable_without_rng() -> None:
    def quadratic(time: NDArray[np.float64], *, gain: float) -> NDArray[np.float64]:
        return np.asarray(gain * time * time, dtype=np.float64)

    generator = SignalGenerator(4, 1.0).add(quadratic, gain=2.0)

    assert np.allclose(
        generator.values(), np.array([0.0, 2.0, 8.0, 18.0]), rtol=1.0e-7, atol=0.0
    )


def test_signal_generator_sample_shift_changes_evaluation_time_without_wrapping() -> (
    None
):
    generator = SignalGenerator(4, 1.0).add(signals.ramp, slope=1.0, sample_shift=2)

    assert np.allclose(
        generator.values(), np.array([-2.0, -1.0, 0.0, 1.0]), rtol=1.0e-7, atol=0.0
    )


def test_signal_generator_noise_is_seeded_and_repeatable() -> None:
    first = SignalGenerator(8, 0.1, seed=12).add(signals.gaussian_noise, std=0.5)
    second = SignalGenerator(8, 0.1, seed=12).add(signals.gaussian_noise, std=0.5)
    different = SignalGenerator(8, 0.1, seed=13).add(signals.gaussian_noise, std=0.5)

    assert np.allclose(first.values(), first.values(), rtol=1.0e-7, atol=0.0)
    assert np.allclose(first.values(), second.values(), rtol=1.0e-7, atol=0.0)
    assert not np.array_equal(first.values(), different.values())


def test_all_named_signal_functions_return_finite_values() -> None:
    generator = SignalGenerator(64, 0.01, seed=3)
    functions: list[
        tuple[
            Callable[
                ...,
                NDArray[np.float64] | Sequence[float | int] | float | int,
            ],
            dict[str, Any],
        ]
    ] = [
        (signals.zero, {}),
        (signals.sample_index, {}),
        (signals.constant, {"value": 2.0}),
        (signals.ramp, {"slope": 1.5}),
        (
            signals.piecewise_ramp,
            {"breakpoints": [0.2, 0.4], "slopes": [1.0, -1.0, 0.5]},
        ),
        (signals.impulse, {"at": 0.25, "amplitude": 3.0}),
        (signals.sparse_spikes, {"count": 4, "amplitude": 2.0}),
        (signals.sine, {"frequency": 2.0}),
        (signals.chirp, {"start_frequency": 1.0, "end_frequency": 4.0}),
        (signals.square_step, {"at": 0.3}),
        (signals.gaussian_noise, {"std": 0.01}),
        (signals.ramp_impulses, {"impulse_times": [0.1, 0.3]}),
        (
            signals.piecewise_discontinuous,
            {"breakpoints": [0.2, 0.5], "values": [0.0, 1.0, -1.0]},
        ),
        (signals.sine_noise, {"frequency": 3.0, "noise_std": 0.01}),
    ]

    for function, params in functions:
        values = (
            SignalGenerator(generator.n, generator.dt, seed=generator.seed)
            .add(function, **params)
            .values()
        )
        assert values.shape == (generator.n,)
        assert np.all(np.isfinite(values))
