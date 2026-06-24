from __future__ import annotations

from typing import (
    TypedDict,
    TypeAlias,
)
import numpy as np
from numpy.typing import NDArray

from ._core import _score_components

SignedInteger: TypeAlias = np.int8 | np.int16 | np.int32 | np.int64
UnsignedInteger: TypeAlias = np.uint8 | np.uint16 | np.uint32 | np.uint64
FloatingPoint: TypeAlias = np.float16 | np.float32 | np.float64 | np.longdouble
ComplexFloat: TypeAlias = np.complex64 | np.complex128 | np.clongdouble
Numeric: TypeAlias = SignedInteger | UnsignedInteger | FloatingPoint | ComplexFloat

NumericArray: TypeAlias = NDArray[Numeric]


class ScoreComponents(TypedDict):
    # Scores
    Z: float
    EP: float
    EM: float
    ES: float
    R: float

    # Input validation
    dt: float

    # Corridor validation
    corridor_t_norm: float
    corridor_inner_half_width: float
    corridor_outer_half_width: float

    # Phase validation
    phase_n_eps: int
    phase_rho_e: float
    phase_reference_start: int
    phase_comparison_start: int
    phase_shift_length: int
    phase_max_shift: float

    # Magnitude validation
    magnitude_numerator: float
    magnitude_denominator: float
    magnitude_error: float

    # Slope validation
    slope_numerator: float
    slope_denominator: float
    slope_error: float


class ISO18571:
    def __init__(
        self,
        reference_curve: NumericArray,
        comparison_curve: NumericArray,
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

        self._reference_curve = np.asarray(reference_curve)
        self._comparison_curve = np.asarray(comparison_curve)
        self._k_z = int(k_z)

        self._scores: ScoreComponents
        self._scores = _score_components(
            self._reference_curve,
            self._comparison_curve,
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

    @property
    def scores(self) -> ScoreComponents:
        return self._scores.copy()

    @property
    def dt(self) -> float:
        return self._scores["dt"]

    @property
    def phase_n_eps(self) -> int:
        return self._scores["phase_n_eps"]

    @property
    def phase_rho_e(self) -> float:
        return self._scores["phase_rho_e"]

    @property
    def phase_max_shift(self) -> float:
        return self._scores["phase_max_shift"]

    @property
    def corridor_t_norm(self) -> float:
        return self._scores["corridor_t_norm"]

    @property
    def corridor_inner_half_width(self) -> float:
        return self._scores["corridor_inner_half_width"]

    @property
    def corridor_outer_half_width(self) -> float:
        return self._scores["corridor_outer_half_width"]

    @property
    def magnitude_numerator(self) -> float:
        return self._scores["magnitude_numerator"]

    @property
    def magnitude_denominator(self) -> float:
        return self._scores["magnitude_denominator"]

    @property
    def magnitude_error(self) -> float:
        return self._scores["magnitude_error"]

    @property
    def slope_numerator(self) -> float:
        return self._scores["slope_numerator"]

    @property
    def slope_denominator(self) -> float:
        return self._scores["slope_denominator"]

    @property
    def slope_error(self) -> float:
        return self._scores["slope_error"]

    @property
    def shifted_reference_values(self) -> NumericArray:
        start = int(self._scores["phase_reference_start"])
        length = int(self._scores["phase_shift_length"])
        values: NumericArray = self._reference_curve[start : start + length, 1]
        return values

    @property
    def shifted_comparison_values(self) -> NumericArray:
        start = int(self._scores["phase_comparison_start"])
        length = int(self._scores["phase_shift_length"])
        values: NumericArray = self._comparison_curve[start : start + length, 1]
        return values

    @property
    def corridor_outer_upper_values(self) -> NumericArray:
        return self._reference_curve[:, 1] + self._scores["corridor_outer_half_width"]

    @property
    def corridor_inner_upper_values(self) -> NumericArray:
        return self._reference_curve[:, 1] + self._scores["corridor_inner_half_width"]

    @property
    def corridor_inner_lower_values(self) -> NumericArray:
        return self._reference_curve[:, 1] - self._scores["corridor_inner_half_width"]

    @property
    def corridor_outer_lower_values(self) -> NumericArray:
        return self._reference_curve[:, 1] - self._scores["corridor_outer_half_width"]

    @property
    def corridor_point_scores(self) -> NDArray[np.float64]:
        t_norm = float(self._scores["corridor_t_norm"])
        inner = float(self._scores["corridor_inner_half_width"])
        outer = float(self._scores["corridor_outer_half_width"])

        reference_values = np.asarray(self._reference_curve[:, 1])
        comparison_values = np.asarray(self._comparison_curve[:, 1])
        diff = np.abs(reference_values - comparison_values)

        if t_norm == 0.0:
            zero_scores: NDArray[np.float64] = (diff == 0.0).astype(np.float64)
            return zero_scores

        point_scores: NDArray[np.float64] = (
            (outer - diff) / (outer - inner)
        ) ** self._k_z
        point_scores[diff < inner] = 1.0
        point_scores[diff > outer] = 0.0
        return point_scores

    @staticmethod
    def _gradient_values(values: NumericArray, dt: float) -> NumericArray:
        values_array = np.asarray(values)
        gradient = np.zeros(values_array.shape[0], dtype=np.float64)
        gradient[0] = (values_array[1] - values_array[0]) / dt
        gradient[-1] = (values_array[-1] - values_array[-2]) / dt
        gradient[1:-1] = (values_array[2:] - values_array[:-2]) / (2.0 * dt)
        return gradient

    @staticmethod
    def _smoothed_slope_values(gradient: NumericArray) -> NumericArray:
        gradient_array = np.asarray(gradient)
        smoothed = np.zeros(gradient_array.shape[0], dtype=np.float64)

        for idx, nr in enumerate((1, 3, 5, 7)):
            smoothed[idx] = np.sum(gradient_array[:nr]) / nr
            smoothed[-idx - 1] = np.sum(gradient_array[-nr:]) / nr

        for idx in range(4, gradient_array.shape[0] - 4):
            smoothed[idx] = np.sum(gradient_array[idx - 4 : idx + 5]) / 9.0

        return smoothed

    @property
    def raw_reference_slope_values(self) -> NumericArray:
        values = self.shifted_reference_values
        dt = self.dt
        return ISO18571._gradient_values(values, dt)

    @property
    def raw_comparison_slope_values(self) -> NumericArray:
        values = self.shifted_comparison_values
        dt = self.dt
        return ISO18571._gradient_values(values, dt)

    @property
    def smoothed_reference_slope_values(self) -> NumericArray:
        raw = self.raw_reference_slope_values
        return ISO18571._smoothed_slope_values(raw)

    @property
    def smoothed_comparison_slope_values(self) -> NumericArray:
        raw = self.raw_comparison_slope_values
        return ISO18571._smoothed_slope_values(raw)

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
