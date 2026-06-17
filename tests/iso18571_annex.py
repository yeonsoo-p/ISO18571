from __future__ import annotations

import csv
import re
import urllib.error
import urllib.request
import zipfile
from dataclasses import dataclass
from pathlib import Path

import numpy as np
import numpy.typing as npt

from tests.iso18571_signals import (
    PHASE_SHIFT_SIGNAL_FAMILIES,
    SIGNAL_FAMILIES,
    analytic_phase_signal_case,
    signal_case,
)

SCORE_NAMES = ("R", "Z", "EP", "EM", "ES")
ANNEX_ZIP_URL = (
    "https://standards.iso.org/iso/ts/18571/ed-2/en/"
    "ISO_TS%2018571%20ed.2%20-%20Annex_data_csv_files.zip"
)
ANNEX_ZIP_NAME = "ISO_TS_18571_ed2_Annex_data_csv_files.zip"
OFFICIAL_CACHE_VERSION = "official-v1"
GENERATED_CACHE_VERSION = "generated-v1"
ANNEX_FILE_RE = re.compile(r"annex_c_(\d+_\d+)__.*__cae(\d+)\.csv")
GENERATED_FILE_RE = re.compile(r"generated__(.+)__n(\d+)\.csv")

EXPECTED_SCORES: dict[str, dict[int, tuple[float, float, float, float, float]]] = {
    "1_1": {
        1: (0.9169, 0.9560, 0.9518, 0.9524, 0.7683),
        2: (0.8848, 0.8977, 0.9196, 0.9640, 0.7449),
        3: (0.9122, 0.9225, 0.9839, 0.9716, 0.7606),
    },
    "1_2": {
        1: (0.4379, 0.4059, 0.0065, 0.6471, 0.7241),
        2: (0.6993, 0.6423, 0.6254, 0.8219, 0.7648),
        3: (0.8542, 0.8452, 0.7720, 0.9659, 0.8427),
    },
    "1_3": {
        1: (0.8045, 0.7263, 0.9401, 0.8977, 0.7321),
        2: (0.8701, 0.8164, 0.9401, 0.9310, 0.8467),
        3: (0.8314, 0.7499, 0.9401, 0.9088, 0.8084),
    },
    "1_4": {
        1: (0.7877, 0.7927, 0.9771, 0.8709, 0.5052),
        2: (0.6540, 0.6476, 0.9200, 0.7909, 0.2641),
        3: (0.7910, 0.7837, 0.9943, 0.8486, 0.5446),
    },
    "2_1": {
        1: (0.6994, 0.5297, 0.7175, 0.8924, 0.8276),
        2: (0.6242, 0.5406, 0.9177, 0.4633, 0.6588),
        3: (0.7060, 0.5379, 0.7496, 0.8476, 0.8568),
    },
    "2_2": {
        1: (0.5912, 0.4033, 0.7203, 0.7599, 0.6694),
        2: (0.3643, 0.2868, 0.9056, 0.0980, 0.2441),
        3: (0.5835, 0.3740, 0.7448, 0.6426, 0.7819),
    },
    "3_1": {
        1: (0.9214, 0.8878, 0.9369, 0.9765, 0.9182),
        2: (0.9825, 1.0000, 0.9747, 0.9814, 0.9563),
        3: (0.9834, 0.9994, 0.9495, 0.9948, 0.9741),
    },
    "3_2": {
        1: (0.6518, 0.4702, 0.8085, 0.6796, 0.8303),
        2: (0.8632, 0.8069, 0.9362, 0.9422, 0.8236),
        3: (0.8368, 0.7916, 0.8582, 0.9553, 0.7875),
    },
    "4_1": {
        1: (0.5364, 0.4501, 0.6899, 0.6177, 0.4739),
        2: (0.6556, 0.5267, 0.5930, 0.9111, 0.7206),
        3: (0.9297, 0.9112, 0.9225, 0.9572, 0.9465),
    },
    "4_2": {
        1: (0.2761, 0.3753, 0.5238, 0.0000, 0.1060),
        2: (0.5766, 0.4277, 0.3810, 0.8390, 0.8079),
        3: (0.6463, 0.5214, 0.8810, 0.6582, 0.6498),
    },
    "4_3": {
        1: (0.7133, 0.6548, 0.9769, 0.7384, 0.5414),
        2: (0.4658, 0.4910, 0.8731, 0.3628, 0.1109),
        3: (0.8642, 0.9032, 0.9596, 0.9282, 0.6269),
    },
    "5_1": {
        1: (0.8203, 0.7402, 0.9784, 0.9099, 0.7329),
        2: (0.7549, 0.7233, 0.5905, 0.9668, 0.7703),
        3: (0.8084, 0.7901, 0.8060, 0.9618, 0.6939),
    },
    "5_2": {
        1: (0.2946, 0.2049, 0.4979, 0.0000, 0.5652),
        2: (0.3770, 0.2797, 0.5711, 0.1995, 0.5551),
        3: (0.5592, 0.4281, 0.9268, 0.4683, 0.5450),
    },
    "5_3": {
        1: (0.6607, 0.5390, 0.6958, 0.8390, 0.6907),
        2: (0.6637, 0.5380, 0.7338, 0.7983, 0.7102),
        3: (0.6713, 0.5563, 1.0000, 0.7304, 0.5132),
    },
}

