from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SRC_DIR = ROOT / "src" / "iso18571_native"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Emit ISO18571 native SIMD assembly artifacts.")
    parser.add_argument("--output-dir", default=".benchmarks/iso18571-asm", help="Directory for generated .s/.asm files.")
    return parser.parse_args()


def compiler() -> str:
    if os.environ.get("CXX"):
        return os.environ["CXX"]
    return shutil.which("c++") or shutil.which("g++") or shutil.which("clang++") or "c++"


def unix_commands(out_dir: Path) -> list[list[str]]:
    cxx = compiler()
    common = [
        cxx,
        "-std=c++17",
        "-O3",
        "-I",
        str(SRC_DIR),
        "-DISO18571_COMPILED_SCALAR=1",
        "-DISO18571_COMPILED_SSE2=1",
        "-DISO18571_COMPILED_AVX2=1",
        "-DISO18571_COMPILED_AVX2_FMA=1",
        "-S",
    ]
    sources = (
        ("simd_scalar.cpp", ()),
        ("simd_sse2.cpp", ("-msse2",)),
        ("simd_avx2.cpp", ("-mavx2",)),
        ("simd_avx2_fma.cpp", ("-mavx2", "-mfma")),
    )
    commands = []
    for source, flags in sources:
        output = out_dir / f"{Path(source).stem}.s"
        commands.append([*common, *flags, str(SRC_DIR / source), "-o", str(output)])
    return commands


def msvc_commands(out_dir: Path) -> list[list[str]]:
    sources = (
        ("simd_scalar.cpp", ()),
        ("simd_sse2.cpp", ()),
        ("simd_avx2.cpp", ("/arch:AVX2",)),
        ("simd_avx2_fma.cpp", ("/arch:AVX2",)),
    )
    commands = []
    for source, flags in sources:
        output = out_dir / f"{Path(source).stem}.asm"
        commands.append(
            [
                "cl",
                "/nologo",
                "/std:c++17",
                "/O2",
                "/fp:precise",
                "/EHsc",
                f"/I{SRC_DIR}",
                "/DISO18571_COMPILED_SCALAR=1",
                "/DISO18571_COMPILED_SSE2=1",
                "/DISO18571_COMPILED_AVX2=1",
                "/DISO18571_COMPILED_AVX2_FMA=1",
                "/c",
                "/FA",
                f"/Fa{output}",
                *flags,
                str(SRC_DIR / source),
            ]
        )
    return commands


def main() -> int:
    args = parse_args()
    out_dir = Path(args.output_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    commands = msvc_commands(out_dir) if sys.platform == "win32" else unix_commands(out_dir)
    for command in commands:
        subprocess.run(command, cwd=ROOT, check=True)
    print(f"Wrote assembly artifacts to {out_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
