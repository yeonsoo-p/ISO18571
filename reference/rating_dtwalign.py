from __future__ import annotations

import numpy as np
from dtwalign import dtw_low
from dtwalign.step_pattern import UserStepPattern
from dtwalign.window import SakoechibaWindow
from scipy.spatial.distance import cdist

from reference._common import BaseISO18571, FloatArray, dtw_window_radius


class ISO18571(BaseISO18571):
    @staticmethod
    def _compute_magnitude(
        x: FloatArray, y: FloatArray, window_size: float
    ) -> tuple[FloatArray, FloatArray]:
        radius = dtw_window_radius(x.shape[0], window_size)
        pattern_info = [
            dict(indices=[(-1, 0), (0, 0)], weights=[1]),
            dict(indices=[(0, -1), (0, 0)], weights=[1]),
            dict(indices=[(-1, -1), (0, 0)], weights=[1]),
        ]
        user_step_pattern = UserStepPattern(pattern_info, normalize_guide="none")
        cost = cdist(x[:, np.newaxis], y[:, np.newaxis], metric="sqeuclidean")
        window = SakoechibaWindow(cost.shape[0], cost.shape[1], size=radius - 1)
        result = dtw_low(cost, window=window, pattern=user_step_pattern)
        return (
            np.asarray(x[result.path[:, 0]], dtype=np.float64),
            np.asarray(y[result.path[:, 1]], dtype=np.float64),
        )
