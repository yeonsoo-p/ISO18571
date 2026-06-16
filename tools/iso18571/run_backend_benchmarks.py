from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

DEFAULT_BACKENDS = ("local_iso_native", "dtw_python", "librosa")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run ISO/TS 18571 backend benchmarks in fresh pytest processes.")
    parser.add_argument(
        "--output-dir", default=".benchmarks/iso18571", help="Directory for pytest-benchmark JSON files."
    )
    parser.add_argument("--backends", nargs="+", default=DEFAULT_BACKENDS, help="Backends to benchmark.")
    parser.add_argument("--max-time", default="5", help="pytest-benchmark --benchmark-max-time value.")
    parser.add_argument("--min-rounds", default="5", help="pytest-benchmark --benchmark-min-rounds value.")
    parser.add_argument("pytest_args", nargs=argparse.REMAINDER, help="Extra arguments passed to pytest.")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    for backend in args.backends:
        json_path = output_dir / f"iso18571_bench_{backend}.json"
        command = [
            sys.executable,
            "-m",
            "pytest",
            "-q",
            "tests/test_iso18571_benchmarks.py",
            "-o",
            "addopts=",
            "-m",
            "benchmark",
            "--iso18571-backends",
            backend,
            "--benchmark-min-rounds",
            args.min_rounds,
            "--benchmark-max-time",
            args.max_time,
            "--benchmark-warmup",
            "off",
            "--benchmark-json",
            str(json_path),
            *args.pytest_args,
        ]
        print(f"Running {backend}: {' '.join(command)}", flush=True)
        subprocess.run(command, check=True)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
