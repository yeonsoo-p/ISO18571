from __future__ import annotations

import argparse
import json
import math
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path

BASELINE_VARIANT = "dtw_current+reduce_none+parallel_none+simd_scalar"


@dataclass(frozen=True)
class Row:
    family: str
    n: int
    effective_n: int
    cells: int
    variant: str
    requested_simd_level: str
    selected_simd_level: str
    simd_fallback: bool
    max_threads: int
    median: float
    iqr: float


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Summarize ISO18571 native variant performance by data-size regime.")
    parser.add_argument("paths", nargs="+", help="pytest-benchmark JSON files from regime benchmark runs.")
    return parser.parse_args()


def cells(effective_n: int) -> int:
    radius = max(1, math.ceil(0.1 * effective_n))
    return effective_n * (2 * radius - 1)


def regime(effective_n: int) -> str:
    if effective_n <= 129:
        return "tiny"
    if effective_n <= 1430:
        return "annex_like"
    if effective_n <= 8192:
        return "medium"
    if effective_n <= 32768:
        return "large"
    return "very_large"


def param_parts(name: str) -> dict[str, str]:
    if "[" not in name or not name.endswith("]"):
        return {}
    raw = name.rsplit("[", 1)[1][:-1]
    pieces = raw.split("__")
    if len(pieces) != 4:
        return {}
    family, n_text, variant, threads_text = pieces
    return {
        "family": family,
        "n": n_text.removeprefix("n"),
        "variant": variant,
        "max_threads": threads_text.removeprefix("t"),
    }


def load_rows(paths: list[str]) -> list[Row]:
    rows: list[Row] = []
    for raw_path in paths:
        data = json.loads(Path(raw_path).read_text())
        for benchmark in data["benchmarks"]:
            name = benchmark["name"]
            if not name.startswith("test_native_score_component_variant_regime_speed"):
                continue
            stats = benchmark["stats"]
            extra = dict(benchmark.get("extra_info") or {})
            parsed = param_parts(name)
            family = str(extra.get("family", parsed.get("family", "")))
            n = int(extra.get("n", parsed.get("n", "0")))
            effective_n = int(extra.get("effective_n", n))
            variant = str(extra.get("variant", parsed.get("variant", "")))
            requested_simd_level = str(extra.get("requested_simd_level", "scalar"))
            selected_simd_level = str(extra.get("selected_simd_level", requested_simd_level))
            raw_fallback = extra.get("simd_fallback", False)
            simd_fallback = raw_fallback if isinstance(raw_fallback, bool) else str(raw_fallback).lower() == "true"
            max_threads = int(extra.get("max_threads", parsed.get("max_threads", "1")))
            q1 = float(stats.get("q1", stats["median"]))
            q3 = float(stats.get("q3", stats["median"]))
            rows.append(
                Row(
                    family=family,
                    n=n,
                    effective_n=effective_n,
                    cells=int(extra.get("cells", cells(effective_n))),
                    variant=variant,
                    requested_simd_level=requested_simd_level,
                    selected_simd_level=selected_simd_level,
                    simd_fallback=simd_fallback,
                    max_threads=max_threads,
                    median=float(stats["median"]),
                    iqr=max(0.0, q3 - q1),
                )
            )
    return rows


def classify_rows(rows: list[Row]) -> dict[Row, tuple[str, float]]:
    by_case: dict[tuple[str, int], list[Row]] = defaultdict(list)
    for row in rows:
        by_case[(row.family, row.n)].append(row)

    out: dict[Row, tuple[str, float]] = {}
    for case_rows in by_case.values():
        baseline_candidates = [row for row in case_rows if row.variant == BASELINE_VARIANT and row.max_threads == 1]
        baseline = baseline_candidates[0].median if baseline_candidates else min(row.median for row in case_rows)
        best = min(row.median for row in case_rows)
        best_iqr = max(row.iqr for row in case_rows if row.median == best)
        tie_band = max(best_iqr, best * 0.03)
        competitive_band = max(best_iqr, best * 0.05)
        for row in case_rows:
            ratio = row.median / baseline if baseline > 0 else float("nan")
            if row.median <= best + tie_band:
                status = "preferred"
            elif row.median <= best + competitive_band:
                status = "competitive"
            else:
                status = "dominated"
            out[row] = (status, ratio)
    return out


