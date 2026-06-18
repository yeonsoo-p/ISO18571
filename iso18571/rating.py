from __future__ import annotations

from collections.abc import Callable
from typing import cast

import numpy as np
from numpy.typing import NDArray, ArrayLike

from . import _core

ScoreComponents = dict[str, float | int]
ScoreParams = dict[str, float | int]
ScoreComponentsFn = Callable[[ArrayLike, ArrayLike, ScoreParams], ScoreComponents]
_score_components = cast(ScoreComponentsFn, getattr(_core, "_score_components"))


class ISO18571:
    def __init__(
        self,
        reference_curve: NDArray[np.float32 | np.float64],
        comparison_curve: NDArray[np.float32 | np.float64],
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
        if reference_curve.shape != comparison_curve.shape:
            raise ValueError("Curves are not equal in size/dimension")

        params: ScoreParams = {
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
        }
        self._scores: ScoreComponents = _score_components(
            reference_curve,
            comparison_curve,
            params,
        )

        reference_start = int(self._scores["reference_start"])
        comparison_start = int(self._scores["comparison_start"])
        shift_length = int(self._scores["shift_length"])
        self._t_ts = reference_curve[
            reference_start : reference_start + shift_length, :
        ].copy()
        self._cae_ts = comparison_curve[
            comparison_start : comparison_start + shift_length, :
        ].copy()
        self._n_eps = int(self._scores["n_eps"])
        self._rho_e = float(self._scores["rho_e"])

    @property
    def scores(self) -> ScoreComponents:
        return dict(self._scores)

    @property
    def n_eps(self) -> int:
        return self._n_eps

    @property
    def rho_e(self) -> float:
        return self._rho_e

    @property
    def shifted_reference_curve(self) -> NDArray[np.float32 | np.float64]:
        return self._t_ts.copy()

    @property
    def shifted_comparison_curve(self) -> NDArray[np.float32 | np.float64]:
        return self._cae_ts.copy()

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
