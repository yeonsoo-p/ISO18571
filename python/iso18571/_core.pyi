from __future__ import annotations

from .rating import NumericArray, ScoreComponents, ScoreTimings

def backend_info() -> dict[str, str]: ...
def _score_components(
    reference_curve: NumericArray,
    comparison_curve: NumericArray,
    params: dict[str, float | int],
) -> tuple[ScoreComponents, ScoreTimings]: ...
