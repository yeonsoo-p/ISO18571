from __future__ import annotations

import csv
import re
from dataclasses import dataclass
from pathlib import Path

import numpy as np

from tests.iso18571_signals import SIGNAL_FAMILIES, signal_case


SCORE_NAMES = ("R", "Z", "EP", "EM", "ES")
DEFAULT_ANNEX_DIR = Path("ISO_TS 18571 ed.2 - Annex_data_csv_files")
ANNEX_FILE_RE = re.compile(r"annex_c_(\d+_\d+)__.*__cae(\d+)\.csv")

EXPECTED_SCORES: dict[str, dict[int, tuple[float, float, float, float, float]]] = {
    "1_1": {1: (0.9169, 0.9560, 0.9518, 0.9524, 0.7683), 2: (0.8848, 0.8977, 0.9196, 0.9640, 0.7449), 3: (0.9122, 0.9225, 0.9839, 0.9716, 0.7606)},
    "1_2": {1: (0.4379, 0.4059, 0.0065, 0.6471, 0.7241), 2: (0.6993, 0.6423, 0.6254, 0.8219, 0.7648), 3: (0.8542, 0.8452, 0.7720, 0.9659, 0.8427)},
    "1_3": {1: (0.8045, 0.7263, 0.9401, 0.8977, 0.7321), 2: (0.8701, 0.8164, 0.9401, 0.9310, 0.8467), 3: (0.8314, 0.7499, 0.9401, 0.9088, 0.8084)},
    "1_4": {1: (0.7877, 0.7927, 0.9771, 0.8709, 0.5052), 2: (0.6540, 0.6476, 0.9200, 0.7909, 0.2641), 3: (0.7910, 0.7837, 0.9943, 0.8486, 0.5446)},
    "2_1": {1: (0.6994, 0.5297, 0.7175, 0.8924, 0.8276), 2: (0.6242, 0.5406, 0.9177, 0.4633, 0.6588), 3: (0.7060, 0.5379, 0.7496, 0.8476, 0.8568)},
    "2_2": {1: (0.5912, 0.4033, 0.7203, 0.7599, 0.6694), 2: (0.3643, 0.2868, 0.9056, 0.0980, 0.2441), 3: (0.5835, 0.3740, 0.7448, 0.6426, 0.7819)},
    "3_1": {1: (0.9214, 0.8878, 0.9369, 0.9765, 0.9182), 2: (0.9825, 1.0000, 0.9747, 0.9814, 0.9563), 3: (0.9834, 0.9994, 0.9495, 0.9948, 0.9741)},
    "3_2": {1: (0.6518, 0.4702, 0.8085, 0.6796, 0.8303), 2: (0.8632, 0.8069, 0.9362, 0.9422, 0.8236), 3: (0.8368, 0.7916, 0.8582, 0.9553, 0.7875)},
    "4_1": {1: (0.5364, 0.4501, 0.6899, 0.6177, 0.4739), 2: (0.6556, 0.5267, 0.5930, 0.9111, 0.7206), 3: (0.9297, 0.9112, 0.9225, 0.9572, 0.9465)},
    "4_2": {1: (0.2761, 0.3753, 0.5238, 0.0000, 0.1060), 2: (0.5766, 0.4277, 0.3810, 0.8390, 0.8079), 3: (0.6463, 0.5214, 0.8810, 0.6582, 0.6498)},
    "4_3": {1: (0.7133, 0.6548, 0.9769, 0.7384, 0.5414), 2: (0.4658, 0.4910, 0.8731, 0.3628, 0.1109), 3: (0.8642, 0.9032, 0.9596, 0.9282, 0.6269)},
    "5_1": {1: (0.8203, 0.7402, 0.9784, 0.9099, 0.7329), 2: (0.7549, 0.7233, 0.5905, 0.9668, 0.7703), 3: (0.8084, 0.7901, 0.8060, 0.9618, 0.6939)},
    "5_2": {1: (0.2946, 0.2049, 0.4979, 0.0000, 0.5652), 2: (0.3770, 0.2797, 0.5711, 0.1995, 0.5551), 3: (0.5592, 0.4281, 0.9268, 0.4683, 0.5450)},
    "5_3": {1: (0.6607, 0.5390, 0.6958, 0.8390, 0.6907), 2: (0.6637, 0.5380, 0.7338, 0.7983, 0.7102), 3: (0.6713, 0.5563, 1.0000, 0.7304, 0.5132)},
}

