"""Composable signal generation helpers for ISO18571 tests and examples."""

from __future__ import annotations

import inspect
from collections.abc import Callable, Sequence
from dataclasses import dataclass, field
from typing import Any, Literal, Protocol, TypeAlias

import numpy as np
from numpy.typing import DTypeLike, NDArray


TimeCase: TypeAlias = Literal[
    "affine",
    "signed_endpoint_overflow",
    "long_signed",
    "wrapped_integer",
    "float_endpoint_span_overflow",
]


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
    _time_axis: NDArray[Any] | None = field(default=None, init=False, repr=False)
    _time_dtype: np.dtype[Any] | None = field(default=None, init=False, repr=False)
    _curve_dtype: np.dtype[Any] | None = field(default=None, init=False, repr=False)

    def __post_init__(self) -> None:
        if self.n < 1:
            raise ValueError("n must be positive")
        if not np.isfinite(self.dt) or self.dt <= 0.0:
            raise ValueError("dt must be finite and positive")
        if not np.isfinite(self.start):
            raise ValueError("start must be finite")

    @property
    def time(self) -> NDArray[Any]:
        if self._time_axis is not None:
            return self._time_axis.copy()

        time = self.start + np.arange(self.n, dtype=np.float64) * self.dt
        if self._time_dtype is not None:
            return time.astype(self._time_dtype, copy=False)
        return time

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

    def with_time_axis(
        self,
        time: NDArray[Any] | Sequence[float | int | complex],
        *,
        allow_invalid: bool = False,
    ) -> SignalGenerator:
        axis = np.asarray(time)
        if axis.shape != (self.n,):
            raise ValueError(f"time axis must have shape ({self.n},)")
        _require_supported_time_dtype(axis.dtype)
        if not allow_invalid:
            _require_valid_time_axis(axis)

        self._time_axis = axis.copy()
        self._time_dtype = axis.dtype
        self._curve_dtype = axis.dtype
        return self

    def with_time_dtype(self, dtype: DTypeLike) -> SignalGenerator:
        time_dtype = _normalize_dtype(dtype)
        _require_supported_time_dtype(time_dtype)
        self._time_dtype = time_dtype
        self._curve_dtype = time_dtype
        if self._time_axis is not None:
            self._time_axis = self._time_axis.astype(time_dtype, copy=False)
            _require_valid_time_axis(self._time_axis)
        return self

    def with_curve_dtype(self, dtype: DTypeLike) -> SignalGenerator:
        curve_dtype = _normalize_dtype(dtype)
        _require_supported_time_dtype(curve_dtype)
        self._curve_dtype = curve_dtype
        return self

    def with_time_case(
        self,
        case: TimeCase,
        dtype: DTypeLike | None = None,
        *,
        allow_invalid: bool = False,
    ) -> SignalGenerator:
        if case == "affine":
            if dtype is None:
                self._time_axis = None
                self._time_dtype = None
                self._curve_dtype = None
                return self
            return self.with_time_dtype(dtype)

        if dtype is None:
            if case == "signed_endpoint_overflow":
                time_dtype = _normalize_dtype(np.int64)
            elif case == "long_signed":
                time_dtype = _normalize_dtype(np.int16)
            elif case == "wrapped_integer":
                time_dtype = _normalize_dtype(np.int8)
            elif case == "float_endpoint_span_overflow":
                time_dtype = _normalize_dtype(np.float64)
            else:
                raise ValueError(f"Unsupported time case: {case}")
        else:
            time_dtype = _normalize_dtype(dtype)
        if case == "signed_endpoint_overflow":
            if not np.issubdtype(time_dtype, np.signedinteger):
                raise ValueError(
                    "signed_endpoint_overflow requires a signed integer dtype"
                )
            if self.n < 2:
                raise ValueError("signed_endpoint_overflow requires at least 2 samples")
            info = np.iinfo(time_dtype)
            denominator = self.n - 1
            step = int(abs(info.min)) // denominator
            if int(abs(info.min)) % denominator != 0:
                step += 1
            last = int(info.min) + step * denominator
            if last > int(info.max):
                raise ValueError(
                    "signed_endpoint_overflow does not fit in the requested dtype"
                )
            time = np.array(
                [int(info.min) + step * idx for idx in range(self.n)], dtype=time_dtype
            )
            return self.with_time_axis(time)

        if case == "long_signed":
            if not np.issubdtype(time_dtype, np.signedinteger):
                raise ValueError("long_signed requires a signed integer dtype")
            info = np.iinfo(time_dtype)
            last = int(info.min) + self.n - 1
            if last > int(info.max):
                raise ValueError("long_signed does not fit in the requested dtype")
            time = np.arange(int(info.min), last + 1, dtype=time_dtype)
            return self.with_time_axis(time)

        if case == "wrapped_integer":
            if not np.issubdtype(time_dtype, np.integer):
                raise ValueError("wrapped_integer requires an integer dtype")
            if not allow_invalid:
                raise ValueError("wrapped_integer requires allow_invalid=True")
            time = np.arange(self.n, dtype=np.int64).astype(time_dtype, copy=False)
            return self.with_time_axis(time, allow_invalid=True)

        if case == "float_endpoint_span_overflow":
            if time_dtype not in (np.dtype(np.float32), np.dtype(np.float64)):
                raise ValueError(
                    "float_endpoint_span_overflow requires float32 or float64"
                )
            if self.n < 3:
                raise ValueError(
                    "float_endpoint_span_overflow requires at least 3 samples"
                )
            scale = 0.9 * float(np.finfo(time_dtype).max)
            positions = np.linspace(-1.0, 1.0, self.n, dtype=np.float64)
            time = np.asarray(positions * scale, dtype=time_dtype)
            if not np.all(np.isfinite(np.diff(time))):
                raise ValueError(
                    "float_endpoint_span_overflow needs more samples for finite adjacent steps"
                )
            return self.with_time_axis(time)

        raise ValueError(f"Unsupported time case: {case}")

    def with_random_time_case(
        self, seed: int | None = None, *, include_invalid: bool = True
    ) -> SignalGenerator:
        rng = np.random.default_rng(self.seed if seed is None else seed)
        choices: list[tuple[TimeCase, DTypeLike, bool]] = [
            ("affine", np.float64, False),
            ("signed_endpoint_overflow", np.int32, False),
            ("signed_endpoint_overflow", np.int64, False),
            ("long_signed", np.int16, False),
            ("float_endpoint_span_overflow", np.float32, False),
            ("float_endpoint_span_overflow", np.float64, False),
        ]
        if include_invalid:
            choices.append(("wrapped_integer", np.int8, True))
        case, dtype, allow_invalid = choices[int(rng.integers(len(choices)))]
        return self.with_time_case(case, dtype, allow_invalid=allow_invalid)

    def values(self) -> NDArray[np.float64]:
        time = self.time.astype(np.float64, copy=False)
        total = np.zeros(self.n, dtype=np.float64)
        rng = np.random.default_rng(self.seed)
        for component in self.components:
            shifted_time = time - component.sample_shift * self.dt
            signature = inspect.signature(component.function)
            accepts_rng = "rng" in signature.parameters or any(
                parameter.kind == inspect.Parameter.VAR_KEYWORD
                for parameter in signature.parameters.values()
            )
            if accepts_rng:
                raw_values = component.function(
                    shifted_time, rng=rng, **component.params
                )
            else:
                raw_values = component.function(shifted_time, **component.params)

            values = np.asarray(raw_values, dtype=np.float64)
            if values.ndim == 0:
                values = np.full(self.n, float(values), dtype=np.float64)
            elif values.shape != (self.n,):
                name = getattr(component.function, "__name__", repr(component.function))
                raise ValueError(
                    f"{name} returned shape {values.shape}, expected ({self.n},)"
                )
            else:
                values = values.astype(np.float64, copy=False)
            total += component.scale * values + component.offset
        return total

    def curve(self) -> NDArray[Any]:
        curve = np.column_stack((self.time, self.values()))
        if self._curve_dtype is not None:
            return curve.astype(self._curve_dtype, copy=False)
        return curve.astype(np.float64, copy=False)


