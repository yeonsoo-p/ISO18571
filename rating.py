import numpy as np


class CurveLengthError(Exception):
    pass


class DTWBackendError(Exception):
    pass


DTW_BACKEND_LOCAL_ISO = "local_iso_numpy"
DTW_BACKEND_LOCAL_ISO_NATIVE = "local_iso_native"
DTW_BACKEND_DTWALIGN = "dtwalign"
DTW_BACKEND_DTAIDISTANCE = "dtaidistance"
DTW_BACKEND_DTW_PYTHON = "dtw_python"
DTW_BACKEND_TSLEARN = "tslearn"
DTW_BACKEND_LIBROSA = "librosa"

DTW_BACKEND_ALIASES = {
    DTW_BACKEND_LOCAL_ISO: DTW_BACKEND_LOCAL_ISO,
    "local": DTW_BACKEND_LOCAL_ISO,
    "numpy": DTW_BACKEND_LOCAL_ISO,
    DTW_BACKEND_LOCAL_ISO_NATIVE: DTW_BACKEND_LOCAL_ISO_NATIVE,
    "native": DTW_BACKEND_LOCAL_ISO_NATIVE,
    DTW_BACKEND_DTWALIGN: DTW_BACKEND_DTWALIGN,
    DTW_BACKEND_DTAIDISTANCE: DTW_BACKEND_DTAIDISTANCE,
    DTW_BACKEND_DTW_PYTHON: DTW_BACKEND_DTW_PYTHON,
    "dtw-python": DTW_BACKEND_DTW_PYTHON,
    "dtwpython": DTW_BACKEND_DTW_PYTHON,
    DTW_BACKEND_TSLEARN: DTW_BACKEND_TSLEARN,
    DTW_BACKEND_LIBROSA: DTW_BACKEND_LIBROSA,
}

DTW_BACKENDS = (
    DTW_BACKEND_LOCAL_ISO,
    DTW_BACKEND_LOCAL_ISO_NATIVE,
    DTW_BACKEND_DTWALIGN,
    DTW_BACKEND_DTAIDISTANCE,
    DTW_BACKEND_DTW_PYTHON,
    DTW_BACKEND_TSLEARN,
    DTW_BACKEND_LIBROSA,
)


def _normalise_dtw_backend(backend: str) -> str:
    try:
        return DTW_BACKEND_ALIASES[backend]
    except KeyError as exc:
        raise ValueError(f"Unknown DTW backend '{backend}'. Valid backends: {', '.join(DTW_BACKENDS)}") from exc


def _dtw_window_radius(n: int, window_size: float) -> int:
    if n <= 0:
        raise DTWBackendError("DTW input arrays must not be empty")
    return max(1, int(np.ceil(window_size * n)))


def _local_cost_matrix(x: np.ndarray, y: np.ndarray, window_size: float) -> np.ndarray:
    n = x.shape[0]
    if y.shape[0] != n:
        raise DTWBackendError("ISO/TS 18571 magnitude DTW expects equal-length shifted curves")

    radius = _dtw_window_radius(n, window_size)
    cost = np.square(x[:, np.newaxis] - y[np.newaxis, :])
    idx = np.arange(n)
    outside_window = np.abs(idx[:, np.newaxis] - idx[np.newaxis, :]) >= radius
    cost[outside_window] = np.inf
    return cost


def _correlation_from_sums(
    product_sum: np.ndarray,
    x_sum: np.ndarray,
    y_sum: np.ndarray,
    x_square_sum: np.ndarray,
    y_square_sum: np.ndarray,
    length: np.ndarray,
) -> np.ndarray:
    numerator = product_sum - (x_sum * y_sum / length)
    x_var = x_square_sum - (x_sum * x_sum / length)
    y_var = y_square_sum - (y_sum * y_sum / length)
    x_scale = np.maximum(x_square_sum, np.abs(x_sum * x_sum / length))
    y_scale = np.maximum(y_square_sum, np.abs(y_sum * y_sum / length))
    x_tol = np.finfo(float).eps * np.maximum(1.0, x_scale) * 64.0
    y_tol = np.finfo(float).eps * np.maximum(1.0, y_scale) * 64.0
    with np.errstate(divide="ignore", invalid="ignore"):
        correlation = numerator / np.sqrt(x_var * y_var)
    correlation = np.where((x_var <= x_tol) | (y_var <= y_tol), np.nan, correlation)
    return np.clip(correlation, -1.0, 1.0)


