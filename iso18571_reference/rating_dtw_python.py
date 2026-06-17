from __future__ import annotations

import dtw
import numpy as np

from iso18571_reference._common import (
    BaseISO18571,
    FloatArray,
    dtw_window_radius,
    iso_backtrack,
)


class ISO18571(BaseISO18571):
    @staticmethod
    def _compute_magnitude(
        x: FloatArray, y: FloatArray, window_size: float
    ) -> tuple[FloatArray, FloatArray]:
        radius = dtw_window_radius(x.shape[0], window_size)
        cost = np.square(x[:, np.newaxis] - y[np.newaxis, :])
        result = dtw.dtw(
            cost,
            step_pattern=dtw.symmetric1,
            window_type="sakoechiba",
            window_args={"window_size": radius - 1},
            keep_internals=True,
        )
        path = iso_backtrack(result.costMatrix)
        return (
            np.asarray(x[path[:, 0]], dtype=np.float64),
            np.asarray(y[path[:, 1]], dtype=np.float64),
        )
