from __future__ import annotations

from dataclasses import dataclass

import numpy as np


@dataclass(frozen=True)
class SignalCase:
    family: str
    reference: np.ndarray
    comparison: np.ndarray


SIGNAL_FAMILIES = (
    "zero",
    "constant",
    "ramp",
    "piecewise_ramp",
    "impulse",
    "sparse_spikes",
    "sine_phase",
    "sine_amp_offset",
    "chirp",
    "square_step",
    "gaussian_noise",
    "sine_noise",
    "ramp_impulses",
    "piecewise_discontinuous",
)

PHASE_SHIFT_SIGNAL_FAMILIES = (
    "phase_multitone_shift_005",
    "phase_multitone_shift_020",
    "phase_chirp_shift_050",
    "phase_pulses_shift_100",
    "phase_smooth_step_shift_180",
)


def curve(values: np.ndarray, dt: float = 0.0001) -> np.ndarray:
    time = np.arange(values.shape[0], dtype=np.float64) * dt
    return np.column_stack((time, values.astype(np.float64, copy=False)))


def signal_case(family: str, n: int) -> SignalCase:
    rng = np.random.default_rng(18571 + n)
    t = np.linspace(0.0, 1.0, n, endpoint=False, dtype=np.float64)

    if family == "zero":
        reference = np.zeros(n, dtype=np.float64)
        comparison = np.zeros(n, dtype=np.float64)
    elif family == "constant":
        reference = np.full(n, 2.0, dtype=np.float64)
        comparison = np.full(n, 2.1, dtype=np.float64)
    elif family == "ramp":
        reference = np.linspace(-1.0, 1.0, n, dtype=np.float64)
        comparison = reference * 1.03 + 0.02 + 0.005 * np.sin(2.0 * np.pi * 3.0 * t)
    elif family == "piecewise_ramp":
        reference = np.where(t < 0.35, 2.0 * t, 0.7 - 1.5 * (t - 0.35))
        comparison = reference + np.where(t > 0.55, 0.08, -0.03)
    elif family == "impulse":
        reference = np.zeros(n, dtype=np.float64)
        comparison = np.zeros(n, dtype=np.float64)
        reference[n // 3] = 1.0
        comparison[min(n - 1, n // 3 + max(1, n // 80))] = 0.92
    elif family == "sparse_spikes":
        reference = np.zeros(n, dtype=np.float64)
        comparison = np.zeros(n, dtype=np.float64)
        idx = np.linspace(1, n - 2, min(9, max(2, n // 16)), dtype=np.int64)
        reference[idx] = np.linspace(0.4, 1.2, idx.shape[0])
        comparison[np.minimum(n - 1, idx + 1)] = reference[idx] * 0.95
    elif family == "sine_phase":
        reference = np.sin(2.0 * np.pi * 5.0 * t)
        comparison = np.sin(2.0 * np.pi * 5.0 * (t - 0.015))
    elif family == "sine_amp_offset":
        reference = np.sin(2.0 * np.pi * 3.0 * t)
        comparison = 1.12 * reference + 0.08
    elif family == "chirp":
        reference = np.sin(2.0 * np.pi * (2.0 * t + 9.0 * t * t))
        comparison = np.sin(2.0 * np.pi * (2.0 * (t - 0.006) + 9.0 * (t - 0.006) ** 2))
    elif family == "square_step":
        reference = np.where(np.sin(2.0 * np.pi * 4.0 * t) >= 0.0, 1.0, -1.0) + 1e-6 * t
        comparison = np.where(np.sin(2.0 * np.pi * 4.0 * (t - 0.01)) >= 0.0, 0.9, -1.1) + 1e-6 * t
    elif family == "gaussian_noise":
        reference = rng.normal(0.0, 1.0, n)
        comparison = 0.92 * reference + rng.normal(0.0, 0.15, n)
    elif family == "sine_noise":
        reference = np.sin(2.0 * np.pi * 6.0 * t)
        comparison = 0.98 * np.sin(2.0 * np.pi * 6.0 * (t - 0.008)) + rng.normal(0.0, 0.04, n)
    elif family == "ramp_impulses":
        reference = np.linspace(0.0, 1.0, n, dtype=np.float64) + 0.03 * (t - 0.5) ** 2
        comparison = reference.copy()
        idx = np.linspace(2, n - 3, min(7, max(2, n // 32)), dtype=np.int64)
        comparison[idx] += np.linspace(0.2, -0.2, idx.shape[0])
    elif family == "piecewise_discontinuous":
        reference = np.piecewise(
            t,
            [t < 0.25, (t >= 0.25) & (t < 0.65), t >= 0.65],
            [lambda x: 2.0 * x, 0.75, lambda x: 0.75 - x],
        )
        comparison = reference + np.where(t > 0.5, -0.07, 0.04)
    else:
        raise ValueError(f"Unknown signal family {family}")

    return SignalCase(family=family, reference=curve(reference), comparison=curve(comparison))


def _phase_multitone(t: np.ndarray) -> np.ndarray:
    return (
        0.65 * np.sin(2.0 * np.pi * 3.0 * t)
        + 0.25 * np.sin(2.0 * np.pi * 7.0 * t + 0.35)
        + 0.10 * np.cos(2.0 * np.pi * 11.0 * t - 0.2)
    )


def _phase_chirp(t: np.ndarray) -> np.ndarray:
    return np.sin(2.0 * np.pi * (1.5 * t + 10.0 * t * t))


def _phase_pulses(t: np.ndarray) -> np.ndarray:
    centers = np.asarray([0.18, 0.41, 0.73], dtype=np.float64)
    widths = np.asarray([0.012, 0.025, 0.018], dtype=np.float64)
    amplitudes = np.asarray([1.0, -0.75, 0.55], dtype=np.float64)
    values = np.zeros_like(t)
    for center, width, amplitude in zip(centers, widths, amplitudes, strict=True):
        values += amplitude * np.exp(-0.5 * ((t - center) / width) ** 2)
    return values


def _phase_smooth_step(t: np.ndarray) -> np.ndarray:
    return 0.75 * np.tanh((t - 0.38) / 0.025) - 0.45 * np.tanh((t - 0.68) / 0.035)


def analytic_phase_signal_case(family: str, n: int) -> SignalCase:
    t = np.linspace(0.0, 1.0, n, endpoint=False, dtype=np.float64)
    if family == "phase_multitone_shift_005":
        shift = 0.005
        reference = _phase_multitone(t)
        comparison = _phase_multitone(t - shift)
    elif family == "phase_multitone_shift_020":
        shift = 0.020
        reference = _phase_multitone(t)
        comparison = _phase_multitone(t - shift)
    elif family == "phase_chirp_shift_050":
        shift = 0.050
        reference = _phase_chirp(t)
        comparison = _phase_chirp(t - shift)
    elif family == "phase_pulses_shift_100":
        shift = 0.100
        reference = _phase_pulses(t)
        comparison = _phase_pulses(t - shift)
    elif family == "phase_smooth_step_shift_180":
        shift = 0.180
        reference = _phase_smooth_step(t)
        comparison = _phase_smooth_step(t - shift)
    else:
        raise ValueError(f"Unknown analytic phase signal family {family}")

    return SignalCase(family=family, reference=curve(reference), comparison=curve(comparison))
