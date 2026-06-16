from __future__ import annotations

import argparse
import re
from dataclasses import dataclass
from pathlib import Path


VECTOR_PATTERN = re.compile(r"\b[vyx]?mm\d+\b|\b[xyz]mmword\b", re.IGNORECASE)
YMM_PATTERN = re.compile(r"\bymm\d+\b|\bymmword\b", re.IGNORECASE)
FMA_PATTERN = re.compile(r"\bvfmadd|\bvfnmadd|\bvfm?sub", re.IGNORECASE)
CALL_PATTERN = re.compile(r"^\s*call", re.IGNORECASE)
STACK_PATTERN = re.compile(r"(%rsp|%rbp|\brsp\b|\brbp\b)", re.IGNORECASE)


@dataclass(frozen=True)
class AssemblySummary:
    path: Path
    lines: int
    vector_lines: int
    ymm_lines: int
    fma_lines: int
    call_lines: int
    stack_lines: int
    observations: tuple[str, ...]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Report ISO18571 native assembly observations without fixing them.")
    parser.add_argument("assembly_dir", nargs="?", default=".benchmarks/iso18571-asm")
    return parser.parse_args()


def summarize(path: Path) -> AssemblySummary:
    text = path.read_text(errors="replace")
    lines = text.splitlines()
    vector_lines = sum(1 for line in lines if VECTOR_PATTERN.search(line))
    ymm_lines = sum(1 for line in lines if YMM_PATTERN.search(line))
    fma_lines = sum(1 for line in lines if FMA_PATTERN.search(line))
    call_lines = sum(1 for line in lines if CALL_PATTERN.search(line))
    stack_lines = sum(1 for line in lines if STACK_PATTERN.search(line))

    observations = []
    name = path.name.lower()
    if "sse2" in name and vector_lines == 0:
        observations.append("no visible SSE/XMM vector instructions")
    if "avx2" in name and ymm_lines == 0:
        observations.append("no visible AVX/YMM vector instructions")
    if "fma" in name and fma_lines == 0:
        observations.append("AVX2+FMA unit does not emit FMA instructions")
    if call_lines > 0:
        observations.append("contains calls; inspect whether any are in hot loops")
    if stack_lines > max(20, len(lines) // 10):
        observations.append("notable stack-frame traffic")
    if not observations:
        observations.append("no obvious wrinkle from simple scan")

    return AssemblySummary(
        path=path,
        lines=len(lines),
        vector_lines=vector_lines,
        ymm_lines=ymm_lines,
        fma_lines=fma_lines,
        call_lines=call_lines,
        stack_lines=stack_lines,
        observations=tuple(observations),
    )


def main() -> int:
    args = parse_args()
    assembly_dir = Path(args.assembly_dir)
    paths = sorted([*assembly_dir.glob("*.s"), *assembly_dir.glob("*.asm")])
    if not paths:
        raise SystemExit(f"No assembly files found in {assembly_dir}")

    print("| file | lines | vector_lines | ymm_lines | fma_lines | call_lines | stack_lines | observations |")
    print("|---|---:|---:|---:|---:|---:|---:|---|")
    for summary in (summarize(path) for path in paths):
        observations = "; ".join(summary.observations)
        print(
            f"| `{summary.path.name}` | {summary.lines} | {summary.vector_lines} | {summary.ymm_lines} | "
            f"{summary.fma_lines} | {summary.call_lines} | {summary.stack_lines} | {observations} |"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
