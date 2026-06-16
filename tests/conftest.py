from __future__ import annotations

import pytest

from rating import DTW_BACKENDS, _normalise_dtw_backend
from tests.iso18571_annex import (
    DEFAULT_ANNEX_DIR,
    FIXED_SIGNAL_STRESS_LENGTHS,
    load_annex_cases,
    load_fixed_signal_annex_cases,
    load_fixed_signal_benchmark_annex_cases,
)


def pytest_addoption(parser: pytest.Parser) -> None:
    parser.addoption(
        "--iso18571-backends",
        default="local_iso_numpy",
        help="Comma-separated DTW backends or 'all'. Run pytest once per backend for isolated prep benchmarks.",
    )
    parser.addoption(
        "--iso18571-annex-dir",
        default=str(DEFAULT_ANNEX_DIR),
        help="Directory containing ISO/TS 18571 ed.2 Annex CSV files.",
    )


def _selected_backends(config: pytest.Config) -> list[str]:
    raw = config.getoption("--iso18571-backends")
    if raw == "all":
        return list(DTW_BACKENDS)
    return [_normalise_dtw_backend(item.strip()) for item in raw.split(",") if item.strip()]


def pytest_generate_tests(metafunc: pytest.Metafunc) -> None:
    if "dtw_backend" in metafunc.fixturenames:
        backends = _selected_backends(metafunc.config)
        metafunc.parametrize("dtw_backend", backends, ids=backends)


@pytest.fixture(scope="session")
def annex_cases(pytestconfig: pytest.Config):
    return load_annex_cases(DEFAULT_ANNEX_DIR.parent / pytestconfig.getoption("--iso18571-annex-dir"))


@pytest.fixture(scope="session")
def fixed_signal_annex_cases():
    return load_fixed_signal_annex_cases()


@pytest.fixture(scope="session")
def fixed_signal_stress_annex_cases():
    return load_fixed_signal_annex_cases(lengths=FIXED_SIGNAL_STRESS_LENGTHS)


@pytest.fixture(scope="session")
def fixed_signal_benchmark_annex_cases():
    return load_fixed_signal_benchmark_annex_cases()