THEORETICAL_INTEGRITY: dict[str, tuple[bool, str]] = {
    "local_iso_numpy": (True, "Local implementation uses squared local cost, ISO 10% band, and explicit ISO predecessor tie order."),
    "local_iso_native": (True, "Clean-room native implementation is validated against local_iso_numpy path and Annex scores."),
    "dtwalign": (True, "Pattern order is configured as ISO predecessor order; dtwalign backtracking uses first minimum."),
    "dtaidistance": (True, "Uses dtaidistance accumulated costs and the shared ISO backtracker because native path tie order differs."),
    "dtw_python": (True, "Uses dtw-python accumulated costs and the shared ISO backtracker because native path tie order differs."),
    "tslearn": (False, "Uses native tslearn path generation; tie-breaking is not proven to follow ISO predecessor order."),
    "librosa": (True, "Uses precomputed ISO local costs and step order configured to match ISO predecessor order."),
}


@dataclass(frozen=True)
class AnnexCase:
    name: str
    reference_curve: np.ndarray
    comparison_curve: np.ndarray
    dt: float
    expected: dict[str, float] | None


FIXED_SIGNAL_NORMAL_LENGTHS = (9, 10, 17, 64, 129, 512, 1430)
FIXED_SIGNAL_STRESS_LENGTHS = (4096, 8192, 16384, 32768)
FIXED_SIGNAL_BENCHMARK_SPECS = (
    ("short_sine_noise", "sine_noise", 129),
    ("annex_like_sine_amp_offset", "sine_amp_offset", 1430),
    ("long_smooth_chirp", "chirp", 8192),
    ("long_noisy_gaussian", "gaussian_noise", 8192),
    ("long_sparse_spikes", "sparse_spikes", 8192),
)


def load_annex_cases(annex_dir: Path = DEFAULT_ANNEX_DIR) -> list[AnnexCase]:
    cases = []
    for path in sorted(annex_dir.glob("*.csv")):
        match = ANNEX_FILE_RE.match(path.name)
        if match is None:
            continue
        table_key = match.group(1)
        cae_no = int(match.group(2))
        rows = []
        with path.open(newline="") as csv_file:
            for row in list(csv.DictReader(csv_file))[1:]:
                try:
                    rows.append((float(row["Time"]), float(row["Test"]), float(row["CAE"])))
                except (KeyError, TypeError, ValueError):
                    continue
        if not rows:
            raise ValueError(f"No signal data found in {path}")
        array = np.asarray(rows, dtype=np.float64)
        expected = dict(zip(SCORE_NAMES, EXPECTED_SCORES[table_key][cae_no], strict=True))
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


def fixed_signal_annex_case(family: str, n: int, *, label: str | None = None) -> AnnexCase:
    case = signal_case(family, n)
    name = label or f"fixed_signal__{family}__n{n}"
    return AnnexCase(
        name=name,
        reference_curve=case.reference,
        comparison_curve=case.comparison,
        dt=float(np.median(np.diff(case.reference[:, 0]))),
        expected=None,
    )


def load_fixed_signal_annex_cases(
    lengths: tuple[int, ...] = FIXED_SIGNAL_NORMAL_LENGTHS,
    families: tuple[str, ...] = SIGNAL_FAMILIES,
) -> list[AnnexCase]:
    return [fixed_signal_annex_case(family, n) for n in lengths for family in families]


def load_fixed_signal_benchmark_annex_cases() -> list[AnnexCase]:
    return [
        fixed_signal_annex_case(family, n, label=f"fixed_signal_benchmark__{label}__n{n}")
        for label, family, n in FIXED_SIGNAL_BENCHMARK_SPECS
    ]
