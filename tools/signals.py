"""Composable signal generation helpers for ISO18571 tests and examples."""

from __future__ import annotations

import inspect
from collections.abc import Callable, Sequence
from dataclasses import dataclass, field
from typing import Any, Protocol

import numpy as np
from numpy.typing import NDArray


class SignalFunction(Protocol):
    def __call__(
        self,
        time: NDArray[np.float64],
        *,
        rng: np.random.Generator | None = None,
        **params: Any,
    ) -> NDArray[np.float64] | Sequence[float | int] | float | int: ...


@dataclass(frozen=True)
class SignalComponent:
    function: Callable[
        ...,
        NDArray[np.float64] | Sequence[float | int] | float | int,
    ]
    scale: float
    offset: float
    sample_shift: int
    params: dict[str, Any] = field(default_factory=dict)


@dataclass
class SignalGenerator:
    n: int
    dt: float
    start: float = 0.0
    seed: int = 0
    components: list[SignalComponent] = field(default_factory=list)

    def __post_init__(self) -> None:
        if self.n < 1:
            raise ValueError("n must be positive")
        if not np.isfinite(self.dt) or self.dt <= 0.0:
            raise ValueError("dt must be finite and positive")
        if not np.isfinite(self.start):
            raise ValueError("start must be finite")

    @property
    def time(self) -> NDArray[np.float64]:
        return self.start + np.arange(self.n, dtype=np.float64) * self.dt

    def add(
        self,
        function: Callable[
            ...,
            NDArray[np.float64] | Sequence[float | int] | float | int,
        ],
        *,
        scale: float = 1.0,
        offset: float = 0.0,
        sample_shift: int = 0,
        **params: Any,
    ) -> SignalGenerator:
        if not np.isfinite(scale):
            raise ValueError("scale must be finite")
        if not np.isfinite(offset):
            raise ValueError("offset must be finite")
        self.components.append(
            SignalComponent(
                function=function,
                scale=float(scale),
                offset=float(offset),
                sample_shift=int(sample_shift),
                params=dict(params),
            )
        )
        return self

    def values(self) -> NDArray[np.float64]:
        time = self.time
        total = np.zeros(self.n, dtype=np.float64)
        rng = np.random.default_rng(self.seed)
        for component in self.components:
            shifted_time = time - component.sample_shift * self.dt
            values = _as_values(
                _call_signal_function(
                    component.function, shifted_time, rng, component.params
                ),
                self.n,
                component.function,
            )
            total += component.scale * values + component.offset
        return total

    def curve(self) -> NDArray[np.float64]:
        return np.column_stack((self.time, self.values())).astype(
            np.float64, copy=False
        )


def zero(
    time: NDArray[np.float64],
    *,
    rng: np.random.Generator | None = None,
) -> NDArray[np.float64]:
    del rng
    return np.zeros_like(time, dtype=np.float64)


def constant(
    time: NDArray[np.float64],
    *,
    value: float = 1.0,
    rng: np.random.Generator | None = None,
) -> NDArray[np.float64]:
    del rng
    return np.full_like(time, value, dtype=np.float64)


def ramp(
    time: NDArray[np.float64],
    *,
    slope: float = 1.0,
    intercept: float = 0.0,
    rng: np.random.Generator | None = None,
) -> NDArray[np.float64]:
    del rng
    return intercept + slope * time


def piecewise_ramp(
    time: NDArray[np.float64],
    *,
    breakpoints: Sequence[float] = (),
    slopes: Sequence[float] = (1.0,),
    start_value: float = 0.0,
    rng: np.random.Generator | None = None,
) -> NDArray[np.float64]:
    del rng
    if len(slopes) != len(breakpoints) + 1:
        raise ValueError("slopes must have one more entry than breakpoints")

    breaks = np.asarray(breakpoints, dtype=np.float64)
    if breaks.size > 1 and np.any(np.diff(breaks) <= 0.0):
        raise ValueError("breakpoints must be strictly increasing")

    values = np.full(time.shape, float(start_value), dtype=np.float64)
    previous_time = (
        float(time[0]) if breaks.size == 0 else min(float(time[0]), float(breaks[0]))
    )
    previous_value = float(start_value)
    segment_starts = np.concatenate(([previous_time], breaks))

    for index, slope in enumerate(slopes):
        segment_start = float(segment_starts[index])
        if index > 0:
            previous_break = float(breaks[index - 1])
            previous_value += float(slopes[index - 1]) * (
                previous_break - previous_time
            )
            previous_time = previous_break
        if index < len(slopes) - 1:
            mask = (time >= segment_start) & (time < breaks[index])
        else:
            mask = time >= segment_start
        values[mask] = previous_value + float(slope) * (time[mask] - segment_start)
    return values


def impulse(
    time: NDArray[np.float64],
    *,
    at: float = 0.0,
    amplitude: float = 1.0,
    width: float = 0.0,
    rng: np.random.Generator | None = None,
) -> NDArray[np.float64]:
    del rng
    values = np.zeros_like(time, dtype=np.float64)
    if time.size == 0:
        return values
    if width <= 0.0:
        values[int(np.argmin(np.abs(time - at)))] = amplitude
        return values
    values[np.abs(time - at) <= width / 2.0] = amplitude
    return values


