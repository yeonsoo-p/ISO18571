"""Helpers for ISO/TS 18571 Annex CSV data used by tests and tools."""

from __future__ import annotations

import csv
import hashlib
import math
import shutil
import tempfile
import urllib.request
import zipfile
from collections.abc import Iterator, Mapping
from dataclasses import dataclass
from pathlib import Path
from typing import Final, TypedDict

import numpy as np
from numpy.typing import NDArray


OFFICIAL_ANNEX_URL: Final = (
    "https://standards.iso.org/iso/ts/18571/ed-2/en/"
    "ISO_TS%2018571%20ed.2%20-%20Annex_data_csv_files.zip"
)
OFFICIAL_ANNEX_ARCHIVE: Final = "ISO_TS_18571_ed2_Annex_data_csv_files.zip"
OFFICIAL_ANNEX_SHA256: Final = (
    "cbc8c5a1ea5677ece8aa097387f9d9d2e6fe7a2a5bb2ce5d17ecf84fe52271d7"
)
OFFICIAL_ANNEX_CASE_COUNT: Final = 42

OFFICIAL_COLUMNS: Final = (
    "Generic_Time",
    "Time",
    "Test",
    "CAE",
    "Outer_Corridor_Upper",
    "Inner_Corridor_Upper",
    "Inner_Corridor_Lower",
    "Outer_Corridor_Lower",
    "Test_Phase_Shifted",
    "CAE_Phase_Shifted",
    "Test_Slope",
    "CAE_Slope",
    "Test_Warped",
    "CAE_Warped",
)
OFFICIAL_UNITS: Final = (
    "[-]",
    "[s]",
    "[m/s2]",
    "[m/s2]",
    "[m/s2]",
    "[m/s2]",
    "[m/s2]",
    "[m/s2]",
    "[m/s2]",
    "[m/s2]",
    "[m/s2]",
    "[m/s2]",
    "[m/s2]",
    "[m/s2]",
)


class MagnitudeFields(TypedDict):
    magnitude_numerator: float
    magnitude_denominator: float
    magnitude_error: float


@dataclass(frozen=True)
class AnnexColumn:
    name: str
    unit: str
    values: NDArray[np.float64]


@dataclass(frozen=True)
class AnnexCase:
    name: str
    columns: Mapping[str, AnnexColumn]
    source_path: Path | None = None

    @classmethod
    def from_csv(cls, path: str | Path) -> AnnexCase:
        csv_path = Path(path)
        rows = _read_csv_rows(csv_path)
        if len(rows) < 2:
            raise ValueError(f"{csv_path} must contain a header and unit row")

        header = tuple(rows[0])
        units = tuple(rows[1])
        _require_official_header(csv_path, header)
        _require_official_units(csv_path, units)

        data_rows = rows[2:]
        parsed: dict[str, list[float]] = {name: [] for name in OFFICIAL_COLUMNS}
        for line_number, row in enumerate(data_rows, start=3):
            if len(row) != len(OFFICIAL_COLUMNS):
                raise ValueError(
                    f"{csv_path}:{line_number}: expected {len(OFFICIAL_COLUMNS)} "
                    f"columns, got {len(row)}"
                )
            for name, raw_value in zip(OFFICIAL_COLUMNS, row, strict=True):
                parsed[name].append(
                    _parse_float_or_nan(csv_path, line_number, name, raw_value)
                )

        columns = {
            name: AnnexColumn(
                name=name,
                unit=units[index],
                values=np.asarray(parsed[name], dtype=np.float64),
            )
            for index, name in enumerate(OFFICIAL_COLUMNS)
        }
        return cls(name=csv_path.stem, columns=columns, source_path=csv_path)

    def to_csv(self, path: str | Path) -> None:
        output_path = Path(path)
        output_path.parent.mkdir(parents=True, exist_ok=True)
        row_count = self.row_count

        with output_path.open("w", newline="") as csv_file:
            writer = csv.writer(csv_file, lineterminator="\n")
            writer.writerow(OFFICIAL_COLUMNS)
            writer.writerow([self.columns[name].unit for name in OFFICIAL_COLUMNS])
            for row_index in range(row_count):
                writer.writerow(
                    [
                        _format_float(self.columns[name].values[row_index])
                        for name in OFFICIAL_COLUMNS
                    ]
                )

    @property
    def row_count(self) -> int:
        counts = {self.columns[name].values.shape[0] for name in OFFICIAL_COLUMNS}
        if len(counts) != 1:
            raise ValueError(f"{self.name} has mismatched column lengths")
        return int(counts.pop())

    def values(self, name: str) -> NDArray[np.float64]:
        return self.columns[name].values.copy()

    def finite_values(self, name: str) -> NDArray[np.float64]:
        values = self.columns[name].values
        return values[np.isfinite(values)].copy()

    def input_values(self, name: str) -> NDArray[np.float64]:
        return self.columns[name].values[self._input_mask()].copy()

    def reference_curve(self) -> NDArray[np.float64]:
        mask = self._input_mask()
        return np.column_stack(
            (self.columns["Time"].values[mask], self.columns["Test"].values[mask])
        ).astype(
            np.float64,
            copy=False,
        )

    def comparison_curve(self) -> NDArray[np.float64]:
        mask = self._input_mask()
        return np.column_stack(
            (self.columns["Time"].values[mask], self.columns["CAE"].values[mask])
        ).astype(
            np.float64,
            copy=False,
        )

    def finite_pair(
        self, left: str, right: str
    ) -> tuple[NDArray[np.float64], NDArray[np.float64]]:
        left_values = self.columns[left].values
        right_values = self.columns[right].values
        mask = np.isfinite(left_values) & np.isfinite(right_values)
        return left_values[mask].copy(), right_values[mask].copy()

    def magnitude_fields_from_warped(self) -> MagnitudeFields:
        test_warped, cae_warped = self.finite_pair("Test_Warped", "CAE_Warped")
        numerator = float(np.sum(np.abs(cae_warped - test_warped)))
        denominator = float(np.sum(np.abs(test_warped)))
        error = math.nan if denominator == 0.0 else numerator / denominator
        return {
            "magnitude_numerator": numerator,
            "magnitude_denominator": denominator,
            "magnitude_error": error,
        }

    def _input_mask(self) -> NDArray[np.bool_]:
        return (
            np.isfinite(self.columns["Time"].values)
            & np.isfinite(self.columns["Test"].values)
            & np.isfinite(self.columns["CAE"].values)
        )


