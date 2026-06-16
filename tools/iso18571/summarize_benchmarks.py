from __future__ import annotations

import argparse
import json
from pathlib import Path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Summarize ISO/TS 18571 pytest-benchmark JSON files.")
    parser.add_argument("paths", nargs="+", help="Benchmark JSON files.")
    return parser.parse_args()


def backend_from_name(name: str) -> str:
    if "[" not in name or not name.endswith("]"):
        return name
    return name.rsplit("[", 1)[1][:-1]


def main() -> int:
    args = parse_args()
    rows: dict[str, dict[str, float | int]] = {}

    for raw_path in args.paths:
        path = Path(raw_path)
        data = json.loads(path.read_text())
        for benchmark in data["benchmarks"]:
            backend = backend_from_name(benchmark["name"])
            row = rows.setdefault(backend, {})
            stats = benchmark["stats"]
            if "first_use" in benchmark["name"]:
                row["first_use_s"] = stats["median"]
                row["first_use_rounds"] = stats["rounds"]
            elif "steady_state" in benchmark["name"]:
                row["steady_s"] = stats["median"]
                row["steady_rounds"] = stats["rounds"]

    print("| Backend | First-use pass | Inferred prep | Steady median pass | Steady rounds |")
    print("|---|---:|---:|---:|---:|")
    for backend, row in sorted(rows.items()):
        first = float(row.get("first_use_s", float("nan")))
        steady = float(row.get("steady_s", float("nan")))
        prep = first - steady
        rounds = int(row.get("steady_rounds", 0))
        print(f"| `{backend}` | {first:.4f}s | {prep:.4f}s | {steady:.4f}s | {rounds} |")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())

