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

from iso18571 import ISO18571, backend_info

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


def score_report(reference: np.ndarray, comparison: np.ndarray) -> dict[str, Any]:
    """Score two curves and return the values a new user usually inspects."""
    iso = ISO18571(reference, comparison)
    return {
        "backend": backend_info(),
        "input": {
            "samples": int(reference.shape[0]),
            "sample_interval": float(reference[1, 0] - reference[0, 0]),
        },
        "scores": {key: json_score_value(value) for key, value in iso.scores.items()},
        "rounded_scores": rounded_scores(iso),
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
