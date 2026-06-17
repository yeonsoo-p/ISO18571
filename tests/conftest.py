from __future__ import annotations

import pytest

from tests.iso18571_annex import DEFAULT_ANNEX_DIR, load_downloaded_annex_cases, load_generated_annex_cases


def pytest_addoption(parser: pytest.Parser) -> None:
    parser.addoption(
        "--iso18571-annex-dir",
        default=str(DEFAULT_ANNEX_DIR),
        help="Directory containing ISO/TS 18571 ed.2 Annex CSV files.",
    )


@pytest.fixture(scope="session")
def downloaded_annex_cases(pytestconfig: pytest.Config):
    return load_downloaded_annex_cases(DEFAULT_ANNEX_DIR.parent / pytestconfig.getoption("--iso18571-annex-dir"))


@pytest.fixture(scope="session")
def generated_annex_cases():
    return load_generated_annex_cases()