def _near_zero_variance_mask(
    value_sum: np.ndarray,
    square_sum: np.ndarray,
    length: np.ndarray,
) -> np.ndarray:
    variance = square_sum - (value_sum * value_sum / length)
    scale = np.maximum(square_sum, np.abs(value_sum * value_sum / length))
    tolerance = np.finfo(float).eps * np.maximum(1.0, scale) * 64.0
    return variance <= tolerance


def _shifted_correlations(
    reference_values: np.ndarray, comparison_values: np.ndarray, window_size: int
) -> tuple[np.ndarray, np.ndarray]:
    n = reference_values.shape[0]
    if comparison_values.shape[0] != n:
        raise CurveLengthError("Curves are not equal in size/dimension.\nInterpolation not implemented. ")

    window_size = min(window_size, n)
    shifts = np.arange(window_size, dtype=np.int64)
    lengths = (n - shifts).astype(np.float64)

    reference_prefix = np.concatenate(([0.0], np.cumsum(reference_values)))
    comparison_prefix = np.concatenate(([0.0], np.cumsum(comparison_values)))
    reference_square_prefix = np.concatenate(([0.0], np.cumsum(reference_values * reference_values)))
    comparison_square_prefix = np.concatenate(([0.0], np.cumsum(comparison_values * comparison_values)))
    product_by_lag = np.correlate(reference_values, comparison_values, mode="full")

    left_reference_sum = reference_prefix[n - shifts]
    left_comparison_sum = comparison_prefix[n] - comparison_prefix[shifts]
    left_reference_square_sum = reference_square_prefix[n - shifts]
    left_comparison_square_sum = comparison_square_prefix[n] - comparison_square_prefix[shifts]
    left_product_sum = product_by_lag[n - 1 - shifts]

    right_reference_sum = reference_prefix[n] - reference_prefix[shifts]
    right_comparison_sum = comparison_prefix[n - shifts]
    right_reference_square_sum = reference_square_prefix[n] - reference_square_prefix[shifts]
    right_comparison_square_sum = comparison_square_prefix[n - shifts]
    right_product_sum = product_by_lag[n - 1 + shifts]

    left_correlations = _correlation_from_sums(
        left_product_sum,
        left_reference_sum,
        left_comparison_sum,
        left_reference_square_sum,
        left_comparison_square_sum,
        lengths,
    )
    left_fallback = _near_zero_variance_mask(
        left_reference_sum,
        left_reference_square_sum,
        lengths,
    ) | _near_zero_variance_mask(
        left_comparison_sum,
        left_comparison_square_sum,
        lengths,
    )
    for idx in np.flatnonzero(left_fallback):
        shift = int(shifts[idx])
        if shift == 0:
            left_correlations[idx] = np.corrcoef(reference_values, comparison_values)[0][-1]
        else:
            left_correlations[idx] = np.corrcoef(reference_values[:-shift], comparison_values[shift:])[0][-1]

    right_correlations = _correlation_from_sums(
        right_product_sum,
        right_reference_sum,
        right_comparison_sum,
        right_reference_square_sum,
        right_comparison_square_sum,
        lengths,
    )
    right_fallback = _near_zero_variance_mask(
        right_reference_sum,
        right_reference_square_sum,
        lengths,
    ) | _near_zero_variance_mask(
        right_comparison_sum,
        right_comparison_square_sum,
        lengths,
    )
    for idx in np.flatnonzero(right_fallback):
        shift = int(shifts[idx])
        if shift == 0:
            right_correlations[idx] = np.corrcoef(reference_values, comparison_values)[0][-1]
        else:
            right_correlations[idx] = np.corrcoef(reference_values[shift:], comparison_values[:-shift])[0][-1]

    return left_correlations, right_correlations