GENERATED_LENGTHS = (9, 10, 17, 64, 129, 512, 1430)
GENERATED_PHASE_LENGTHS = (64, 129, 512, 1430)


@dataclass(frozen=True)
class AnnexCase:
    name: str
    reference_curve: npt.NDArray[np.float64]
    comparison_curve: npt.NDArray[np.float64]
    dt: float
    expected: dict[str, float] | None


def official_annex_dir(cache_dir: Path) -> Path:
    target_dir = cache_dir / OFFICIAL_CACHE_VERSION
    if _official_annex_is_ready(target_dir):
        return target_dir

    target_dir.mkdir(parents=True, exist_ok=True)
    zip_path = cache_dir / ANNEX_ZIP_NAME
    if not zip_path.exists():
        _download_official_annex(zip_path)
    _extract_official_annex(zip_path, target_dir)
    if not _official_annex_is_ready(target_dir):
        raise RuntimeError(f"Downloaded Annex cache is incomplete in {target_dir}")
    return target_dir


def generated_annex_dir(cache_dir: Path) -> Path:
    target_dir = cache_dir / GENERATED_CACHE_VERSION
    manifest_path = target_dir / "manifest.txt"
    expected_manifest = _generated_manifest()
    if (
        manifest_path.exists()
        and manifest_path.read_text(encoding="utf-8") == expected_manifest
    ):
        return target_dir

    target_dir.mkdir(parents=True, exist_ok=True)
    for path in target_dir.glob("generated__*.csv"):
        path.unlink()
    for case in _generated_cases_in_memory():
        _write_case_csv(target_dir / f"{case.name}.csv", case)
    manifest_path.write_text(expected_manifest, encoding="utf-8")
    return target_dir


def load_downloaded_annex_cases(annex_dir: Path) -> list[AnnexCase]:
    cases: list[AnnexCase] = []
    for path in sorted(annex_dir.glob("*.csv")):
        match = ANNEX_FILE_RE.match(path.name)
        if match is None:
            continue
        table_key = match[1]
        cae_no = int(match.group(2))
        rows: list[tuple[float, float, float]] = []
        with path.open(newline="") as csv_file:
            for row in list(csv.DictReader(csv_file))[1:]:
                try:
                    rows.append(
                        (float(row["Time"]), float(row["Test"]), float(row["CAE"]))
                    )
                except (KeyError, TypeError, ValueError):
                    continue
        if not rows:
            raise ValueError(f"No signal data found in {path}")
        array = np.asarray(rows, dtype=np.float64)
        expected = dict(
            zip(SCORE_NAMES, EXPECTED_SCORES[table_key][cae_no], strict=True)
        )
        cases.append(
            AnnexCase(
                name=path.name,
                reference_curve=np.column_stack((array[:, 0], array[:, 1])),
                comparison_curve=np.column_stack((array[:, 0], array[:, 2])),
                dt=float(np.median(np.diff(array[:, 0]))),
                expected=expected,
            )
        )
    if len(cases) != 42:
        raise ValueError(f"Expected 42 Annex cases, found {len(cases)} in {annex_dir}")
    return cases


