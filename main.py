from __future__ import annotations

import argparse
import json
from pathlib import Path

import numpy as np

from iso18571 import ISO18571


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Calculate ISO/TS 18571 scores for two curve CSV files.")
    parser.add_argument("reference", type=Path, help="Reference CSV with two columns: time,value")
    parser.add_argument("comparison", type=Path, help="Comparison CSV with two columns: time,value")
    parser.add_argument("--dt", type=float, default=0.0001, help="Sample period used for slope scoring")
    parser.add_argument("--delimiter", default=",", help="CSV delimiter")
    return parser.parse_args(argv)


def load_curve(path: Path, delimiter: str) -> np.ndarray:
    curve = np.loadtxt(path, delimiter=delimiter)
    if curve.ndim != 2 or curve.shape[1] != 2:
        raise ValueError(f"{path} must contain exactly two columns: time,value")
    return curve


def score_curves(reference: np.ndarray, comparison: np.ndarray, dt: float) -> dict[str, float]:
    iso = ISO18571(reference, comparison, dt=dt)
    return {
        "R": iso.overall_rating(),
        "Z": iso.corridor_rating(),
        "EP": iso.phase_rating(),
        "EM": iso.magnitude_rating(),
        "ES": iso.slope_rating(),
    }


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    scores = score_curves(
        load_curve(args.reference, args.delimiter),
        load_curve(args.comparison, args.delimiter),
        args.dt,
    )
    print(json.dumps(scores, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
