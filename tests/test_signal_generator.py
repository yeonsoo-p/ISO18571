from __future__ import annotations

from collections.abc import Callable
from typing import Any

import numpy as np
from numpy.typing import ArrayLike, NDArray

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
    functions: list[tuple[Callable[..., ArrayLike], dict[str, Any]]] = [
        (signals.zero, {}),
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