def load_generated_annex_cases(annex_dir: Path) -> list[AnnexCase]:
    cases: list[AnnexCase] = []
    for path in sorted(annex_dir.glob("generated__*.csv")):
        match = GENERATED_FILE_RE.match(path.name)
        if match is None:
            continue
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
        cases.append(
            AnnexCase(
                name=path.stem,
                reference_curve=np.column_stack((array[:, 0], array[:, 1])),
                comparison_curve=np.column_stack((array[:, 0], array[:, 2])),
                dt=float(np.median(np.diff(array[:, 0]))),
                expected=None,
            )
        )

    expected_count = len(GENERATED_LENGTHS) * len(SIGNAL_FAMILIES) + len(
        GENERATED_PHASE_LENGTHS
    ) * len(PHASE_SHIFT_SIGNAL_FAMILIES)
    if len(cases) != expected_count:
        raise ValueError(
            f"Expected {expected_count} generated Annex cases, found {len(cases)} in {annex_dir}"
        )
    return cases


def _official_annex_is_ready(annex_dir: Path) -> bool:
    return (
        annex_dir.is_dir()
        and len(
            [path for path in annex_dir.glob("*.csv") if ANNEX_FILE_RE.match(path.name)]
        )
        == 42
    )


def _download_official_annex(zip_path: Path) -> None:
    zip_path.parent.mkdir(parents=True, exist_ok=True)
    tmp_path = zip_path.with_suffix(".tmp")
    try:
        with urllib.request.urlopen(ANNEX_ZIP_URL, timeout=60.0) as response:
            tmp_path.write_bytes(response.read())
    except (OSError, urllib.error.URLError) as exc:
        raise RuntimeError(
            f"Could not download ISO/TS 18571 Annex data from {ANNEX_ZIP_URL}: {exc}"
        ) from exc
    tmp_path.replace(zip_path)


def _extract_official_annex(zip_path: Path, target_dir: Path) -> None:
    for path in target_dir.glob("*.csv"):
        path.unlink()
    try:
        with zipfile.ZipFile(zip_path) as archive:
            for member in archive.infolist():
                member_name = Path(member.filename).name
                if ANNEX_FILE_RE.match(member_name):
                    (target_dir / member_name).write_bytes(archive.read(member))
    except (OSError, zipfile.BadZipFile) as exc:
        raise RuntimeError(
            f"Could not extract ISO/TS 18571 Annex cache {zip_path}: {exc}"
        ) from exc


def _generated_cases_in_memory() -> list[AnnexCase]:
    cases: list[AnnexCase] = []
    for n in GENERATED_LENGTHS:
        for family in SIGNAL_FAMILIES:
            signal = signal_case(family, n)
            cases.append(
                AnnexCase(
                    name=f"generated__{family}__n{n}",
                    reference_curve=signal.reference,
                    comparison_curve=signal.comparison,
                    dt=float(np.median(np.diff(signal.reference[:, 0]))),
                    expected=None,
                )
            )
    for n in GENERATED_PHASE_LENGTHS:
        for family in PHASE_SHIFT_SIGNAL_FAMILIES:
            signal = analytic_phase_signal_case(family, n)
            cases.append(
                AnnexCase(
                    name=f"generated__{family}__n{n}",
                    reference_curve=signal.reference,
                    comparison_curve=signal.comparison,
                    dt=float(np.median(np.diff(signal.reference[:, 0]))),
                    expected=None,
                )
            )
    return cases


def _write_case_csv(path: Path, case: AnnexCase) -> None:
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


def _generated_manifest() -> str:
    return "\n".join(
        (
            GENERATED_CACHE_VERSION,
            ",".join(str(length) for length in GENERATED_LENGTHS),
            ",".join(str(length) for length in GENERATED_PHASE_LENGTHS),
            ",".join(SIGNAL_FAMILIES),
            ",".join(PHASE_SHIFT_SIGNAL_FAMILIES),
            "",
        )
    )
