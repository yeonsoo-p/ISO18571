from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

import numpy as np
import numpy.typing as npt

from tools import example_data

SCORE_NAMES = ("R", "Z", "EP", "EM", "ES")

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


@dataclass(frozen=True)
class AnnexCase:
    name: str
    reference_curve: npt.NDArray[np.float64]
    comparison_curve: npt.NDArray[np.float64]
    dt: float
    expected: dict[str, float] | None
    expected_shifted_reference_values: npt.NDArray[np.float64] | None = None
    expected_shifted_comparison_values: npt.NDArray[np.float64] | None = None


def official_annex_dir(cache_dir: Path) -> Path:
    return example_data.ensure_official_annex_csvs(
        cache_dir / example_data.OFFICIAL_CACHE_VERSION,
        zip_path=cache_dir / example_data.ANNEX_ZIP_NAME,
    )


def generated_annex_dir(cache_dir: Path) -> Path:
    return example_data.write_generated_annex_csvs(
        cache_dir / example_data.GENERATED_CACHE_VERSION
    )[0]


def load_downloaded_annex_cases(annex_dir: Path) -> list[AnnexCase]:
    cases: list[AnnexCase] = []
    for path in sorted(annex_dir.glob("*.csv")):
        match = example_data.ANNEX_FILE_RE.match(path.name)
        if match is None:
            continue
        table_key = match[1]
        cae_no = int(match.group(2))
        array, shifted_array = example_data.load_official_annex_arrays(path)
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
                expected_shifted_reference_values=(
                    shifted_array[:, 0] if shifted_array is not None else None
                ),
                expected_shifted_comparison_values=(
                    shifted_array[:, 1] if shifted_array is not None else None
                ),
            )
        )
    if len(cases) != 42:
        raise ValueError(f"Expected 42 Annex cases, found {len(cases)} in {annex_dir}")
    return cases


def load_generated_annex_cases(annex_dir: Path) -> list[AnnexCase]:
    cases: list[AnnexCase] = []
    for path in sorted(annex_dir.glob("generated__*.csv")):
        match = example_data.GENERATED_FILE_RE.match(path.name)
        if match is None:
            continue
        array = example_data.load_generated_annex_array(path)
        cases.append(
            AnnexCase(
                name=path.stem,
                reference_curve=np.column_stack((array[:, 0], array[:, 1])),
                comparison_curve=np.column_stack((array[:, 0], array[:, 2])),
                dt=float(np.median(np.diff(array[:, 0]))),
                expected=None,
            )
        )

    expected_count = example_data.generated_annex_case_count()
    if len(cases) != expected_count:
        raise ValueError(
            f"Expected {expected_count} generated Annex cases, found {len(cases)} in {annex_dir}"
        )
    return cases