def _iso_accumulated_cost_matrix(cost: np.ndarray) -> np.ndarray:
    n, m = cost.shape
    if n != m:
        raise DTWBackendError("ISO/TS 18571 magnitude DTW expects a square local cost matrix")

    accumulated = np.full((n, m), np.inf)
    finite_rows, finite_cols = np.where(np.isfinite(cost))
    if finite_rows.size == 0:
        raise DTWBackendError("No finite local DTW costs found")
    radius = int(np.max(np.abs(finite_rows - finite_cols)) + 1)

    for i in range(n):
        j_start = max(0, i - radius + 1)
        j_stop = min(m, i + radius)
        for j in range(j_start, j_stop):
            local_cost = cost[i, j]
            if i == 0 and j == 0:
                accumulated[i, j] = local_cost
                continue

            best_previous = np.inf
            if i > 0:
                best_previous = min(best_previous, accumulated[i - 1, j])
            if j > 0:
                best_previous = min(best_previous, accumulated[i, j - 1])
            if i > 0 and j > 0:
                best_previous = min(best_previous, accumulated[i - 1, j - 1])
            if np.isfinite(best_previous):
                accumulated[i, j] = local_cost + best_previous
    return accumulated


def _iso_backtrack(accumulated: np.ndarray) -> np.ndarray:
    accumulated = np.where(np.isnan(accumulated), np.inf, accumulated)
    n, m = accumulated.shape
    if n != m:
        raise DTWBackendError("ISO/TS 18571 backtracking expects a square accumulated cost matrix")
    if not np.isfinite(accumulated[-1, -1]):
        raise DTWBackendError("No valid ISO DTW path found")

    i = n - 1
    j = m - 1
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
        best_idx = int(np.argmin(costs))
        best_cost, i, j = candidates[best_idx]
        if not np.isfinite(best_cost):
            raise DTWBackendError("No valid ISO DTW predecessor found")
        path.append((i, j))

    path.reverse()
    return np.asarray(path, dtype=np.int64)


def _compute_magnitude_local_iso_numpy(
    x: np.ndarray, y: np.ndarray, window_size: float
) -> tuple[np.ndarray, np.ndarray]:
    cost = _local_cost_matrix(x, y, window_size)
    accumulated = _iso_accumulated_cost_matrix(cost)
    path = _iso_backtrack(accumulated)
    return x[path[:, 0]], y[path[:, 1]]


def _compute_magnitude_local_iso_native(
    x: np.ndarray, y: np.ndarray, window_size: float
) -> tuple[np.ndarray, np.ndarray]:
    try:
        from iso18571_native import warp_path
    except ImportError as exc:
        raise DTWBackendError("The native ISO/TS 18571 backend is not built") from exc

    path = warp_path(
        np.asarray(x, dtype=np.float64),
        np.asarray(y, dtype=np.float64),
        window_size,
    )
    return x[path[:, 0]], y[path[:, 1]]


def _compute_magnitude_ratio_local_iso_native(x: np.ndarray, y: np.ndarray, window_size: float) -> float:
    try:
        from iso18571_native import magnitude_ratio
    except ImportError as exc:
        raise DTWBackendError("The native ISO/TS 18571 backend is not built") from exc

    return float(
        magnitude_ratio(
            np.asarray(x, dtype=np.float64),
            np.asarray(y, dtype=np.float64),
            window_size,
        )
    )


