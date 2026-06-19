from __future__ import annotations

import numpy as np
from numpy.typing import NDArray

from . import ScoreComponents

def backend_info() -> dict[str, str]: ...
def _score_components(
    reference_curve: NDArray[np.float32 | np.float64],
    comparison_curve: NDArray[np.float32 | np.float64],
    params: dict[str, float | int],
) -> tuple[
    ScoreComponents,
    NDArray[np.float32 | np.float64],
    NDArray[np.float32 | np.float64],
]: ...
