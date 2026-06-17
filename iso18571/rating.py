from __future__ import annotations

from typing import Any

import numpy as np

from iso18571_native import score_components


class CurveLengthError(ValueError):
    pass


class ISO18571:
    def __init__(
        self,
        reference_curve: np.ndarray,
        comparison_curve: np.ndarray,
        k_z: int = 2,
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
            raise CurveLengthError("Curves are not equal in size/dimension.\nInterpolation not implemented. ")

        self._scores = score_components(
            self.reference_curve,
            self.comparison_curve,
            {
                "k_z": k_z,
                "k_p": k_p,
                "k_m": k_m,
                "eps_m": eps_m,
                "e_s": e_s,
                "init_min": init_min,
                "a_0": a_0,
                "b_0": b_0,
                "w_z": w_z,
                "w_p": w_p,
                "w_m": w_m,
                "w_s": w_s,
                "dt": dt,
            },
        )

        reference_start = int(self._scores["reference_start"])
        comparison_start = int(self._scores["comparison_start"])
        shift_length = int(self._scores["shift_length"])
        self._t_ts = self.reference_curve[reference_start : reference_start + shift_length, :].copy()
        self._cae_ts = self.comparison_curve[comparison_start : comparison_start + shift_length, :].copy()
        self._n_eps = int(self._scores["n_eps"])
        self._rho_e = float(self._scores["rho_e"])

    @staticmethod
    def _rating_value(value: Any, ndigits: int) -> Any:
        if ndigits < 0:
            return value
        return round(value, ndigits=ndigits)

    def corridor_rating(self, ndigits: int = 3) -> float:
        return ISO18571._rating_value(self._scores["Z"], ndigits)

    def phase_rating(self, ndigits: int = 3) -> float:
        return ISO18571._rating_value(self._scores["EP"], ndigits)

    def magnitude_rating(self, ndigits: int = 3) -> float:
        return ISO18571._rating_value(self._scores["EM"], ndigits)

    def slope_rating(self, ndigits: int = 3) -> float:
        return ISO18571._rating_value(self._scores["ES"], ndigits)

    def overall_rating(self, ndigits: int = 3) -> float:
        return ISO18571._rating_value(self._scores["R"], ndigits)
