from __future__ import annotations

import math
from typing import cast

import numpy as np
from numpy.typing import NDArray


class BaseISO18571:
    def __init__(
        self,
        reference_curve: NDArray[np.float64],
        comparison_curve: NDArray[np.float64],
        k_z: float = 2.0,
        k_p: int = 1,
        k_m: int = 1,
        eps_m: float = 0.50,
        e_s: float = 2.0,
        init_min: float = 0.8,
        a_0: float = 0.05,
        b_0: float = 0.5,
        w_z: float = 0.4,
        w_p: float = 0.2,
        w_m: float = 0.2,
        w_s: float = 0.2,
        dt: float = 0.0001,
    ) -> None:
        self.reference_curve = np.asarray(reference_curve, dtype=np.float64)
        self.comparison_curve = np.asarray(comparison_curve, dtype=np.float64)

        if self.reference_curve.shape != self.comparison_curve.shape:
            raise ValueError(
                "Curves are not equal in size/dimension.\nInterpolation not implemented. "
            )

        k_z_float = float(k_z)
        if (
            not math.isfinite(k_z_float)
            or k_z_float < 1.0
            or math.floor(k_z_float) != k_z_float
        ):
            raise ValueError("k_z must be a positive integer")
        self._k_z = int(k_z_float)

        self._k_p = k_p
        if self._k_p not in [1, 2, 3]:
            raise ValueError("k_p has to be 1, 2, or 3")

        self._k_m = k_m
        if self._k_m not in [1, 2, 3]:
            raise ValueError("k_m has to be 1, 2, or 3")

        self._eps_m = eps_m
        self._e_s = e_s
        self._init_min = init_min
        self._a_0 = a_0
        self._b_0 = b_0
        self._w_z = w_z
        self._w_p = w_p
        self._w_m = w_m
        self._w_s = w_s

        weights_sum = self._w_z + self._w_m + self._w_p + self._w_s
        if weights_sum != 1:
            raise ValueError(
                f"Sum of weighting factors (w_z, w_m, w_p, w_s) is {weights_sum}, but must be 1"
            )

        self.dt = dt
        self._max_shift = round(1.0 - self._init_min, 2)
        (
            self._cae_ts,
            self._t_ts,
            self._n_eps,
            self._rho_e,
            self._reference_start,
            self._comparison_start,
            self._shift_length,
        ) = self._get_shifted_curve_and_pr()

    @property
    def n_eps(self) -> int:
        return int(self._n_eps)

    @property
    def rho_e(self) -> float:
        return float(self._rho_e)

    @property
    def shifted_reference_curve(self) -> NDArray[np.float64]:
        return self._t_ts.copy()

    @property
    def shifted_comparison_curve(self) -> NDArray[np.float64]:
        return self._cae_ts.copy()

    @property
    def reference_start(self) -> int:
        return int(self._reference_start)

    @property
    def comparison_start(self) -> int:
        return int(self._comparison_start)

    @property
    def shift_length(self) -> int:
        return int(self._shift_length)

    def _get_shifted_curve_and_pr(
        self,
    ) -> tuple[
        NDArray[np.float64],
        NDArray[np.float64],
        int,
        float,
        int,
        int,
        int,
    ]:
        window_size = int(
            np.floor(len(self.comparison_curve[:, 1]) * self._max_shift) + 1
        )
        ccr_max = self._phase_correlation(
            self.reference_curve[:, 1], self.comparison_curve[:, 1]
        )
        idx_ccr_max = 0
        t_ts = self.reference_curve
        cae_ts = self.comparison_curve
        reference_start = 0
        comparison_start = 0
        shift_length = self.reference_curve.shape[0]
        for idx in range(1, window_size):
            ccr_left = self._phase_correlation(
                self.reference_curve[:-idx, 1], self.comparison_curve[idx:, 1]
            )
            if ccr_left > ccr_max:
                ccr_max = ccr_left
                idx_ccr_max = idx
                t_ts = self.reference_curve[:-idx, :]
                cae_ts = self.comparison_curve[idx:, :]
                reference_start = 0
                comparison_start = idx
                shift_length = self.reference_curve.shape[0] - idx

            ccr_right = self._phase_correlation(
                self.reference_curve[idx:, 1], self.comparison_curve[:-idx, 1]
            )
            if ccr_right > ccr_max:
                ccr_max = ccr_right
                idx_ccr_max = idx
                t_ts = self.reference_curve[idx:, :]
                cae_ts = self.comparison_curve[:-idx, :]
                reference_start = idx
                comparison_start = 0
                shift_length = self.reference_curve.shape[0] - idx

        return (
            cae_ts.copy(),
            t_ts.copy(),
            idx_ccr_max,
            ccr_max,
            reference_start,
            comparison_start,
            shift_length,
        )

    @staticmethod
    def _compute_magnitude(
        x: NDArray[np.float64], y: NDArray[np.float64], window_size: float
    ) -> tuple[NDArray[np.float64], NDArray[np.float64]]:
        raise NotImplementedError

    @staticmethod
    def _phase_correlation(
        reference_values: NDArray[np.float64],
        comparison_values: NDArray[np.float64],
    ) -> float:
        if reference_values.shape[0] < 2:
            return 1.0 if np.array_equal(reference_values, comparison_values) else 0.0

        with np.errstate(invalid="ignore", divide="ignore"):
            correlation = float(np.corrcoef(reference_values, comparison_values)[0][-1])
        if math.isnan(correlation):
            return 1.0 if np.array_equal(reference_values, comparison_values) else 0.0
        return correlation

    @staticmethod
    def _rating_value(value: float | int, ndigits: int) -> float:
        value_float = float(value)
        if ndigits < 0:
            return value_float
        return round(value_float, ndigits=ndigits)

    def corridor_rating(self, ndigits: int = 3) -> float:
        t_norm = max(np.abs(self.reference_curve[:, 1]))
        inner_corridor = self._a_0 * t_norm
        outer_corridor = self._b_0 * t_norm
        abs_diff = np.absolute(np.subtract(self.reference_curve, self.comparison_curve))
        if t_norm == 0:
            z = float(np.sum(abs_diff[:, 1] == 0.0) / len(abs_diff))
            return self._rating_value(z, ndigits)
        c_i = np.array(
            pow(
                ((outer_corridor - abs_diff[:, 1]) / (outer_corridor - inner_corridor)),
                self._k_z,
            )
        )
        c_i[abs_diff[:, 1] < inner_corridor] = 1
        c_i[abs_diff[:, 1] > outer_corridor] = 0
        z = float(np.sum(c_i) / len(abs_diff))
        return self._rating_value(z, ndigits)

    def phase_rating(self, ndigits: int = 3) -> float:
        curve_size = self.reference_curve.shape[0]
        max_allowable_time_shift_threshold = curve_size * self._max_shift

        if self._n_eps == 0:
            e_p = 1.0
        elif abs(self._n_eps) >= max_allowable_time_shift_threshold:
            e_p = 0.0
        else:
            e_p = (
                float(
                    (max_allowable_time_shift_threshold - abs(self._n_eps))
                    / max_allowable_time_shift_threshold
                )
                ** self._k_p
            )
        return self._rating_value(e_p, ndigits)

    def magnitude_rating(self, ndigits: int = 3) -> float:
        cae_ts_w, t_ts_w = self._compute_magnitude(
            self._cae_ts[:, 1], self._t_ts[:, 1], window_size=0.1
        )
        numerator = float(np.linalg.norm(cae_ts_w - t_ts_w, ord=1))
        denominator = float(np.linalg.norm(t_ts_w, ord=1))
        if denominator == 0.0:
            score_mag = 1.0 if numerator == 0.0 else 0.0
            return self._rating_value(score_mag, ndigits)

        e_mag = numerator / denominator

        if e_mag == 0:
            score_mag = 1.0
        elif e_mag > self._eps_m:
            score_mag = 0.0
        else:
            score_mag = float(((self._eps_m - e_mag) / self._eps_m) ** self._k_m)
        return self._rating_value(score_mag, ndigits)

    def slope_rating(self, ndigits: int = 3) -> float:
        cae_ts_0_d = np.gradient(self._cae_ts[:, 1], self.dt)
        t_ts_0_d = np.gradient(self._t_ts[:, 1], self.dt)

        cae_ts_d = np.zeros(len(cae_ts_0_d))
        t_ts_d = np.zeros(len(t_ts_0_d))

        for idx, nr in enumerate([1, 3, 5, 7]):
            idx_begin = idx
            idx_end = (-1) * idx - 1
            cae_ts_d[idx_begin] = 1 / nr * np.sum(cae_ts_0_d[0:nr])
            cae_ts_d[idx_end] = 1 / nr * np.sum(cae_ts_0_d[-nr:])
            t_ts_d[idx_begin] = 1 / nr * np.sum(t_ts_0_d[0:nr])
            t_ts_d[idx_end] = 1 / nr * np.sum(t_ts_0_d[-nr:])

        nr = 9
        cae_ts_d[4:-4] = np.convolve(cae_ts_0_d, np.ones(nr) / nr, mode="valid")
        t_ts_d[4:-4] = np.convolve(t_ts_0_d, np.ones(nr) / nr, mode="valid")

        numerator = float(np.linalg.norm(cae_ts_d - t_ts_d, ord=1))
        denominator = float(np.linalg.norm(t_ts_d, ord=1))
        if denominator == 0.0:
            slope_score = 1.0 if numerator == 0.0 else 0.0
            return self._rating_value(slope_score, ndigits)

        e_slope = numerator / denominator

        if e_slope <= 0:
            slope_score = 1.0
        elif e_slope >= self._e_s:
            slope_score = 0.0
        else:
            slope_score = float((self._e_s - e_slope) / self._e_s)
        return self._rating_value(slope_score, ndigits)

    def overall_rating(self, ndigits: int = 3) -> float:
        z = self.corridor_rating(ndigits=-1)
        e_p = self.phase_rating(ndigits=-1)
        e_m = self.magnitude_rating(ndigits=-1)
        e_s = self.slope_rating(ndigits=-1)
        overall_rating = float(
            self._w_z * z + self._w_p * e_p + self._w_m * e_m + self._w_s * e_s
        )
        return self._rating_value(overall_rating, ndigits)


