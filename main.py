"""Minimal runnable ISO/TS 18571 CSV scoring example.

Run this file from the repository root with:

```
uv run python main.py
```

The example CSVs are numeric files with exactly two columns and no header:
``time,value``. The reference and comparison time columns must match, be
strictly increasing, and have a uniform sample interval.
"""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any

import numpy as np

from iso18571 import ISO18571, ScoreComponents, ScoreTimings, backend_info

REFERENCE_CSV = Path("examples/reference.csv")
COMPARISON_CSV = Path("examples/comparison.csv")


def load_curve(path: Path, delimiter: str) -> np.ndarray:
    """Load a numeric (n, 2) curve with time in column 0 and signal in column 1."""
    curve = np.loadtxt(path, delimiter=delimiter)
    if curve.ndim != 2 or curve.shape[1] != 2:
        raise ValueError(f"{path} must contain exactly two columns: time,value")
    return curve


def rounded_scores(iso: ISO18571) -> dict[str, float]:
    """Return rounded ISO/TS 18571 component scores."""
    return {
        "R": float(iso.overall_rating()),
        "Z": float(iso.corridor_rating()),
        "EP": float(iso.phase_rating()),
        "EM": float(iso.magnitude_rating()),
        "ES": float(iso.slope_rating()),
    }


def json_score_value(value: float | int) -> float | int:
    if isinstance(value, int):
        return int(value)
    return float(value)


def score_values(scores: ScoreComponents) -> dict[str, float | int]:
    """Return JSON-ready raw component scores and diagnostic fields."""
    return {
        "R": json_score_value(scores["R"]),
        "Z": json_score_value(scores["Z"]),
        "EP": json_score_value(scores["EP"]),
        "EM": json_score_value(scores["EM"]),
        "ES": json_score_value(scores["ES"]),
        "dt": json_score_value(scores["dt"]),
        "corridor_t_norm": json_score_value(scores["corridor_t_norm"]),
        "corridor_inner_half_width": json_score_value(
            scores["corridor_inner_half_width"]
        ),
        "corridor_outer_half_width": json_score_value(
            scores["corridor_outer_half_width"]
        ),
        "phase_n_eps": json_score_value(scores["phase_n_eps"]),
        "phase_rho_e": json_score_value(scores["phase_rho_e"]),
        "phase_reference_start": json_score_value(scores["phase_reference_start"]),
        "phase_comparison_start": json_score_value(scores["phase_comparison_start"]),
        "phase_shift_length": json_score_value(scores["phase_shift_length"]),
        "phase_max_shift": json_score_value(scores["phase_max_shift"]),
        "magnitude_numerator": json_score_value(scores["magnitude_numerator"]),
        "magnitude_denominator": json_score_value(scores["magnitude_denominator"]),
        "magnitude_error": json_score_value(scores["magnitude_error"]),
        "slope_numerator": json_score_value(scores["slope_numerator"]),
        "slope_denominator": json_score_value(scores["slope_denominator"]),
        "slope_error": json_score_value(scores["slope_error"]),
    }


def timing_values(timings: ScoreTimings) -> dict[str, float]:
    """Return native scorer timing values in milliseconds."""
    return {
        "corridor": float(timings["corridor_ms"]),
        "phase": float(timings["phase_ms"]),
        "magnitude": float(timings["magnitude_ms"]),
        "slope": float(timings["slope_ms"]),
        "total": float(timings["total_ms"]),
    }


def score_report(reference: np.ndarray, comparison: np.ndarray) -> dict[str, Any]:
    """Score two curves and return the values a new user usually inspects."""
    iso = ISO18571(reference, comparison)
    return {
        "backend": backend_info(),
        "input": {
            "samples": int(reference.shape[0]),
            "sample_interval": float(reference[1, 0] - reference[0, 0]),
        },
        "scores": score_values(iso.scores),
        "rounded_scores": rounded_scores(iso),
        "timings_ms": timing_values(iso.timings),
    }


def main() -> int:
    report = score_report(
        load_curve(REFERENCE_CSV, ","),
        load_curve(COMPARISON_CSV, ","),
    )
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