def _score_components_local_iso_native(
    reference_curve: np.ndarray,
    comparison_curve: np.ndarray,
    *,
    k_z: float,
    k_p: int,
    k_m: int,
    eps_m: float,
    e_s: float,
    init_min: float,
    a_0: float,
    b_0: float,
    w_z: float,
    w_p: float,
    w_m: float,
    w_s: float,
    dt: float,
) -> dict[str, float]:
    try:
        from iso18571_native import score_components
    except ImportError as exc:
        raise DTWBackendError("The native ISO/TS 18571 backend is not built") from exc

    return score_components(
        np.asarray(reference_curve, dtype=np.float64),
        np.asarray(comparison_curve, dtype=np.float64),
        {
            "k_z": int(k_z),
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


def _compute_magnitude_dtwalign(x: np.ndarray, y: np.ndarray, window_size: float) -> tuple[np.ndarray, np.ndarray]:
    try:
        from dtwalign import dtw_low
        from dtwalign.step_pattern import UserStepPattern
        from dtwalign.window import SakoechibaWindow
    except ImportError as exc:
        raise DTWBackendError("The 'dtwalign' package is required for dtw_backend='dtwalign'") from exc

    n = x.shape[0]
    radius = _dtw_window_radius(n, window_size)
    pattern_info = [
        dict(indices=[(-1, 0), (0, 0)], weights=[1]),
        dict(indices=[(0, -1), (0, 0)], weights=[1]),
        dict(indices=[(-1, -1), (0, 0)], weights=[1]),
    ]
    user_step_pattern = UserStepPattern(pattern_info, normalize_guide="none")
    cost = np.square(x[:, np.newaxis] - y[np.newaxis, :])
    window = SakoechibaWindow(cost.shape[0], cost.shape[1], size=radius - 1)
    res = dtw_low(cost, window=window, pattern=user_step_pattern)
    return x[res.path[:, 0]], y[res.path[:, 1]]


def _compute_magnitude_dtaidistance(x: np.ndarray, y: np.ndarray, window_size: float) -> tuple[np.ndarray, np.ndarray]:
    try:
        from dtaidistance import dtw
    except ImportError as exc:
        raise DTWBackendError("The 'dtaidistance' package is required for dtw_backend='dtaidistance'") from exc

    radius = _dtw_window_radius(x.shape[0], window_size)
    _, paths = dtw.warping_paths(
        x.astype(np.double),
        y.astype(np.double),
        window=radius,
        inner_dist="squared euclidean",
        psi_neg=False,
        use_c=False,
    )
    path = _iso_backtrack(paths[1:, 1:])
    return x[path[:, 0]], y[path[:, 1]]


def _compute_magnitude_dtw_python(x: np.ndarray, y: np.ndarray, window_size: float) -> tuple[np.ndarray, np.ndarray]:
    try:
        import dtw
    except ImportError as exc:
        raise DTWBackendError("The 'dtw-python' package is required for dtw_backend='dtw_python'") from exc

    radius = _dtw_window_radius(x.shape[0], window_size)
    cost = np.square(x[:, np.newaxis] - y[np.newaxis, :])
    res = dtw.dtw(
        cost,
        step_pattern=dtw.symmetric1,
        window_type="sakoechiba",
        window_args={"window_size": radius - 1},
        keep_internals=True,
    )
    path = _iso_backtrack(res.costMatrix)
    return x[path[:, 0]], y[path[:, 1]]


def _compute_magnitude_tslearn(x: np.ndarray, y: np.ndarray, window_size: float) -> tuple[np.ndarray, np.ndarray]:
    try:
        from tslearn.metrics import dtw_path_from_metric
    except ImportError as exc:
        raise DTWBackendError("The 'tslearn' package is required for dtw_backend='tslearn'") from exc

    radius = _dtw_window_radius(x.shape[0], window_size) - 1
    path, _ = dtw_path_from_metric(
        x,
        y,
        metric=lambda a, b: (a - b) ** 2,
        global_constraint="sakoe_chiba",
        sakoe_chiba_radius=radius,
    )
    path = np.asarray(path, dtype=np.int64)
    return x[path[:, 0]], y[path[:, 1]]


def _compute_magnitude_librosa(x: np.ndarray, y: np.ndarray, window_size: float) -> tuple[np.ndarray, np.ndarray]:
    try:
        import librosa
    except ImportError as exc:
        raise DTWBackendError("The 'librosa' package is required for dtw_backend='librosa'") from exc

    cost = _local_cost_matrix(x, y, window_size)
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


DTW_BACKEND_FUNCTIONS = {
    DTW_BACKEND_LOCAL_ISO: _compute_magnitude_local_iso_numpy,
    DTW_BACKEND_LOCAL_ISO_NATIVE: _compute_magnitude_local_iso_native,
    DTW_BACKEND_DTWALIGN: _compute_magnitude_dtwalign,
    DTW_BACKEND_DTAIDISTANCE: _compute_magnitude_dtaidistance,
    DTW_BACKEND_DTW_PYTHON: _compute_magnitude_dtw_python,
    DTW_BACKEND_TSLEARN: _compute_magnitude_tslearn,
    DTW_BACKEND_LIBROSA: _compute_magnitude_librosa,
}


class ISO18571:
    """This class is used to calculate the Objective Rating Metric for non-ambiguous signals as defined in
        ISO/TS 18571

    Methods of the Class:
        - _cross_correlation:
        - _compute_magnitude:
        - _get_shifted_curve_and_pr:
        - corridor_rating:
        - phase_rating:  calculates the phase rating according to ISO18571
        - magnitude_rating: calculates the magnitude rating according to ISO18571
        - slope_rating: calculates the slope rating according to ISO18571
        - overall_rating: calculates the weighted overall rating according to ISO18571

    Attributes of the Class:
        - method (str): Indicates the method on which the Objective rating method is based on
        - comparison_curve (np.ndarray): xxx
        - reference_curve (np.ndarray): xxx
        - max_shift (float): xxx
        - cae_ts (np.ndarray): xxx
        - t_ts (np.ndarray): xxx
        - n_eps (int):
        - rho_e (float):
        - dt (float):

    """

    def __init__(
        self,
        reference_curve: np.ndarray,
        comparison_curve: np.ndarray,
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
        dtw_backend: str = DTW_BACKEND_LOCAL_ISO,
    ):
        """Constructor of ISO18571

        Args:
            reference_curve (np.ndarray): numpy array of shape (n, 2). First column must contain the time vector in [s]
                                          with sampling frequency of 10 kHz. Second column contains the reference curve
            comparison_curve (np.ndarray): numpy array of shape (n, 2). First column must contain the time vector in [s]
                                          with sampling frequency of 10 kHz. Second column contains the comparison curve
            k_z (float):
            k_p (int):
            k_m (int):
            eps_m (float):
            e_s (float):
            init_min (float):
            a_0  (float):
            b_0  (float):
            w_z  (float):  Weighting factor of the corridor score
            w_p  (float):  Weighting factor of the phase score
            w_m  (float):  Weighting factor of the magnitude score
            w_s  (float):  Weighting factor of the slope score
            dt (float): Delta t for central differences
            dtw_backend (str): Backend for the magnitude-score DTW path.

        Raises:
            ValueError: if sum of weighting factors (w_z, w_m, w_p, w_s) is unequal to 1
        """

        self.reference_curve = reference_curve
        self.comparison_curve = comparison_curve

        # sanity checks for the two curves
        if self.reference_curve.shape != self.comparison_curve.shape:
            raise CurveLengthError("Curves are not equal in size/dimension.\nInterpolation not implemented. ")

        self._k_z = k_z
        if self._k_z not in [1, 2, 3]:
            raise ValueError("k_z has to be 1, 2, or 3")

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
            raise ValueError("Sum of weighting factors (w_z, w_m, w_p, w_s) is " + str(weights_sum) + ", but must be 1")

        self.dt = dt
        self._dtw_backend = _normalise_dtw_backend(dtw_backend)

        self._max_shift = round(1.0 - self._init_min, 2)

        self._native_scores = None
        if self._dtw_backend == DTW_BACKEND_LOCAL_ISO_NATIVE:
            self._native_scores = _score_components_local_iso_native(
                self.reference_curve,
                self.comparison_curve,
                k_z=self._k_z,
                k_p=self._k_p,
                k_m=self._k_m,
                eps_m=self._eps_m,
                e_s=self._e_s,
                init_min=self._init_min,
                a_0=self._a_0,
                b_0=self._b_0,
                w_z=self._w_z,
                w_p=self._w_p,
                w_m=self._w_m,
                w_s=self._w_s,
                dt=self.dt,
            )
            reference_start = int(self._native_scores["reference_start"])
            comparison_start = int(self._native_scores["comparison_start"])
            shift_length = int(self._native_scores["shift_length"])
            self._t_ts = self.reference_curve[reference_start : reference_start + shift_length, :].copy()
            self._cae_ts = self.comparison_curve[comparison_start : comparison_start + shift_length, :].copy()
            self._n_eps = int(self._native_scores["n_eps"])
            self._rho_e = float(self._native_scores["rho_e"])
        else:
            self._cae_ts, self._t_ts, self._n_eps, self._rho_e = self._get_shifted_curve_and_pr()

    @staticmethod
    def _rating_value(value: float, ndigits: int):
        if ndigits < 0:
            return value
        return round(value, ndigits=ndigits)

    def _get_shifted_curve_and_pr(self) -> (np.ndarray, np.ndarray, float, float):
        """Calculates the shifted curves, yielded by the highest correlation coefficients

        Returns: The shifted curves cae_ts, t_ts, the maximum cross correlation rho_e and the shifting values,
        which yields rho_e
        """

        window_size = int(np.floor(len(self.comparison_curve[:, 1]) * self._max_shift) + 1)
        left_correlations, right_correlations = _shifted_correlations(
            self.reference_curve[:, 1],
            self.comparison_curve[:, 1],
            window_size,
        )
        # No shift, initialize values
        ccr_max = left_correlations[0]
        idx_ccr_max = 0
        t_ts = self.reference_curve
        cae_ts = self.comparison_curve
        for idx in range(1, window_size):
            # Shift comparison curve left (first left to prioritize rho_L(m), see ISO 18571)
            ccr_left = left_correlations[idx]
            # Update only if strictly greater to use minimum time shifting if maximum cross correlation would be
            # ambiguous (see ISO 18571)
            if ccr_left > ccr_max:
                ccr_max = ccr_left
                idx_ccr_max = idx
                t_ts = self.reference_curve[:-idx, :]
                cae_ts = self.comparison_curve[idx:, :]

            # Shift comparison curve right
            ccr_right = right_correlations[idx]
            # Update only if strictly greater to use minimum time shifting if maximum cross correlation would be
            # ambiguous (see ISO 18571)
            if ccr_right > ccr_max:
                ccr_max = ccr_right
                idx_ccr_max = idx
                t_ts = self.reference_curve[idx:, :]
                cae_ts = self.comparison_curve[:-idx, :]

        rho_e = ccr_max
        n_eps = idx_ccr_max

        return cae_ts.copy(), t_ts.copy(), n_eps, rho_e

    @staticmethod
    def _compute_magnitude(x, y, window_size, backend: str = DTW_BACKEND_LOCAL_ISO):
        """

        Args:
            x:
            y:
            window_size:
            backend:

        Returns:

        """

        backend = _normalise_dtw_backend(backend)
        return DTW_BACKEND_FUNCTIONS[backend](x, y, window_size)

    def corridor_rating(self, ndigits: int = 3):
        """Returns the corridor rating for the comparison_curve and the reference_curve

        The corridor score indicates the deviation of the two curves by means of corridor fitting.
        inner and outer corridors form 3 different zones. In the most inner, the fit is rated with 1, in the most
        outer zone, the fit is 0. In between, the value of the fit is affected by the K, which indicates
        the regression relationship. a_0 and b_0 are used to calculate the inner and the outer corridor,
        defining the relative half widths of both.

        Args:
            ndigits (int): precision of ndigits. If negative result is not rounded.

        Returns: the corridor rating, with a precision of ndigits

        """

        if self._native_scores is not None:
            return ISO18571._rating_value(self._native_scores["Z"], ndigits)

        t_norm = max(np.abs(self.reference_curve[:, 1]))
        inner_corridor = self._a_0 * t_norm
        outer_corridor = self._b_0 * t_norm

        abs_diff = np.absolute(np.subtract(self.reference_curve, self.comparison_curve))

        # regression for in between corridor
        c_i = np.array(pow(((outer_corridor - abs_diff[:, 1]) / (outer_corridor - inner_corridor)), self._k_z))

        # points in the inner corridor are rated with 1
        c_i[abs_diff[:, 1] < inner_corridor] = 1

        # points in the outer corridor are rated with 0
        c_i[abs_diff[:, 1] > outer_corridor] = 0

        z = sum(c_i) / len(abs_diff)

        if ndigits < 0:
            return z

        return round(z, ndigits=ndigits)

    def phase_rating(self, ndigits: int = 3):
        """Returns the phase score e_p, which is in the range 0 to 1.
         1, if there is no shift between reference and comparison curve
         0, if the maximum allowable time shift threshold has been exceeded.
         In between, the value is calculated by a regression model, depending on k_p

        Args:
            ndigits (int): precision of ndigits. If negative result is not rounded.

        Returns: the phase rating, with a precision of ndigits
        """

        if self._native_scores is not None:
            return ISO18571._rating_value(self._native_scores["EP"], ndigits)

        curve_size = self.reference_curve.shape[0]

        # maximum allowable time shift threshold
        # eps_p * N
        max_allowable_time_shift_threshold = curve_size * self._max_shift  # int(round(curve_size * max_shift))

        if self._n_eps == 0:
            e_p = 1
        elif abs(self._n_eps) >= max_allowable_time_shift_threshold:
            e_p = 0
        else:
            e_p = (
                (max_allowable_time_shift_threshold - abs(self._n_eps)) / max_allowable_time_shift_threshold
            ) ** self._k_p

        if ndigits < 0:
            return e_p

        return round(e_p, ndigits=ndigits)

    def magnitude_rating(self, ndigits: int = 3):
        """Returns the magnitude rating for the comparison_curve and the reference_curve

        Args:
             ndigits (int): precision of ndigits. If negative result is not rounded.

         Returns: the magnitude rating, with a precision of ndigits
        """

        if self._native_scores is not None:
            return ISO18571._rating_value(self._native_scores["EM"], ndigits)

        if self._dtw_backend == DTW_BACKEND_LOCAL_ISO_NATIVE:
            e_mag = _compute_magnitude_ratio_local_iso_native(
                self._cae_ts[:, 1],
                self._t_ts[:, 1],
                window_size=0.1,
            )
        else:
            cae_ts_w, t_ts_w = ISO18571._compute_magnitude(
                self._cae_ts[:, 1],
                self._t_ts[:, 1],
                window_size=0.1,
                backend=self._dtw_backend,
            )

            e_mag = np.linalg.norm(cae_ts_w - t_ts_w, ord=1) / np.linalg.norm(t_ts_w, ord=1)

        if e_mag == 0:
            score_mag = 1
        elif e_mag > self._eps_m:
            score_mag = 0
        else:
            score_mag = ((self._eps_m - e_mag) / self._eps_m) ** self._k_m

        if ndigits < 0:
            return score_mag

        return round(score_mag, ndigits=ndigits)

    def slope_rating(self, ndigits: int = 3):
        """Returns the slope rating for the comparison_curve and the reference_curve

        Args:
             ndigits (int): precision of ndigits. If negative result is not rounded.

         Returns: the slope rating, with a precision of ndigits
        """

        if self._native_scores is not None:
            return ISO18571._rating_value(self._native_scores["ES"], ndigits)

        # central difference
        cae_ts_0_d = np.gradient(self._cae_ts[:, 1], self.dt)
        t_ts_0_d = np.gradient(self._t_ts[:, 1], self.dt)

        cae_ts_d = np.zeros(len(cae_ts_0_d))
        t_ts_d = np.zeros(len(t_ts_0_d))

        # case 1/1
        for idx, nr in enumerate([1, 3, 5, 7]):
            idx_begin = idx
            idx_end = (-1) * idx - 1
            cae_ts_d[idx_begin] = 1 / nr * np.sum(cae_ts_0_d[0:nr])
            cae_ts_d[idx_end] = 1 / nr * np.sum(cae_ts_0_d[-nr:])
            t_ts_d[idx_begin] = 1 / nr * np.sum(t_ts_0_d[0:nr])
            t_ts_d[idx_end] = 1 / nr * np.sum(t_ts_0_d[-nr:])

        # case 1/9
        nr = 9
        cae_ts_d[4:-4] = np.convolve(cae_ts_0_d, np.ones(nr) / nr, mode="valid")
        t_ts_d[4:-4] = np.convolve(t_ts_0_d, np.ones(nr) / nr, mode="valid")

        e_slope = (np.linalg.norm((cae_ts_d - t_ts_d), ord=1)) / (np.linalg.norm(t_ts_d, ord=1))

        if e_slope <= 0:
            slope_score = 1
        elif e_slope >= self._e_s:
            slope_score = 0
        else:
            slope_score = (self._e_s - e_slope) / self._e_s

        if ndigits < 0:
            return slope_score

        return round(slope_score, ndigits=ndigits)

    def overall_rating(self, ndigits: int = 3) -> float:
        """Returns the overall rating for the comparison_curve and the reference_curve

         Combines the four metric ratings corridor, phase, magnitude and slope to a single number.
         Each rating is weighted with an according weighting factor as indicated in the ISO document.

        Args:
             ndigits (int): precision of ndigits. If negative result is not rounded.

         Returns: overall_rating, which indicates the objective correlation of the analyzed signals, with a
                  precision of ndigits
        """

        if self._native_scores is not None:
            return ISO18571._rating_value(self._native_scores["R"], ndigits)

        z = self.corridor_rating(ndigits=-1)
        e_p = self.phase_rating(ndigits=-1)
        e_m = self.magnitude_rating(ndigits=-1)
        e_s = self.slope_rating(ndigits=-1)

        overall_rating = self._w_z * z + self._w_p * e_p + self._w_m * e_m + self._w_s * e_s

        if ndigits < 0:
            return overall_rating

        return round(overall_rating, ndigits=ndigits)