def dtw_window_radius(n: int, window_size: float) -> int:
    if n <= 0:
        raise ValueError("DTW input arrays must not be empty")
    return min(n, max(1, int(np.ceil(window_size * n))))


def iso_backtrack(accumulated: NDArray[np.float64]) -> NDArray[np.int64]:
    accumulated = np.where(np.isnan(accumulated), np.inf, accumulated)
    i = accumulated.shape[0] - 1
    j = accumulated.shape[1] - 1
    path = [(i, j)]
    while i > 0 or j > 0:
        candidates = []
        if i > 0:
            candidates.append((accumulated[i - 1, j], i - 1, j))
        if j > 0:
            candidates.append((accumulated[i, j - 1], i, j - 1))
        if i > 0 and j > 0:
            candidates.append((accumulated[i - 1, j - 1], i - 1, j - 1))
        costs = [candidate[0] for candidate in candidates]
        _, i, j = candidates[int(np.argmin(costs))]
        path.append((i, j))
    path.reverse()
    return cast(NDArray[np.int64], np.asarray(path, dtype=np.int64))


def local_cost_matrix(
    x: NDArray[np.float64], y: NDArray[np.float64], window_size: float
) -> NDArray[np.float64]:
    n = x.shape[0]
    radius = dtw_window_radius(n, window_size)
    cost = np.square(x[:, np.newaxis] - y[np.newaxis, :])
    idx = np.arange(n)
    cost[np.abs(idx[:, np.newaxis] - idx[np.newaxis, :]) >= radius] = np.inf
    return cast(NDArray[np.float64], cost)
