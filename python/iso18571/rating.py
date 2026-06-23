from __future__ import annotations

from typing import TypedDict
import numpy as np
from numpy.typing import ArrayLike

from ._core import _score_components


class ScoreComponents(TypedDict):
    Z: float
    EP: float
    EM: float
    ES: float
    R: float
    n_eps: int
    rho_e: float
    reference_start: int
    comparison_start: int
    shift_length: int


class ISO18571:
    def __init__(
        self,
        reference_curve: ArrayLike,
        comparison_curve: ArrayLike,
        k_z: int | float = 2,
        k_p: int | float = 1,
        k_m: int | float = 1,
        eps_m: float = 0.50,
        e_s: float = 2.0,
        init_min: float = 0.8,
        a_0: float = 0.05,
        b_0: float = 0.5,
        w_z: float = 0.4,
        w_p: float = 0.2,
        w_m: float = 0.2,
        w_s: float = 0.2,
    ) -> None:
        if np.ma.isMaskedArray(reference_curve):
            raise ValueError("reference_curve must not be a masked array")
        if np.ma.isMaskedArray(comparison_curve):
            raise ValueError("comparison_curve must not be a masked array")

        reference_array = np.asarray(reference_curve)
        comparison_array = np.asarray(comparison_curve)

        self._scores: ScoreComponents
        self._scores = _score_components(
            reference_array,
            comparison_array,
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
            },
        )

        self._n_eps = int(self._scores["n_eps"])
        self._rho_e = float(self._scores["rho_e"])

    @property
    def scores(self) -> ScoreComponents:
        return self._scores.copy()

    @property
    def n_eps(self) -> int:
        return self._n_eps

    @property
    def rho_e(self) -> float:
        return self._rho_e

    @property
    def reference_start(self) -> int:
        return int(self._scores["reference_start"])

    @property
    def comparison_start(self) -> int:
        return int(self._scores["comparison_start"])

    @property
    def shift_length(self) -> int:
        return int(self._scores["shift_length"])

    @staticmethod
    def _rating_value(value: float | int, ndigits: int) -> float:
        value_float = float(value)
        if ndigits < 0:
            return value_float
        return round(value_float, ndigits=ndigits)

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
