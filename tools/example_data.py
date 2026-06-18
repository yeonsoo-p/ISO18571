"""Prepare ISO18571 example, official Annex, and generated Annex CSV data."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import re
import urllib.error
import urllib.request
import zipfile
from dataclasses import dataclass
from pathlib import Path
from typing import cast

import numpy as np
import numpy.typing as npt

FloatArray = npt.NDArray[np.float64]

ANNEX_ZIP_URL = (
    "https://standards.iso.org/iso/ts/18571/ed-2/en/"
    "ISO_TS%2018571%20ed.2%20-%20Annex_data_csv_files.zip"
)
ANNEX_ZIP_NAME = "ISO_TS_18571_ed2_Annex_data_csv_files.zip"
ANNEX_ZIP_SHA256 = "cbc8c5a1ea5677ece8aa097387f9d9d2e6fe7a2a5bb2ce5d17ecf84fe52271d7"
OFFICIAL_CACHE_VERSION = "official-v2"
GENERATED_CACHE_VERSION = "generated-v2"
ANNEX_FILE_RE = re.compile(r"annex_c_(\d+_\d+)__.*__cae(\d+)\.csv")
GENERATED_FILE_RE = re.compile(r"generated__(.+)__n(\d+)\.csv")

DEFAULT_EXAMPLE_DIR = Path("examples")
DEFAULT_ANNEX_ROOT = DEFAULT_EXAMPLE_DIR / "data" / "annex"
DEFAULT_OFFICIAL_ANNEX_DIR = DEFAULT_ANNEX_ROOT / "official"
DEFAULT_GENERATED_ANNEX_DIR = DEFAULT_ANNEX_ROOT / "generated"

SIGNAL_FAMILIES = (
    "ramp",
    "piecewise_ramp",
    "impulse",
    "sparse_spikes",
    "sine_phase",
    "sine_amp_offset",
    "chirp",
    "square_step",
    "gaussian_noise",
    "sine_noise",
    "ramp_impulses",
    "piecewise_discontinuous",
)
GENERATED_SIGNAL_EXCLUDED_CASES = frozenset(
    {
        ("impulse", 9),
        ("sparse_spikes", 9),
    }
)
PHASE_SHIFT_SIGNAL_FAMILIES = (
    "phase_multitone_shift_005",
    "phase_multitone_shift_020",
    "phase_chirp_shift_050",
    "phase_pulses_shift_100",
    "phase_smooth_step_shift_180",
)
GENERATED_LENGTHS = (9, 10, 17, 64, 129, 512, 1430)
GENERATED_PHASE_LENGTHS = (64, 129, 512, 1430)


@dataclass(frozen=True)
class CurvePair:
    name: str
    reference_curve: FloatArray
    comparison_curve: FloatArray


def make_demo_curves(
    length: int = 600, dt: float = 0.0001
) -> tuple[FloatArray, FloatArray]:
    if length < 9:
        raise ValueError("demo curves must have at least 9 samples")
    time = np.arange(length, dtype=np.float64) * dt
    tau = np.linspace(0.0, 1.0, length, endpoint=False, dtype=np.float64)
    reference = (
        0.65 * np.sin(2.0 * np.pi * 3.0 * tau)
        + 0.20 * np.sin(2.0 * np.pi * 9.0 * tau + 0.25)
        + 0.10 * np.tanh((tau - 0.42) / 0.035)
    )
    shifted_tau = tau - 0.012
    comparison = (
        0.62 * np.sin(2.0 * np.pi * 3.0 * shifted_tau)
        + 0.18 * np.sin(2.0 * np.pi * 9.0 * shifted_tau + 0.25)
        + 0.09 * np.tanh((tau - 0.44) / 0.04)
        + 0.025
    )
    return (
        np.column_stack((time, reference)).astype(np.float64, copy=False),
        np.column_stack((time, comparison)).astype(np.float64, copy=False),
    )


def write_curve_csv(path: Path, curve: FloatArray) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    np.savetxt(path, curve, delimiter=",", fmt="%.17g")


def load_curve_csv(path: Path, delimiter: str = ",") -> FloatArray:
    curve = np.loadtxt(path, delimiter=delimiter)
    if curve.ndim != 2 or curve.shape[1] != 2:
        raise ValueError(f"{path} must contain exactly two columns: time,value")
    return curve.astype(np.float64, copy=False)


def write_demo_csvs(output_dir: Path = DEFAULT_EXAMPLE_DIR) -> tuple[Path, Path]:
    reference, comparison = make_demo_curves()
    reference_path = output_dir / "reference.csv"
    comparison_path = output_dir / "comparison.csv"
    write_curve_csv(reference_path, reference)
    write_curve_csv(comparison_path, comparison)
    return reference_path, comparison_path


def curve(values: FloatArray, dt: float = 0.0001) -> FloatArray:
    time = np.arange(values.shape[0], dtype=np.float64) * dt
    return cast(
        FloatArray, np.column_stack((time, values.astype(np.float64, copy=False)))
    )


def signal_case(family: str, n: int) -> CurvePair:
    rng = np.random.default_rng(18571 + n)
    t = np.linspace(0.0, 1.0, n, endpoint=False, dtype=np.float64)

    if family == "zero":
        reference = np.zeros(n, dtype=np.float64)
        comparison = np.zeros(n, dtype=np.float64)
    elif family == "constant":
        reference = np.full(n, 2.0, dtype=np.float64)
        comparison = np.full(n, 2.1, dtype=np.float64)
    elif family == "ramp":
        reference = np.linspace(-1.0, 1.0, n, dtype=np.float64)
        comparison = reference * 1.03 + 0.02 + 0.005 * np.sin(2.0 * np.pi * 3.0 * t)
    elif family == "piecewise_ramp":
        reference = np.where(t < 0.35, 2.0 * t, 0.7 - 1.5 * (t - 0.35))
        comparison = reference + np.where(t > 0.55, 0.08, -0.03)
    elif family == "impulse":
        reference = np.zeros(n, dtype=np.float64)
        comparison = np.zeros(n, dtype=np.float64)
        reference[n // 3] = 1.0
        comparison[min(n - 1, n // 3 + max(1, n // 80))] = 0.92
    elif family == "sparse_spikes":
        reference = np.zeros(n, dtype=np.float64)
        comparison = np.zeros(n, dtype=np.float64)
        idx = np.linspace(1, n - 2, min(9, max(2, n // 16)), dtype=np.int64)
        reference[idx] = np.linspace(0.4, 1.2, idx.shape[0])
        comparison[np.minimum(n - 1, idx + 1)] = reference[idx] * 0.95
    elif family == "sine_phase":
        reference = np.sin(2.0 * np.pi * 5.0 * t)
        comparison = np.sin(2.0 * np.pi * 5.0 * (t - 0.015))
    elif family == "sine_amp_offset":
        reference = np.sin(2.0 * np.pi * 3.0 * t)
        comparison = 1.12 * reference + 0.08
    elif family == "chirp":
        reference = np.sin(2.0 * np.pi * (2.0 * t + 9.0 * t * t))
        comparison = np.sin(2.0 * np.pi * (2.0 * (t - 0.006) + 9.0 * (t - 0.006) ** 2))
    elif family == "square_step":
        reference = np.where(np.sin(2.0 * np.pi * 4.0 * t) >= 0.0, 1.0, -1.0) + 1e-6 * t
        comparison = (
            np.where(np.sin(2.0 * np.pi * 4.0 * (t - 0.01)) >= 0.0, 0.9, -1.1)
            + 1e-6 * t
        )
    elif family == "gaussian_noise":
        reference = rng.normal(0.0, 1.0, n)
        comparison = 0.92 * reference + rng.normal(0.0, 0.15, n)
    elif family == "sine_noise":
        reference = np.sin(2.0 * np.pi * 6.0 * t)
        comparison = 0.98 * np.sin(2.0 * np.pi * 6.0 * (t - 0.008)) + rng.normal(
            0.0, 0.04, n
        )
    elif family == "ramp_impulses":
        reference = np.linspace(0.0, 1.0, n, dtype=np.float64) + 0.03 * (t - 0.5) ** 2
        comparison = reference.copy()
        idx = np.linspace(2, n - 3, min(7, max(2, n // 32)), dtype=np.int64)
        comparison[idx] += np.linspace(0.2, -0.2, idx.shape[0])
    elif family == "piecewise_discontinuous":
        reference = np.asarray(
            np.piecewise(
                t,
                [t < 0.25, (t >= 0.25) & (t < 0.65), t >= 0.65],
                [lambda x: 2.0 * x, 0.75, lambda x: 0.75 - x],
            ),
            dtype=np.float64,
        )
        comparison = reference + np.where(t > 0.5, -0.07, 0.04)
    else:
        raise ValueError(f"Unknown signal family {family}")

    return CurvePair(
        name=family,
        reference_curve=curve(reference),
        comparison_curve=curve(comparison),
    )


def _phase_multitone(t: FloatArray) -> FloatArray:
    return (
        0.65 * np.sin(2.0 * np.pi * 3.0 * t)
        + 0.25 * np.sin(2.0 * np.pi * 7.0 * t + 0.35)
        + 0.10 * np.cos(2.0 * np.pi * 11.0 * t - 0.2)
    )


def _phase_chirp(t: FloatArray) -> FloatArray:
    return np.sin(2.0 * np.pi * (1.5 * t + 10.0 * t * t))


def _phase_pulses(t: FloatArray) -> FloatArray:
    centers = np.asarray([0.18, 0.41, 0.73], dtype=np.float64)
    widths = np.asarray([0.012, 0.025, 0.018], dtype=np.float64)
    amplitudes = np.asarray([1.0, -0.75, 0.55], dtype=np.float64)
    values = np.zeros_like(t)
    for center, width, amplitude in zip(centers, widths, amplitudes, strict=True):
        values += amplitude * np.exp(-0.5 * ((t - center) / width) ** 2)
    return values


def _phase_smooth_step(t: FloatArray) -> FloatArray:
    return 0.75 * np.tanh((t - 0.38) / 0.025) - 0.45 * np.tanh((t - 0.68) / 0.035)


def analytic_phase_signal_case(family: str, n: int) -> CurvePair:
    t = np.linspace(0.0, 1.0, n, endpoint=False, dtype=np.float64)
    if family == "phase_multitone_shift_005":
        reference = _phase_multitone(t)
        comparison = _phase_multitone(t - 0.005)
    elif family == "phase_multitone_shift_020":
        reference = _phase_multitone(t)
        comparison = _phase_multitone(t - 0.020)
    elif family == "phase_chirp_shift_050":
        reference = _phase_chirp(t)
        comparison = _phase_chirp(t - 0.050)
    elif family == "phase_pulses_shift_100":
        reference = _phase_pulses(t)
        comparison = _phase_pulses(t - 0.100)
    elif family == "phase_smooth_step_shift_180":
        reference = _phase_smooth_step(t)
        comparison = _phase_smooth_step(t - 0.180)
    else:
        raise ValueError(f"Unknown analytic phase signal family {family}")

    return CurvePair(
        name=family,
        reference_curve=curve(reference),
        comparison_curve=curve(comparison),
    )


def generated_annex_case_count() -> int:
    signal_count = sum(
        (family, n) not in GENERATED_SIGNAL_EXCLUDED_CASES
        for n in GENERATED_LENGTHS
        for family in SIGNAL_FAMILIES
    )
    return signal_count + len(GENERATED_PHASE_LENGTHS) * len(
        PHASE_SHIFT_SIGNAL_FAMILIES
    )


def generated_annex_cases() -> list[CurvePair]:
    cases: list[CurvePair] = []
    for n in GENERATED_LENGTHS:
        for family in SIGNAL_FAMILIES:
            if (family, n) in GENERATED_SIGNAL_EXCLUDED_CASES:
                continue
            signal = signal_case(family, n)
            cases.append(
                CurvePair(
                    name=f"generated__{family}__n{n}",
                    reference_curve=signal.reference_curve,
                    comparison_curve=signal.comparison_curve,
                )
            )
    for n in GENERATED_PHASE_LENGTHS:
        for family in PHASE_SHIFT_SIGNAL_FAMILIES:
            signal = analytic_phase_signal_case(family, n)
            cases.append(
                CurvePair(
                    name=f"generated__{family}__n{n}",
                    reference_curve=signal.reference_curve,
                    comparison_curve=signal.comparison_curve,
                )
            )
    return cases


def generated_annex_manifest() -> str:
    return "\n".join(
        (
            GENERATED_CACHE_VERSION,
            ",".join(str(length) for length in GENERATED_LENGTHS),
            ",".join(str(length) for length in GENERATED_PHASE_LENGTHS),
            ",".join(SIGNAL_FAMILIES),
            ",".join(PHASE_SHIFT_SIGNAL_FAMILIES),
            ",".join(
                f"{family}:n{length}"
                for family, length in sorted(GENERATED_SIGNAL_EXCLUDED_CASES)
            ),
            "",
        )
    )


def generated_annex_is_ready(annex_dir: Path) -> bool:
    manifest_path = annex_dir / "manifest.txt"
    return (
        annex_dir.is_dir()
        and manifest_path.exists()
        and manifest_path.read_text(encoding="utf-8") == generated_annex_manifest()
        and len(
            [
                path
                for path in annex_dir.glob("generated__*.csv")
                if GENERATED_FILE_RE.match(path.name)
            ]
        )
        == generated_annex_case_count()
    )


def write_generated_annex_case_csv(path: Path, case: CurvePair) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="") as csv_file:
        writer = csv.writer(csv_file)
        writer.writerow(("Time", "Reference", "Comparison"))
        for row in np.column_stack(
            (
                case.reference_curve[:, 0],
                case.reference_curve[:, 1],
                case.comparison_curve[:, 1],
            )
        ):
            writer.writerow((f"{row[0]:.17g}", f"{row[1]:.17g}", f"{row[2]:.17g}"))


def write_generated_annex_csvs(
    output_dir: Path = DEFAULT_GENERATED_ANNEX_DIR,
) -> tuple[Path, int]:
    if generated_annex_is_ready(output_dir):
        return output_dir, generated_annex_case_count()

    output_dir.mkdir(parents=True, exist_ok=True)
    for path in output_dir.glob("generated__*.csv"):
        path.unlink()
    for case in generated_annex_cases():
        write_generated_annex_case_csv(output_dir / f"{case.name}.csv", case)
    (output_dir / "manifest.txt").write_text(
        generated_annex_manifest(), encoding="utf-8"
    )
    if not generated_annex_is_ready(output_dir):
        raise RuntimeError(f"Generated Annex cache is incomplete in {output_dir}")
    return output_dir, generated_annex_case_count()


def official_annex_manifest() -> str:
    return "\n".join(
        (
            OFFICIAL_CACHE_VERSION,
            ANNEX_ZIP_URL,
            ANNEX_ZIP_SHA256,
            "",
        )
    )


def official_annex_is_ready(annex_dir: Path) -> bool:
    manifest_path = annex_dir / "manifest.txt"
    return (
        annex_dir.is_dir()
        and manifest_path.exists()
        and manifest_path.read_text(encoding="utf-8") == official_annex_manifest()
        and len(
            [path for path in annex_dir.glob("*.csv") if ANNEX_FILE_RE.match(path.name)]
        )
        == 42
    )


def file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as file:
        for chunk in iter(lambda: file.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def require_annex_zip_hash(zip_path: Path) -> None:
    actual = file_sha256(zip_path)
    if actual != ANNEX_ZIP_SHA256:
        raise RuntimeError(
            f"ISO/TS 18571 Annex ZIP hash mismatch for {zip_path}: "
            f"expected {ANNEX_ZIP_SHA256}, got {actual}"
        )


def download_annex_zip(zip_path: Path) -> None:
    zip_path.parent.mkdir(parents=True, exist_ok=True)
    request = urllib.request.Request(
        ANNEX_ZIP_URL, headers={"User-Agent": "iso18571-example-data"}
    )
    tmp_path = zip_path.with_suffix(".tmp")
    try:
        with urllib.request.urlopen(request, timeout=60.0) as response:
            tmp_path.write_bytes(response.read())
    except (OSError, urllib.error.URLError) as exc:
        raise RuntimeError(
            f"Could not download ISO/TS 18571 Annex data from {ANNEX_ZIP_URL}: {exc}"
        ) from exc
    tmp_path.replace(zip_path)


def ensure_annex_zip(zip_path: Path) -> None:
    if zip_path.exists():
        try:
            require_annex_zip_hash(zip_path)
            return
        except RuntimeError:
            zip_path.unlink()

    download_annex_zip(zip_path)
    try:
        require_annex_zip_hash(zip_path)
    except RuntimeError:
        zip_path.unlink(missing_ok=True)
        raise


def extract_official_annex_csvs(zip_path: Path, output_dir: Path) -> int:
    require_annex_zip_hash(zip_path)
    output_dir.mkdir(parents=True, exist_ok=True)
    for path in output_dir.glob("*.csv"):
        path.unlink()
    (output_dir / "manifest.txt").unlink(missing_ok=True)

    count = 0
    try:
        with zipfile.ZipFile(zip_path) as archive:
            for member in archive.infolist():
                member_name = Path(member.filename).name
                if ANNEX_FILE_RE.match(member_name):
                    (output_dir / member_name).write_bytes(archive.read(member))
                    count += 1
    except (OSError, zipfile.BadZipFile) as exc:
        raise RuntimeError(
            f"Could not extract ISO/TS 18571 Annex cache {zip_path}: {exc}"
        ) from exc
    (output_dir / "manifest.txt").write_text(
        official_annex_manifest(), encoding="utf-8"
    )
    return count


def ensure_official_annex_csvs(
    output_dir: Path = DEFAULT_OFFICIAL_ANNEX_DIR, zip_path: Path | None = None
) -> Path:
    if official_annex_is_ready(output_dir):
        return output_dir

    actual_zip_path = zip_path if zip_path is not None else output_dir / ANNEX_ZIP_NAME
    ensure_annex_zip(actual_zip_path)
    extract_official_annex_csvs(actual_zip_path, output_dir)
    if not official_annex_is_ready(output_dir):
        raise RuntimeError(f"Downloaded Annex cache is incomplete in {output_dir}")
    return output_dir


def download_annex_csvs(
    output_dir: Path = DEFAULT_OFFICIAL_ANNEX_DIR, zip_path: Path | None = None
) -> tuple[Path, int]:
    annex_dir = ensure_official_annex_csvs(output_dir, zip_path)
    count = len(
        [path for path in annex_dir.glob("*.csv") if ANNEX_FILE_RE.match(path.name)]
    )
    return annex_dir, count


def load_official_annex_curve_pair(path: Path) -> CurvePair:
    rows: list[tuple[float, float, float]] = []
    with path.open(newline="") as csv_file:
        for row in list(csv.DictReader(csv_file))[1:]:
            try:
                rows.append((float(row["Time"]), float(row["Test"]), float(row["CAE"])))
            except (KeyError, TypeError, ValueError):
                continue
    if not rows:
        raise ValueError(f"No signal data found in {path}")
    array = np.asarray(rows, dtype=np.float64)
    return CurvePair(
        name=path.stem,
        reference_curve=np.column_stack((array[:, 0], array[:, 1])),
        comparison_curve=np.column_stack((array[:, 0], array[:, 2])),
    )


def load_generated_annex_curve_pair(path: Path) -> CurvePair:
    rows: list[tuple[float, float, float]] = []
    with path.open(newline="") as csv_file:
        for row in csv.DictReader(csv_file):
            try:
                rows.append(
                    (
                        float(row["Time"]),
                        float(row["Reference"]),
                        float(row["Comparison"]),
                    )
                )
            except (KeyError, TypeError, ValueError):
                continue
    if not rows:
        raise ValueError(f"No generated signal data found in {path}")
    array = np.asarray(rows, dtype=np.float64)
    return CurvePair(
        name=path.stem,
        reference_curve=np.column_stack((array[:, 0], array[:, 1])),
        comparison_curve=np.column_stack((array[:, 0], array[:, 2])),
    )


def first_official_annex_curve_pair(
    annex_dir: Path = DEFAULT_OFFICIAL_ANNEX_DIR,
) -> CurvePair:
    for path in sorted(annex_dir.glob("*.csv")):
        if ANNEX_FILE_RE.match(path.name):
            return load_official_annex_curve_pair(path)
    raise ValueError(f"No official Annex CSV files found in {annex_dir}")


def first_generated_annex_curve_pair(
    annex_dir: Path = DEFAULT_GENERATED_ANNEX_DIR,
) -> CurvePair:
    for path in sorted(annex_dir.glob("generated__*.csv")):
        if GENERATED_FILE_RE.match(path.name):
            return load_generated_annex_curve_pair(path)
    raise ValueError(f"No generated Annex CSV files found in {annex_dir}")


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Prepare ISO18571 example data files.")
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=DEFAULT_EXAMPLE_DIR,
        help="Directory for generated reference.csv and comparison.csv.",
    )
    parser.add_argument(
        "--annex-dir",
        type=Path,
        default=DEFAULT_ANNEX_ROOT,
        help="Directory for official/ and generated/ Annex CSV data.",
    )
    parser.add_argument(
        "--download-annex",
        action="store_true",
        help="Download and extract the official ISO/TS 18571 Annex CSV files.",
    )
    parser.add_argument(
        "--generate-annex",
        action="store_true",
        help="Generate the synthetic Annex parity CSV files.",
    )
    parser.add_argument(
        "--all",
        action="store_true",
        help="Prepare demo CSVs, official Annex CSVs, and generated Annex CSVs.",
    )
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    reference_path, comparison_path = write_demo_csvs(args.output_dir)
    result: dict[str, str | int] = {
        "reference_csv": str(reference_path),
        "comparison_csv": str(comparison_path),
    }

    if args.download_annex or args.all:
        annex_dir, annex_count = download_annex_csvs(args.annex_dir / "official")
        result["official_annex_dir"] = str(annex_dir)
        result["official_annex_csv_count"] = annex_count

    if args.generate_annex or args.all:
        annex_dir, annex_count = write_generated_annex_csvs(
            args.annex_dir / "generated"
        )
        result["generated_annex_dir"] = str(annex_dir)
        result["generated_annex_csv_count"] = annex_count

    print(json.dumps(result, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
