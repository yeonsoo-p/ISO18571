from __future__ import annotations

from pathlib import Path

import pytest

from tools.annex import AnnexDataset


@pytest.fixture(scope="session")
def annex_dataset(pytestconfig: pytest.Config) -> AnnexDataset:
    local_root = Path(".cache") / "official-v2"
    if local_root.exists():
        return AnnexDataset.ensure(local_root)
    cache_root = Path(str(pytestconfig.cache.mkdir("official-annex-v2")))
    return AnnexDataset.ensure(cache_root)
