from __future__ import annotations

import argparse
import json
import math
from collections import defaultdict
from pathlib import Path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Analyze ISO18571 diagonal-parallel threshold benchmarks.")
    parser.add_argument("paths", nargs="+", help="pytest-benchmark JSON files from threshold benchmark runs.")
    return parser.parse_args()


def _param_text(name: str) -> str | None:
    if "[" not in name or not name.endswith("]"):
        return None
    return name.rsplit("[", 1)[1][:-1]


def _cells(n: int) -> int:
    radius = max(1, math.ceil(0.1 * n))
    return n * (2 * radius - 1)


def load_rows(paths: list[str]) -> tuple[dict[tuple[str, int, int], float], dict[tuple[int, int], float]]:
    timings: dict[tuple[str, int, int], float] = {}
    overheads: dict[tuple[int, int], float] = {}

    for raw_path in paths:
        data = json.loads(Path(raw_path).read_text())
        for benchmark in data["benchmarks"]:
            name = benchmark["name"]
            params = _param_text(name)
            if params is None:
                continue
            median = float(benchmark["stats"]["median"])

            if name.startswith("test_diagonal_parallel_threshold_speed"):
                thread_text, n_text, family = params.split("-", 2)
                timings[(family, int(n_text), int(thread_text))] = median
            elif name.startswith("test_parallel_barrier_overhead"):
                thread_text, n_text = params.split("-", 1)
                overheads[(int(n_text), int(thread_text))] = median

    return timings, overheads


def main() -> int:
    args = parse_args()
    timings, overheads = load_rows(args.paths)
    families_by_n: dict[int, set[str]] = defaultdict(set)
    threads_by_n: dict[int, set[int]] = defaultdict(set)
    for family, n, threads in timings:
        families_by_n[n].add(family)
        threads_by_n[n].add(threads)

    print("| n | cells | threads | families passing | max regression | min speedup | max net saved | decision |")
    print("|---:|---:|---:|---:|---:|---:|---:|---|")

    accepted: tuple[int, int] | None = None
    for n in sorted(families_by_n):
        families = sorted(families_by_n[n])
        serial = {family: timings[(family, n, 1)] for family in families if (family, n, 1) in timings}
        if len(serial) != len(families):
            continue
        for threads in sorted(thread for thread in threads_by_n[n] if thread > 1):
            pass_count = 0
            max_regression = 0.0
            min_speedup = float("inf")
            max_net_saved = float("-inf")
            overhead = overheads.get((n, threads), 0.0)

            for family in families:
                parallel = timings.get((family, n, threads))
                if parallel is None:
                    continue
                base = serial[family]
                speedup = base / parallel if parallel > 0 else float("inf")
                regression = (parallel - base) / base if parallel > base else 0.0
                net_saved = base - parallel - overhead
                max_regression = max(max_regression, regression)
                min_speedup = min(min_speedup, speedup)
                max_net_saved = max(max_net_saved, net_saved)
                if speedup >= 1.10 and net_saved > 0.0:
                    pass_count += 1

            accepted_here = pass_count == len(families) and max_regression <= 0.03
            decision = "accept" if accepted_here else "reject"
            if accepted_here and accepted is None:
                accepted = (n, threads)
            print(
                f"| {n} | {_cells(n)} | {threads} | {pass_count}/{len(families)} | "
                f"{max_regression * 100.0:.2f}% | {min_speedup:.3f}x | "
                f"{max_net_saved * 1000.0:.3f} ms | {decision} |"
            )

    if accepted is None:
        print("\nNo production parallel threshold satisfies the acceptance rules.")
    else:
        n, threads = accepted
        print(f"\nRecommended production threshold: n >= {n}, cells >= {_cells(n)}, threads={threads}.")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
