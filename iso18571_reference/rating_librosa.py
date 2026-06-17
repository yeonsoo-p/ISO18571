from __future__ import annotations

import librosa
import numpy as np

from iso18571_reference._common import BaseISO18571, local_cost_matrix


class ISO18571(BaseISO18571):
    @staticmethod
    def _compute_magnitude(x: np.ndarray, y: np.ndarray, window_size: float) -> tuple[np.ndarray, np.ndarray]:
        cost = local_cost_matrix(x, y, window_size)
        step_sizes = np.asarray([[1, 0], [0, 1], [1, 1]], dtype=np.uint32)
        weights = np.zeros(3, dtype=np.float64)
        _, warping_path = librosa.sequence.dtw(
            C=cost,
            step_sizes_sigma=step_sizes,
            weights_add=weights,
            weights_mul=np.ones(3, dtype=np.float64),
            global_constraints=False,
            backtrack=True,
        )
        path = np.asarray(warping_path[::-1], dtype=np.int64)
        return x[path[:, 0]], y[path[:, 1]]
