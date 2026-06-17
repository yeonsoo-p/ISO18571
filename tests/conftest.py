from __future__ import annotations

from collections.abc import Sequence

import pytest

from tests.iso18571_annex import (
    AnnexCase,
    generated_annex_dir,
    load_downloaded_annex_cases,
    load_generated_annex_cases,
    official_annex_dir,
)


@pytest.fixture(scope="session")
def downloaded_annex_cases(pytestconfig: pytest.Config) -> Sequence[AnnexCase]:
    cache_dir = pytestconfig.cache.mkdir("iso18571_annex")
    return load_downloaded_annex_cases(official_annex_dir(cache_dir))


@pytest.fixture(scope="session")
def generated_annex_cases(pytestconfig: pytest.Config) -> Sequence[AnnexCase]:
    cache_dir = pytestconfig.cache.mkdir("iso18571_annex")
    return load_generated_annex_cases(generated_annex_dir(cache_dir))
