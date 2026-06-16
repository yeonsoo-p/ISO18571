from __future__ import annotations

import argparse
import subprocess
import sys


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run ISO/TS 18571 backend validation in fresh pytest processes.")
    parser.add_argument("--backends", nargs="+", default=("local_iso_native",), help="Backends to validate.")
    parser.add_argument("pytest_args", nargs=argparse.REMAINDER, help="Extra arguments passed to pytest.")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    for backend in args.backends:
        command = [
            sys.executable,
            "-m",
            "pytest",
            "-q",
            "tests/test_iso18571_backends.py",
            "--iso18571-backends",
            backend,
            *args.pytest_args,
        ]
        print(f"Validating {backend}: {' '.join(command)}", flush=True)
        subprocess.run(command, check=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

