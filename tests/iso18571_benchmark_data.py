from __future__ import annotations

from dataclasses import dataclass
from typing import cast

import numpy as np
import numpy.typing as npt

BENCHMARK_BACKENDS = ("native", "dtwalign", "dtw_python", "librosa")
BENCHMARK_LENGTHS = (512, 2048, 8192, 32768)

FloatArray = npt.NDArray[np.float64]


@dataclass(frozen=True)
class BenchmarkSignal:
    reference_curve: FloatArray
    comparison_curve: FloatArray
    dt: float


def make_mixed_signal(length: int) -> BenchmarkSignal:
    if length < 9:
        raise ValueError("benchmark length must be at least 9")

    dt = 0.0001
    rng = np.random.default_rng(18571 + length)
    time = np.arange(length, dtype=np.float64) * dt
    tau = np.linspace(0.0, 1.0, length, endpoint=False, dtype=np.float64)

    smooth = 0.55 * np.sin(2.0 * np.pi * 4.0 * tau)
    chirp = 0.22 * np.sin(2.0 * np.pi * (1.5 * tau + 7.0 * tau * tau))
    step = 0.18 * np.tanh((tau - 0.42) / 0.025) - 0.12 * np.tanh((tau - 0.71) / 0.035)
    ramp = 0.12 * (tau - 0.5)
    reference = smooth + chirp + step + ramp

    spike_count = min(17, max(3, length // 512))
    spike_idx = np.linspace(8, length - 9, spike_count, dtype=np.int64)
    reference[spike_idx] += np.linspace(0.24, -0.20, spike_count, dtype=np.float64)

    shifted_tau = tau - 0.006
    comparison = (
        1.035 * 0.55 * np.sin(2.0 * np.pi * 4.0 * shifted_tau)
        + 0.22
        * np.sin(2.0 * np.pi * (1.5 * shifted_tau + 7.0 * shifted_tau * shifted_tau))
        + step
        + 0.10 * (tau - 0.5)
        + 0.025
        + rng.normal(0.0, 0.006, length)
    )
    comparison[np.minimum(length - 1, spike_idx + 2)] += np.linspace(
        0.18, -0.16, spike_count, dtype=np.float64
    )

    return BenchmarkSignal(
        reference_curve=cast(FloatArray, np.column_stack((time, reference))),
        comparison_curve=cast(FloatArray, np.column_stack((time, comparison))),
        dt=dt,
    )
