from __future__ import annotations

import math
from typing import Literal, TypedDict

import numpy as np
from numpy.typing import NDArray


FloatArray = NDArray[np.float64]
Predecessor = Literal["vertical", "horizontal", "diagonal"]


class DtwFields(TypedDict):
    magnitude_numerator: float
    magnitude_denominator: float
    magnitude_error: float


class PhaseFields(TypedDict):
    phase_reference_start: int
    phase_comparison_start: int
    phase_shift_length: int
    phase_n_eps: int
    phase_rho_e: float


def dtw_magnitude_fields(
    comparison: FloatArray,
    reference: FloatArray,
    *,
    window_size: float = 0.1,
    squared_local_cost: bool = True,
    inclusive_boundary: bool = False,
    tie_order: tuple[Predecessor, Predecessor, Predecessor] = (
        "vertical",
        "horizontal",
        "diagonal",
    ),
) -> DtwFields:
    comparison_values = np.asarray(comparison, dtype=np.float64)
    reference_values = np.asarray(reference, dtype=np.float64)
    if comparison_values.shape != reference_values.shape:
        raise ValueError("comparison and reference must have matching shapes")
    if comparison_values.ndim != 1:
        raise ValueError("comparison and reference must be one-dimensional")

    n = comparison_values.shape[0]
    radius = min(n, max(1, math.ceil(window_size * n)))
    cost = np.full((n, n), math.inf, dtype=np.float64)
    numerator = np.zeros((n, n), dtype=np.float64)
    denominator = np.zeros((n, n), dtype=np.float64)

    for i in range(n):
        for j in range(n):
            if not _valid_dtw_cell(i, j, radius, inclusive_boundary):
                continue
            delta = comparison_values[i] - reference_values[j]
            local_cost = delta * delta if squared_local_cost else abs(delta)
            local_numerator = abs(delta)
            local_denominator = abs(reference_values[j])

            if i == 0 and j == 0:
                best_cost = 0.0
                best_numerator = 0.0
                best_denominator = 0.0
            else:
                best_cost = math.inf
                best_numerator = 0.0
                best_denominator = 0.0
                for predecessor in tie_order:
                    previous = _previous_cell(i, j, predecessor)
                    if previous is None:
                        continue
                    previous_i, previous_j = previous
                    candidate_cost = cost[previous_i, previous_j]
                    if candidate_cost < best_cost:
                        best_cost = candidate_cost
                        best_numerator = numerator[previous_i, previous_j]
                        best_denominator = denominator[previous_i, previous_j]
                if not math.isfinite(best_cost):
                    continue

            cost[i, j] = best_cost + local_cost
            numerator[i, j] = best_numerator + local_numerator
            denominator[i, j] = best_denominator + local_denominator

    final_numerator = float(numerator[n - 1, n - 1])
    final_denominator = float(denominator[n - 1, n - 1])
    final_error = (
        math.nan if final_denominator == 0.0 else final_numerator / final_denominator
    )
    return {
        "magnitude_numerator": final_numerator,
        "magnitude_denominator": final_denominator,
        "magnitude_error": final_error,
    }


def phase_alignment_fields(
    reference: FloatArray,
    comparison: FloatArray,
    *,
    init_min: float = 0.8,
) -> PhaseFields:
    reference_values = np.asarray(reference, dtype=np.float64)
    comparison_values = np.asarray(comparison, dtype=np.float64)
    if reference_values.shape != comparison_values.shape:
        raise ValueError("reference and comparison must have matching shapes")
    n = reference_values.shape[0]
    max_shift = round((1.0 - init_min) * 100.0) / 100.0
    bounded_window = min(math.floor(n * max_shift) + 1, n)

    best = _phase_candidate(reference_values, comparison_values, 0, 0, n, 0)
    best_rho = best["phase_rho_e"]
    if best_rho == 1.0:
        return best

    for shift in range(1, bounded_window):
        length = n - shift
        left = _phase_candidate(
            reference_values, comparison_values, 0, shift, length, shift
        )
        if left["phase_rho_e"] > best_rho + 1.0e-12:
            best = left
            best_rho = left["phase_rho_e"]
        right = _phase_candidate(
            reference_values, comparison_values, shift, 0, length, shift
        )
        if right["phase_rho_e"] > best_rho + 1.0e-12:
            best = right
            best_rho = right["phase_rho_e"]

    if best["phase_shift_length"] < 9:
        return _phase_candidate(reference_values, comparison_values, 0, 0, n, 0)
    return best


def _valid_dtw_cell(i: int, j: int, radius: int, inclusive_boundary: bool) -> bool:
    distance = abs(i - j)
    if inclusive_boundary:
        return distance <= radius
    return distance < radius


def _previous_cell(i: int, j: int, predecessor: Predecessor) -> tuple[int, int] | None:
    if predecessor == "vertical":
        return (i - 1, j) if i > 0 else None
    if predecessor == "horizontal":
        return (i, j - 1) if j > 0 else None
    return (i - 1, j - 1) if i > 0 and j > 0 else None


def _phase_candidate(
    reference: FloatArray,
    comparison: FloatArray,
    reference_start: int,
    comparison_start: int,
    length: int,
    n_eps: int,
) -> PhaseFields:
    reference_window = reference[reference_start : reference_start + length]
    comparison_window = comparison[comparison_start : comparison_start + length]
    rho_e = _correlation(reference_window, comparison_window)
    return {
        "phase_reference_start": reference_start,
        "phase_comparison_start": comparison_start,
        "phase_shift_length": length,
        "phase_n_eps": n_eps,
        "phase_rho_e": rho_e,
    }


def _correlation(reference: FloatArray, comparison: FloatArray) -> float:
    if reference.size < 2:
        return 1.0 if np.array_equal(reference, comparison) else 0.0
    reference_centered = reference - float(np.mean(reference))
    comparison_centered = comparison - float(np.mean(comparison))
    reference_norm = float(np.linalg.norm(reference_centered))
    comparison_norm = float(np.linalg.norm(comparison_centered))
    if reference_norm == 0.0 or comparison_norm == 0.0:
        return 1.0 if np.array_equal(reference, comparison) else 0.0
    rho = float(
        np.dot(reference_centered, comparison_centered)
        / (reference_norm * comparison_norm)
    )
    return min(1.0, max(-1.0, rho))