def zero(
    time: NDArray[np.float64],
    *,
    rng: np.random.Generator | None = None,
) -> NDArray[np.float64]:
    del rng
    return np.zeros_like(time, dtype=np.float64)


def sample_index(
    time: NDArray[np.float64],
    *,
    rng: np.random.Generator | None = None,
) -> NDArray[np.float64]:
    del rng
    return np.arange(time.shape[0], dtype=np.float64)


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


def _normalize_dtype(dtype: DTypeLike) -> np.dtype[Any]:
    return np.dtype(dtype)


def _require_supported_time_dtype(dtype: np.dtype[Any]) -> None:
    if not (
        np.issubdtype(dtype, np.integer)
        or np.issubdtype(dtype, np.floating)
        or np.issubdtype(dtype, np.complexfloating)
    ):
        raise ValueError("time axis dtype must be numeric")
    if np.issubdtype(dtype, np.bool_):
        raise ValueError("time axis dtype must be numeric")


def _require_valid_time_axis(time: NDArray[Any]) -> None:
    if np.issubdtype(time.dtype, np.complexfloating):
        if np.any(np.imag(time) != 0):
            raise ValueError("time axis must have zero imaginary components")
        real_time = np.real(time)
    else:
        real_time = time

    if not np.all(np.isfinite(real_time)):
        raise ValueError("time axis must be finite")
    previous = real_time[0].item()
    for value in real_time[1:]:
        current = value.item()
        if current <= previous:
            raise ValueError("time axis must be strictly increasing")
        previous = current
