from __future__ import annotations

from numpy.typing import ArrayLike

from . import ScoreComponents

def backend_info() -> dict[str, str]: ...
def _score_components(
    reference_curve: ArrayLike,
    comparison_curve: ArrayLike,
    params: dict[str, float | int],
    store_validation: bool,
) -> ScoreComponents: ...