@dataclass(frozen=True)
class AnnexDataset:
    root: Path

    @classmethod
    def ensure(
        cls,
        root: str | Path = Path(".cache") / "official-v2",
        *,
        archive_path: str | Path | None = None,
        url: str = OFFICIAL_ANNEX_URL,
        sha256: str = OFFICIAL_ANNEX_SHA256,
    ) -> AnnexDataset:
        dataset_root = Path(root)
        if _has_official_cases(dataset_root):
            return cls(dataset_root)

        archive = (
            Path(archive_path)
            if archive_path is not None
            else dataset_root.parent / OFFICIAL_ANNEX_ARCHIVE
        )
        archive.parent.mkdir(parents=True, exist_ok=True)
        if not archive.exists() or _sha256(archive) != sha256:
            _download(url, archive)

        actual_sha256 = _sha256(archive)
        if actual_sha256 != sha256:
            raise ValueError(
                f"{archive} SHA-256 mismatch: expected {sha256}, got {actual_sha256}"
            )

        _extract_official_zip(archive, dataset_root)
        if not _has_official_cases(dataset_root):
            raise ValueError(
                f"{archive} did not provide {OFFICIAL_ANNEX_CASE_COUNT} official Annex CSV files"
            )
        return cls(dataset_root)

    @property
    def paths(self) -> tuple[Path, ...]:
        return tuple(sorted(self.root.glob("*.csv")))

    def cases(self) -> Iterator[AnnexCase]:
        for path in self.paths:
            yield AnnexCase.from_csv(path)


def _read_csv_rows(path: Path) -> list[list[str]]:
    with path.open(newline="") as csv_file:
        return list(csv.reader(csv_file))


def _require_official_header(path: Path, header: tuple[str, ...]) -> None:
    if header != OFFICIAL_COLUMNS:
        missing = [name for name in OFFICIAL_COLUMNS if name not in header]
        if missing:
            raise ValueError(
                f"{path} is missing required Annex columns: {', '.join(missing)}"
            )
        raise ValueError(f"{path} uses unsupported Annex column order")


def _require_official_units(path: Path, units: tuple[str, ...]) -> None:
    if units != OFFICIAL_UNITS:
        raise ValueError(f"{path} uses unsupported Annex unit row")


def _parse_float_or_nan(
    path: Path, line_number: int, column: str, raw_value: str
) -> float:
    value = raw_value.strip()
    if value == "":
        return math.nan
    try:
        return float(value)
    except ValueError as exc:
        raise ValueError(
            f"{path}:{line_number}: {column} has non-numeric value {raw_value!r}"
        ) from exc


def _format_float(value: float | np.float64) -> str:
    value_float = float(value)
    if math.isnan(value_float):
        return ""
    return format(value_float, ".17g")


def _has_official_cases(root: Path) -> bool:
    paths = sorted(root.glob("*.csv"))
    return len(paths) == OFFICIAL_ANNEX_CASE_COUNT


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as binary_file:
        for chunk in iter(lambda: binary_file.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _download(url: str, destination: Path) -> None:
    with urllib.request.urlopen(url, timeout=60.0) as response:
        with destination.open("wb") as output:
            shutil.copyfileobj(response, output)


def _extract_official_zip(archive: Path, destination: Path) -> None:
    with tempfile.TemporaryDirectory(prefix="iso18571-annex-") as temporary_directory:
        staging = Path(temporary_directory)
        with zipfile.ZipFile(archive) as zip_file:
            for member in zip_file.infolist():
                member_path = Path(member.filename)
                if (
                    member_path.name != member.filename
                    or member_path.suffix.lower() != ".csv"
                ):
                    raise ValueError(
                        f"{archive} contains unsupported member {member.filename!r}"
                    )
                zip_file.extract(member, staging)

        destination.mkdir(parents=True, exist_ok=True)
        for path in staging.glob("*.csv"):
            shutil.copy2(path, destination / path.name)
