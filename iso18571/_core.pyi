from __future__ import annotations

from collections.abc import Mapping
from typing import Any

import numpy.typing as npt

ScoreComponents = dict[str, float | int]

def _score_components(
    reference_curve: npt.ArrayLike,
    comparison_curve: npt.ArrayLike,
    params: Mapping[str, float | int] | None = ...,
) -> ScoreComponents: ...
def backend_info() -> dict[str, Any]: ...
