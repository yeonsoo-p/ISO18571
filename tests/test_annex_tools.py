from __future__ import annotations

import csv
import hashlib
import zipfile
from pathlib import Path

import numpy as np
import pytest

from tools.annex import (
    OFFICIAL_ANNEX_CASE_COUNT,
    OFFICIAL_COLUMNS,
    OFFICIAL_UNITS,
    AnnexCase,
    AnnexDataset,
)


def test_annex_case_imports_and_exports_canonical_csv(
    annex_dataset: AnnexDataset,
    tmp_path: Path,
) -> None:
    source = annex_dataset.paths[0]
    case = AnnexCase.from_csv(source)

    assert tuple(case.columns) == OFFICIAL_COLUMNS
    assert tuple(column.unit for column in case.columns.values()) == OFFICIAL_UNITS
    input_rows = (
        np.isfinite(case.values("Time"))
        & np.isfinite(case.values("Test"))
        & np.isfinite(case.values("CAE"))
    )
    assert case.reference_curve().shape == (int(np.sum(input_rows)), 2)
    assert case.comparison_curve().shape == (int(np.sum(input_rows)), 2)

    exported = tmp_path / "exported.csv"
    case.to_csv(exported)
    reloaded = AnnexCase.from_csv(exported)

    for name in OFFICIAL_COLUMNS:
        np.testing.assert_array_equal(
            reloaded.columns[name].values, case.columns[name].values
        )
        assert reloaded.columns[name].unit == case.columns[name].unit


def test_annex_case_rejects_missing_required_columns(tmp_path: Path) -> None:
    path = tmp_path / "bad.csv"
    with path.open("w", newline="") as csv_file:
        writer = csv.writer(csv_file)
        writer.writerow(OFFICIAL_COLUMNS[:-1])
        writer.writerow(OFFICIAL_UNITS[:-1])
        writer.writerow(["0"] * (len(OFFICIAL_COLUMNS) - 1))

    with pytest.raises(ValueError, match="missing required Annex columns"):
        AnnexCase.from_csv(path)


def test_annex_dataset_has_expected_official_case_count(
    annex_dataset: AnnexDataset,
) -> None:
    assert len(annex_dataset.paths) == OFFICIAL_ANNEX_CASE_COUNT
    assert sum(1 for _ in annex_dataset.cases()) == OFFICIAL_ANNEX_CASE_COUNT


def test_annex_dataset_downloads_verifies_and_extracts_zip(tmp_path: Path) -> None:
    source_zip = tmp_path / "source.zip"
    _write_minimal_annex_zip(source_zip)
    expected_hash = hashlib.sha256(source_zip.read_bytes()).hexdigest()

    dataset = AnnexDataset.ensure(
        tmp_path / "official", url=source_zip.as_uri(), sha256=expected_hash
    )

    assert len(dataset.paths) == OFFICIAL_ANNEX_CASE_COUNT


def test_annex_dataset_rejects_hash_mismatch(tmp_path: Path) -> None:
    source_zip = tmp_path / "source.zip"
    _write_minimal_annex_zip(source_zip)

    with pytest.raises(ValueError, match="SHA-256 mismatch"):
        AnnexDataset.ensure(
            tmp_path / "official", url=source_zip.as_uri(), sha256="0" * 64
        )


def _write_minimal_annex_zip(path: Path) -> None:
    with zipfile.ZipFile(path, "w") as zip_file:
        for index in range(OFFICIAL_ANNEX_CASE_COUNT):
            rows = [
                ",".join(OFFICIAL_COLUMNS),
                ",".join(OFFICIAL_UNITS),
                ",".join(["0"] * len(OFFICIAL_COLUMNS)),
            ]
            zip_file.writestr(f"case_{index:02d}.csv", "\n".join(rows) + "\n")