def sparse_spikes(
    time: NDArray[np.float64],
    *,
    count: int = 3,
    amplitude: float = 1.0,
    positions: Sequence[float] | None = None,
    rng: np.random.Generator | None = None,
) -> NDArray[np.float64]:
    values = np.zeros_like(time, dtype=np.float64)
    if positions is not None:
        for position in positions:
            values[int(np.argmin(np.abs(time - float(position))))] = amplitude
        return values
    if rng is None:
        rng = np.random.default_rng(0)
    spike_count = min(max(int(count), 0), time.size)
    if spike_count == 0:
        return values
    indices = rng.choice(time.size, size=spike_count, replace=False)
    values[indices] = amplitude
    return values


def sine(
    time: NDArray[np.float64],
    *,
    frequency: float = 1.0,
    amplitude: float = 1.0,
    phase: float = 0.0,
    offset: float = 0.0,
    rng: np.random.Generator | None = None,
) -> NDArray[np.float64]:
    del rng
    return offset + amplitude * np.sin(2.0 * np.pi * frequency * time + phase)


def chirp(
    time: NDArray[np.float64],
    *,
    start_frequency: float = 1.0,
    end_frequency: float = 10.0,
    amplitude: float = 1.0,
    phase: float = 0.0,
    rng: np.random.Generator | None = None,
) -> NDArray[np.float64]:
    del rng
    duration = max(float(time[-1] - time[0]), np.finfo(np.float64).eps)
    tau = time - time[0]
    sweep = (end_frequency - start_frequency) / duration
    angle = 2.0 * np.pi * (start_frequency * tau + 0.5 * sweep * tau * tau) + phase
    return np.asarray(amplitude * np.sin(angle), dtype=np.float64)


def square_step(
    time: NDArray[np.float64],
    *,
    at: float = 0.0,
    low: float = 0.0,
    high: float = 1.0,
    rng: np.random.Generator | None = None,
) -> NDArray[np.float64]:
    del rng
    return np.where(time < at, low, high).astype(np.float64, copy=False)


def gaussian_noise(
    time: NDArray[np.float64],
    *,
    mean: float = 0.0,
    std: float = 1.0,
    rng: np.random.Generator | None = None,
) -> NDArray[np.float64]:
    if rng is None:
        rng = np.random.default_rng(0)
    return rng.normal(loc=mean, scale=std, size=time.shape).astype(
        np.float64, copy=False
    )


def ramp_impulses(
    time: NDArray[np.float64],
    *,
    slope: float = 1.0,
    intercept: float = 0.0,
    impulse_times: Sequence[float] = (),
    impulse_amplitude: float = 1.0,
    rng: np.random.Generator | None = None,
) -> NDArray[np.float64]:
    values = ramp(time, slope=slope, intercept=intercept, rng=rng)
    for impulse_time in impulse_times:
        values += impulse(
            time, at=float(impulse_time), amplitude=impulse_amplitude, rng=rng
        )
    return values


def piecewise_discontinuous(
    time: NDArray[np.float64],
    *,
    breakpoints: Sequence[float] = (),
    values: Sequence[float] = (0.0,),
    rng: np.random.Generator | None = None,
) -> NDArray[np.float64]:
    del rng
    if len(values) != len(breakpoints) + 1:
        raise ValueError("values must have one more entry than breakpoints")
    breaks = np.asarray(breakpoints, dtype=np.float64)
    levels = np.asarray(values, dtype=np.float64)
    indices = np.searchsorted(breaks, time, side="right")
    return levels[indices].astype(np.float64, copy=False)


def sine_noise(
    time: NDArray[np.float64],
    *,
    frequency: float = 1.0,
    amplitude: float = 1.0,
    noise_std: float = 0.1,
    phase: float = 0.0,
    rng: np.random.Generator | None = None,
) -> NDArray[np.float64]:
    return sine(
        time, frequency=frequency, amplitude=amplitude, phase=phase
    ) + gaussian_noise(
        time,
        std=noise_std,
        rng=rng,
    )


def _call_signal_function(
    function: Callable[
        ...,
        NDArray[np.float64] | Sequence[float | int] | float | int,
    ],
    time: NDArray[np.float64],
    rng: np.random.Generator,
    params: dict[str, Any],
) -> NDArray[np.float64] | Sequence[float | int] | float | int:
    signature = inspect.signature(function)
    accepts_rng = "rng" in signature.parameters or any(
        parameter.kind == inspect.Parameter.VAR_KEYWORD
        for parameter in signature.parameters.values()
    )
    if accepts_rng:
        return function(time, rng=rng, **params)
    return function(time, **params)


def _as_values(
    values: NDArray[np.float64] | Sequence[float | int] | float | int,
    n: int,
    function: Callable[
        ...,
        NDArray[np.float64] | Sequence[float | int] | float | int,
    ],
) -> NDArray[np.float64]:
    array = np.asarray(values, dtype=np.float64)
    if array.ndim == 0:
        return np.full(n, float(array), dtype=np.float64)
    if array.shape != (n,):
        name = getattr(function, "__name__", repr(function))
        raise ValueError(f"{name} returned shape {array.shape}, expected ({n},)")
    return array.astype(np.float64, copy=False)