def summarize_regimes(
    rows: list[Row], classifications: dict[Row, tuple[str, float]]
) -> list[tuple[str, str, int, str, int, float]]:
    grouped: dict[tuple[str, str, int], list[Row]] = defaultdict(list)
    for row in rows:
        grouped[(regime(row.effective_n), row.variant, row.max_threads)].append(row)

    summary = []
    for key, group_rows in grouped.items():
        statuses = [classifications[row][0] for row in group_rows]
        ratios = [classifications[row][1] for row in group_rows]
        if all(status in {"preferred", "competitive"} for status in statuses):
            label = "preferred" if "preferred" in statuses else "competitive"
        elif "preferred" in statuses or "competitive" in statuses:
            label = "unstable"
        else:
            label = "dominated"
        summary.append((*key, label, len(group_rows), sum(ratios) / len(ratios)))
    return sorted(summary)


def dispatch_threshold_candidates(
    rows: list[Row], classifications: dict[Row, tuple[str, float]]
) -> list[tuple[int, int, str, int, int, float]]:
    blocked_keys = sorted(
        {(row.variant, row.max_threads) for row in rows if "blocked" in row.variant and row.max_threads > 1}
    )
    thresholds = sorted({row.effective_n for row in rows})
    candidates = []
    for variant, threads in blocked_keys:
        variant_rows = [row for row in rows if row.variant == variant and row.max_threads == threads]
        for threshold in thresholds:
            covered_rows = [row for row in variant_rows if row.effective_n >= threshold]
            expected_cases = {(row.family, row.n) for row in rows if row.effective_n >= threshold}
            covered_cases = {(row.family, row.n) for row in covered_rows}
            if not expected_cases or covered_cases != expected_cases:
                continue
            statuses = [classifications[row][0] for row in covered_rows]
            ratios = [classifications[row][1] for row in covered_rows]
            if all(status in {"preferred", "competitive"} for status in statuses) and all(
                ratio < 1.0 for ratio in ratios
            ):
                candidates.append(
                    (
                        threshold,
                        cells(threshold),
                        variant,
                        threads,
                        len(covered_rows),
                        sum(ratios) / len(ratios),
                    )
                )
                break
    return sorted(candidates)


def main() -> int:
    args = parse_args()
    rows = load_rows(args.paths)
    if not rows:
        raise SystemExit("No regime benchmark rows found.")
    classifications = classify_rows(rows)

    print(
        "| effective_n | cells | family | variant | requested_simd | selected_simd | fallback | threads | median_ms | iqr_ms | ratio_to_current | class |"
    )
    print("|---:|---:|---|---|---|---|---|---:|---:|---:|---:|---|")
    for row in sorted(rows, key=lambda item: (item.effective_n, item.family, item.variant, item.max_threads)):
        status, ratio = classifications[row]
        print(
            f"| {row.effective_n} | {row.cells} | {row.family} | `{row.variant}` | "
            f"{row.requested_simd_level} | {row.selected_simd_level} | {row.simd_fallback} | {row.max_threads} | "
            f"{row.median * 1000.0:.3f} | {row.iqr * 1000.0:.3f} | {ratio:.3f} | {status} |"
        )

    print("\n| regime | variant | threads | class | rows | mean_ratio_to_current |")
    print("|---|---|---:|---|---:|---:|")
    for regime_name, variant, threads, label, count, mean_ratio in summarize_regimes(rows, classifications):
        print(f"| {regime_name} | `{variant}` | {threads} | {label} | {count} | {mean_ratio:.3f} |")

    candidates = dispatch_threshold_candidates(rows, classifications)
    print("\n| dispatch_effective_n | cells | variant | threads | covered_rows | mean_ratio_to_current |")
    print("|---:|---:|---|---:|---:|---:|")
    if not candidates:
        print("| - | - | no stable blocked-wavefront threshold in this data | - | - | - |")
    else:
        for threshold, threshold_cells, variant, threads, count, mean_ratio in candidates:
            print(f"| {threshold} | {threshold_cells} | `{variant}` | {threads} | {count} | {mean_ratio:.3f} |")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
