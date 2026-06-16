#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace py = pybind11;

namespace {

constexpr std::uint8_t DIR_NONE = 0;
constexpr std::uint8_t DIR_VERTICAL = 1;
constexpr std::uint8_t DIR_HORIZONTAL = 2;
constexpr std::uint8_t DIR_DIAGONAL = 3;

struct DtwState {
    py::ssize_t n = 0;
    py::ssize_t radius = 0;
    py::ssize_t band_width = 0;
    std::vector<std::uint8_t> directions;
};

struct ArrayView {
    const char* data = nullptr;
    py::ssize_t stride = 0;
    py::ssize_t n = 0;

    double operator[](py::ssize_t index) const {
        return *reinterpret_cast<const double*>(data + index * stride);
    }
};

struct CurveView {
    const char* data = nullptr;
    py::ssize_t row_stride = 0;
    py::ssize_t column_stride = 0;
    py::ssize_t n = 0;

    double value(py::ssize_t index) const {
        return *reinterpret_cast<const double*>(data + index * row_stride + column_stride);
    }
};

struct ScoreParams {
    int k_z = 2;
    int k_p = 1;
    int k_m = 1;
    double eps_m = 0.50;
    double e_s = 2.0;
    double init_min = 0.8;
    double a_0 = 0.05;
    double b_0 = 0.5;
    double w_z = 0.4;
    double w_p = 0.2;
    double w_m = 0.2;
    double w_s = 0.2;
    double dt = 0.0001;
};

struct ShiftResult {
    py::ssize_t reference_start = 0;
    py::ssize_t comparison_start = 0;
    py::ssize_t length = 0;
    py::ssize_t n_eps = 0;
    double rho_e = 0.0;
    double max_shift = 0.2;
};

struct ScoreResult {
    double z = 0.0;
    double ep = 0.0;
    double em = 0.0;
    double es = 0.0;
    double r = 0.0;
    ShiftResult shift;
};

struct PhaseCache {
    std::vector<double> reference_sum;
    std::vector<double> comparison_sum;
    std::vector<double> reference_square_sum;
    std::vector<double> comparison_square_sum;
};

py::ssize_t window_radius(py::ssize_t n, double window_size) {
    if (n <= 0) {
        throw std::invalid_argument("DTW input arrays must not be empty");
    }
    if (!std::isfinite(window_size) || window_size < 0.0) {
        throw std::invalid_argument("DTW window_size must be a finite non-negative value");
    }

    const auto raw = static_cast<py::ssize_t>(std::ceil(window_size * static_cast<double>(n)));
    return std::max<py::ssize_t>(1, raw);
}

bool valid_cell(py::ssize_t i, py::ssize_t j, py::ssize_t n, py::ssize_t radius) {
    return i >= 0 && i < n && j >= 0 && j < n && std::llabs(i - j) < radius;
}

py::ssize_t direction_index(py::ssize_t i, py::ssize_t j, py::ssize_t radius, py::ssize_t band_width) {
    const py::ssize_t row_start = i - radius + 1;
    const py::ssize_t offset = j - row_start;
    return i * band_width + offset;
}

template <typename Func>
DtwState compute_directions(
    const ArrayView& x,
    const ArrayView& y,
    py::ssize_t n,
    double window_size,
    Func&& on_accumulated
) {
    DtwState state;
    state.n = n;
    state.radius = window_radius(n, window_size);
    state.band_width = 2 * state.radius - 1;
    state.directions.assign(static_cast<std::size_t>(n * state.band_width), DIR_NONE);

    const double inf = std::numeric_limits<double>::infinity();
    std::vector<double> previous(static_cast<std::size_t>(n), inf);
    std::vector<double> current(static_cast<std::size_t>(n), inf);

    for (py::ssize_t i = 0; i < n; ++i) {
        const py::ssize_t j_start = std::max<py::ssize_t>(0, i - state.radius + 1);
        const py::ssize_t j_stop = std::min<py::ssize_t>(n, i + state.radius);

        for (py::ssize_t j = j_start; j < j_stop; ++j) {
            const double delta = x[i] - y[j];
            const double local_cost = delta * delta;
            double accumulated = inf;
            std::uint8_t direction = DIR_NONE;

            if (i == 0 && j == 0) {
                accumulated = local_cost;
            } else {
                double best_previous = inf;

                if (i > 0 && valid_cell(i - 1, j, n, state.radius)) {
                    best_previous = previous[static_cast<std::size_t>(j)];
                    direction = DIR_VERTICAL;
                }
                if (j > 0 && j - 1 >= j_start) {
                    const double candidate = current[static_cast<std::size_t>(j - 1)];
                    if (candidate < best_previous) {
                        best_previous = candidate;
                        direction = DIR_HORIZONTAL;
                    }
                }
                if (i > 0 && j > 0 && valid_cell(i - 1, j - 1, n, state.radius)) {
                    const double candidate = previous[static_cast<std::size_t>(j - 1)];
                    if (candidate < best_previous) {
                        best_previous = candidate;
                        direction = DIR_DIAGONAL;
                    }
                }

                if (std::isfinite(best_previous)) {
                    accumulated = local_cost + best_previous;
                } else {
                    direction = DIR_NONE;
                }
            }

            current[static_cast<std::size_t>(j)] = accumulated;
            state.directions[static_cast<std::size_t>(direction_index(i, j, state.radius, state.band_width))] = direction;
            on_accumulated(i, j, accumulated);
        }

        std::swap(previous, current);
    }

    if (!std::isfinite(previous[static_cast<std::size_t>(n - 1)])) {
        throw std::runtime_error("No valid ISO DTW path found");
    }

    return state;
}

DtwState compute_directions(
    const ArrayView& x,
    const ArrayView& y,
    py::ssize_t n,
    double window_size
) {
    return compute_directions(x, y, n, window_size, [](py::ssize_t, py::ssize_t, double) {});
}

double magnitude_ratio_from_views(const ArrayView& x, const ArrayView& y, double window_size) {
    const auto n = x.n;
    const auto state = compute_directions(x, y, n, window_size);

    double numerator = 0.0;
    double denominator = 0.0;
    py::ssize_t i = n - 1;
    py::ssize_t j = n - 1;

    while (true) {
        numerator += std::abs(x[i] - y[j]);
        denominator += std::abs(y[j]);

        if (i == 0 && j == 0) {
            break;
        }

        const auto direction = state.directions[static_cast<std::size_t>(
            direction_index(i, j, state.radius, state.band_width)
        )];

        if (direction == DIR_VERTICAL) {
            --i;
        } else if (direction == DIR_HORIZONTAL) {
            --j;
        } else if (direction == DIR_DIAGONAL) {
            --i;
            --j;
        } else {
            throw std::runtime_error("No valid ISO DTW predecessor found");
        }
    }

    return numerator / denominator;
}

std::vector<std::pair<py::ssize_t, py::ssize_t>> backtrack_pairs(const DtwState& state) {
    std::vector<std::pair<py::ssize_t, py::ssize_t>> reversed;
    reversed.reserve(static_cast<std::size_t>(2 * state.n));

    py::ssize_t i = state.n - 1;
    py::ssize_t j = state.n - 1;

    while (true) {
        reversed.emplace_back(i, j);
        if (i == 0 && j == 0) {
            break;
        }

        if (!valid_cell(i, j, state.n, state.radius)) {
            throw std::runtime_error("Invalid ISO DTW path cell encountered");
        }

        const auto direction = state.directions[static_cast<std::size_t>(
            direction_index(i, j, state.radius, state.band_width)
        )];

        if (direction == DIR_VERTICAL) {
            --i;
        } else if (direction == DIR_HORIZONTAL) {
            --j;
        } else if (direction == DIR_DIAGONAL) {
            --i;
            --j;
        } else {
            throw std::runtime_error("No valid ISO DTW predecessor found");
        }
    }

    std::reverse(reversed.begin(), reversed.end());
    return reversed;
}

template <typename T>
T get_param(const py::dict& params, const char* name, T default_value) {
    if (params.contains(name)) {
        return py::cast<T>(params[py::str(name)]);
    }
    return default_value;
}

ScoreParams score_params_from_dict(const py::dict& params) {
    ScoreParams out;
    out.k_z = get_param<int>(params, "k_z", out.k_z);
    out.k_p = get_param<int>(params, "k_p", out.k_p);
    out.k_m = get_param<int>(params, "k_m", out.k_m);
    out.eps_m = get_param<double>(params, "eps_m", out.eps_m);
    out.e_s = get_param<double>(params, "e_s", out.e_s);
    out.init_min = get_param<double>(params, "init_min", out.init_min);
    out.a_0 = get_param<double>(params, "a_0", out.a_0);
    out.b_0 = get_param<double>(params, "b_0", out.b_0);
    out.w_z = get_param<double>(params, "w_z", out.w_z);
    out.w_p = get_param<double>(params, "w_p", out.w_p);
    out.w_m = get_param<double>(params, "w_m", out.w_m);
    out.w_s = get_param<double>(params, "w_s", out.w_s);
    out.dt = get_param<double>(params, "dt", out.dt);

    if (out.k_z < 1 || out.k_z > 3) {
        throw std::invalid_argument("k_z has to be 1, 2, or 3");
    }
    if (out.k_p < 1 || out.k_p > 3) {
        throw std::invalid_argument("k_p has to be 1, 2, or 3");
    }
    if (out.k_m < 1 || out.k_m > 3) {
        throw std::invalid_argument("k_m has to be 1, 2, or 3");
    }
    const double weights_sum = out.w_z + out.w_m + out.w_p + out.w_s;
    if (weights_sum != 1.0) {
        throw std::invalid_argument("Sum of weighting factors (w_z, w_m, w_p, w_s) must be 1");
    }
    return out;
}

CurveView curve_view_from_array(const py::array_t<double, py::array::forcecast>& array, const char* name) {
    const py::buffer_info info = array.request();
    if (info.ndim != 2) {
        throw std::invalid_argument(std::string(name) + " must be a 2D array");
    }
    if (info.shape[0] <= 0) {
        throw std::invalid_argument(std::string(name) + " must not be empty");
    }
    if (info.shape[1] != 2) {
        throw std::invalid_argument(std::string(name) + " must have shape (n, 2)");
    }

    CurveView view;
    view.data = static_cast<const char*>(info.ptr);
    view.row_stride = info.strides[0];
    view.column_stride = info.strides[1];
    view.n = info.shape[0];
    return view;
}

std::pair<CurveView, CurveView> validate_curves(
    const py::array_t<double, py::array::forcecast>& reference_curve,
    const py::array_t<double, py::array::forcecast>& comparison_curve
) {
    const CurveView reference = curve_view_from_array(reference_curve, "reference_curve");
    const CurveView comparison = curve_view_from_array(comparison_curve, "comparison_curve");
    if (reference.n != comparison.n) {
        throw std::invalid_argument("Curves are not equal in size/dimension");
    }
    return {reference, comparison};
}

ArrayView view_from_array(const py::array_t<double, py::array::forcecast>& array, const char* name) {
    const py::buffer_info info = array.request();
    if (info.ndim != 1) {
        throw std::invalid_argument(std::string(name) + " must be a 1D array");
    }
    if (info.shape[0] <= 0) {
        throw std::invalid_argument(std::string(name) + " must not be empty");
    }

    ArrayView view;
    view.data = static_cast<const char*>(info.ptr);
    view.stride = info.strides[0];
    view.n = info.shape[0];
    return view;
}

ArrayView value_view_from_curve(const CurveView& curve, py::ssize_t start, py::ssize_t length) {
    ArrayView view;
    view.data = curve.data + start * curve.row_stride + curve.column_stride;
    view.stride = curve.row_stride;
    view.n = length;
    return view;
}

std::vector<double> copy_values(const ArrayView& values) {
    std::vector<double> out(static_cast<std::size_t>(values.n));
    for (py::ssize_t idx = 0; idx < values.n; ++idx) {
        out[static_cast<std::size_t>(idx)] = values[idx];
    }
    return out;
}

ArrayView view_from_vector(const std::vector<double>& values) {
    ArrayView view;
    view.data = reinterpret_cast<const char*>(values.data());
    view.stride = static_cast<py::ssize_t>(sizeof(double));
    view.n = static_cast<py::ssize_t>(values.size());
    return view;
}

double correlation_for_shift(
    const CurveView& reference,
    const CurveView& comparison,
    py::ssize_t reference_start,
    py::ssize_t comparison_start,
    py::ssize_t length
) {
    double reference_sum = 0.0;
    double comparison_sum = 0.0;

    for (py::ssize_t idx = 0; idx < length; ++idx) {
        reference_sum += reference.value(reference_start + idx);
        comparison_sum += comparison.value(comparison_start + idx);
    }

    const double n = static_cast<double>(length);
    const double reference_mean = reference_sum / n;
    const double comparison_mean = comparison_sum / n;
    double reference_cov = 0.0;
    double comparison_cov = 0.0;
    double cross_cov = 0.0;

    for (py::ssize_t idx = 0; idx < length; ++idx) {
        const double x = reference.value(reference_start + idx) - reference_mean;
        const double y = comparison.value(comparison_start + idx) - comparison_mean;
        reference_cov += x * x;
        comparison_cov += y * y;
        cross_cov += x * y;
    }

    const double fact = n - 1.0;
    reference_cov /= fact;
    comparison_cov /= fact;
    cross_cov /= fact;

    double correlation = cross_cov / std::sqrt(reference_cov);
    correlation /= std::sqrt(comparison_cov);
    if (correlation > 1.0) {
        correlation = 1.0;
    } else if (correlation < -1.0) {
        correlation = -1.0;
    }
    return correlation;
}

PhaseCache build_phase_cache(const CurveView& reference, const CurveView& comparison) {
    PhaseCache cache;
    const std::size_t size = static_cast<std::size_t>(reference.n + 1);
    cache.reference_sum.assign(size, 0.0);
    cache.comparison_sum.assign(size, 0.0);
    cache.reference_square_sum.assign(size, 0.0);
    cache.comparison_square_sum.assign(size, 0.0);

    for (py::ssize_t idx = 0; idx < reference.n; ++idx) {
        const std::size_t current = static_cast<std::size_t>(idx + 1);
        const std::size_t previous = static_cast<std::size_t>(idx);
        const double x = reference.value(idx);
        const double y = comparison.value(idx);
        cache.reference_sum[current] = cache.reference_sum[previous] + x;
        cache.comparison_sum[current] = cache.comparison_sum[previous] + y;
        cache.reference_square_sum[current] = cache.reference_square_sum[previous] + x * x;
        cache.comparison_square_sum[current] = cache.comparison_square_sum[previous] + y * y;
    }
    return cache;
}

double prefix_range(const std::vector<double>& values, py::ssize_t start, py::ssize_t length) {
    return values[static_cast<std::size_t>(start + length)] - values[static_cast<std::size_t>(start)];
}

double correlation_for_shift(
    const CurveView& reference,
    const CurveView& comparison,
    const PhaseCache& cache,
    py::ssize_t reference_start,
    py::ssize_t comparison_start,
    py::ssize_t length
) {
    if (length < 32) {
        return correlation_for_shift(reference, comparison, reference_start, comparison_start, length);
    }

    const double n = static_cast<double>(length);
    const double reference_sum = prefix_range(cache.reference_sum, reference_start, length);
    const double comparison_sum = prefix_range(cache.comparison_sum, comparison_start, length);
    const double reference_square_sum = prefix_range(cache.reference_square_sum, reference_start, length);
    const double comparison_square_sum = prefix_range(cache.comparison_square_sum, comparison_start, length);

    double product_sum = 0.0;
    for (py::ssize_t idx = 0; idx < length; ++idx) {
        product_sum += reference.value(reference_start + idx) * comparison.value(comparison_start + idx);
    }

    const double numerator = product_sum - (reference_sum * comparison_sum / n);
    const double reference_var = reference_square_sum - (reference_sum * reference_sum / n);
    const double comparison_var = comparison_square_sum - (comparison_sum * comparison_sum / n);
    const double reference_scale = std::max(reference_square_sum, std::abs(reference_sum * reference_sum / n));
    const double comparison_scale = std::max(comparison_square_sum, std::abs(comparison_sum * comparison_sum / n));
    const double reference_tol = std::numeric_limits<double>::epsilon() * std::max(1.0, reference_scale) * 64.0;
    const double comparison_tol = std::numeric_limits<double>::epsilon() * std::max(1.0, comparison_scale) * 64.0;
    if (reference_var <= reference_tol || comparison_var <= comparison_tol) {
        return correlation_for_shift(reference, comparison, reference_start, comparison_start, length);
    }

    double correlation = numerator / std::sqrt(reference_var * comparison_var);
    if (correlation > 1.0) {
        correlation = 1.0;
    } else if (correlation < -1.0) {
        correlation = -1.0;
    }
    return correlation;
}

ShiftResult compute_shift(const CurveView& reference, const CurveView& comparison, const ScoreParams& params) {
    ShiftResult shift;
    shift.length = reference.n;
    shift.max_shift = std::round((1.0 - params.init_min) * 100.0) / 100.0;
    const py::ssize_t window_size = static_cast<py::ssize_t>(
        std::floor(static_cast<double>(comparison.n) * shift.max_shift) + 1.0
    );
    const py::ssize_t bounded_window_size = std::min(window_size, reference.n);
    const PhaseCache cache = build_phase_cache(reference, comparison);

    double ccr_max = correlation_for_shift(reference, comparison, cache, 0, 0, reference.n);
    shift.rho_e = ccr_max;

    for (py::ssize_t idx = 1; idx < bounded_window_size; ++idx) {
        const py::ssize_t length = reference.n - idx;
        const double ccr_left = correlation_for_shift(reference, comparison, cache, 0, idx, length);
        if (ccr_left > ccr_max) {
            ccr_max = ccr_left;
            shift.n_eps = idx;
            shift.reference_start = 0;
            shift.comparison_start = idx;
            shift.length = length;
            shift.rho_e = ccr_left;
        }

        const double ccr_right = correlation_for_shift(reference, comparison, cache, idx, 0, length);
        if (ccr_right > ccr_max) {
            ccr_max = ccr_right;
            shift.n_eps = idx;
            shift.reference_start = idx;
            shift.comparison_start = 0;
            shift.length = length;
            shift.rho_e = ccr_right;
        }
    }

    return shift;
}

double corridor_score(const CurveView& reference, const CurveView& comparison, const ScoreParams& params) {
    double t_norm = 0.0;
    for (py::ssize_t idx = 0; idx < reference.n; ++idx) {
        t_norm = std::max(t_norm, std::abs(reference.value(idx)));
    }

    const double inner_corridor = params.a_0 * t_norm;
    const double outer_corridor = params.b_0 * t_norm;
    double sum = 0.0;
    for (py::ssize_t idx = 0; idx < reference.n; ++idx) {
        const double diff = std::abs(reference.value(idx) - comparison.value(idx));
        double c_i = std::pow(
            (outer_corridor - diff) / (outer_corridor - inner_corridor),
            static_cast<double>(params.k_z)
        );
        if (diff < inner_corridor) {
            c_i = 1.0;
        }
        if (diff > outer_corridor) {
            c_i = 0.0;
        }
        sum += c_i;
    }
    return sum / static_cast<double>(reference.n);
}

double phase_score(const CurveView& reference, const ScoreParams& params, const ShiftResult& shift) {
    const double max_allowable_time_shift_threshold = static_cast<double>(reference.n) * shift.max_shift;
    if (shift.n_eps == 0) {
        return 1.0;
    }
    if (std::abs(static_cast<double>(shift.n_eps)) >= max_allowable_time_shift_threshold) {
        return 0.0;
    }
    return std::pow(
        (max_allowable_time_shift_threshold - std::abs(static_cast<double>(shift.n_eps))) /
            max_allowable_time_shift_threshold,
        static_cast<double>(params.k_p)
    );
}

double magnitude_score(const CurveView& reference, const CurveView& comparison, const ScoreParams& params, const ShiftResult& shift) {
    const ArrayView comparison_values = value_view_from_curve(comparison, shift.comparison_start, shift.length);
    const ArrayView reference_values = value_view_from_curve(reference, shift.reference_start, shift.length);
    double e_mag = 0.0;
    if (comparison_values.stride == static_cast<py::ssize_t>(sizeof(double)) &&
        reference_values.stride == static_cast<py::ssize_t>(sizeof(double))) {
        e_mag = magnitude_ratio_from_views(comparison_values, reference_values, 0.1);
    } else {
        const std::vector<double> contiguous_comparison = copy_values(comparison_values);
        const std::vector<double> contiguous_reference = copy_values(reference_values);
        e_mag = magnitude_ratio_from_views(
            view_from_vector(contiguous_comparison),
            view_from_vector(contiguous_reference),
            0.1
        );
    }

    if (e_mag == 0.0) {
        return 1.0;
    }
    if (e_mag > params.eps_m) {
        return 0.0;
    }
    return std::pow((params.eps_m - e_mag) / params.eps_m, static_cast<double>(params.k_m));
}

void gradient_values(const ArrayView& values, double dt, std::vector<double>& gradient) {
    const py::ssize_t n = values.n;
    gradient.assign(static_cast<std::size_t>(n), 0.0);
    gradient[0] = (values[1] - values[0]) / dt;
    for (py::ssize_t idx = 1; idx < n - 1; ++idx) {
        gradient[static_cast<std::size_t>(idx)] = (values[idx + 1] - values[idx - 1]) / (2.0 * dt);
    }
    gradient[static_cast<std::size_t>(n - 1)] = (values[n - 1] - values[n - 2]) / dt;
}

void smooth_slopes(const std::vector<double>& gradient, std::vector<double>& smoothed) {
    const py::ssize_t n = static_cast<py::ssize_t>(gradient.size());
    smoothed.assign(static_cast<std::size_t>(n), 0.0);

    const py::ssize_t windows[4] = {1, 3, 5, 7};
    for (py::ssize_t idx = 0; idx < 4; ++idx) {
        const py::ssize_t nr = windows[idx];
        double start_sum = 0.0;
        double end_sum = 0.0;
        for (py::ssize_t j = 0; j < nr; ++j) {
            start_sum += gradient[static_cast<std::size_t>(j)];
            end_sum += gradient[static_cast<std::size_t>(n - nr + j)];
        }
        smoothed[static_cast<std::size_t>(idx)] = start_sum / static_cast<double>(nr);
        smoothed[static_cast<std::size_t>(n - idx - 1)] = end_sum / static_cast<double>(nr);
    }

    for (py::ssize_t idx = 4; idx < n - 4; ++idx) {
        double sum = 0.0;
        for (py::ssize_t j = idx - 4; j <= idx + 4; ++j) {
            sum += gradient[static_cast<std::size_t>(j)];
        }
        smoothed[static_cast<std::size_t>(idx)] = sum / 9.0;
    }
}

double slope_score(const CurveView& reference, const CurveView& comparison, const ScoreParams& params, const ShiftResult& shift) {
    if (shift.length < 9) {
        throw std::invalid_argument("Shifted curves must have at least 9 samples for slope rating");
    }

    const ArrayView comparison_values = value_view_from_curve(comparison, shift.comparison_start, shift.length);
    const ArrayView reference_values = value_view_from_curve(reference, shift.reference_start, shift.length);

    std::vector<double> comparison_gradient;
    std::vector<double> reference_gradient;
    std::vector<double> comparison_smoothed;
    std::vector<double> reference_smoothed;
    gradient_values(comparison_values, params.dt, comparison_gradient);
    gradient_values(reference_values, params.dt, reference_gradient);
    smooth_slopes(comparison_gradient, comparison_smoothed);
    smooth_slopes(reference_gradient, reference_smoothed);

    double numerator = 0.0;
    double denominator = 0.0;
    for (py::ssize_t idx = 0; idx < shift.length; ++idx) {
        numerator += std::abs(
            comparison_smoothed[static_cast<std::size_t>(idx)] -
            reference_smoothed[static_cast<std::size_t>(idx)]
        );
        denominator += std::abs(reference_smoothed[static_cast<std::size_t>(idx)]);
    }
    const double e_slope = numerator / denominator;

    if (e_slope <= 0.0) {
        return 1.0;
    }
    if (e_slope >= params.e_s) {
        return 0.0;
    }
    return (params.e_s - e_slope) / params.e_s;
}

ScoreResult score_components_native(const CurveView& reference, const CurveView& comparison, const ScoreParams& params) {
    ScoreResult score;
    score.shift = compute_shift(reference, comparison, params);
    score.z = corridor_score(reference, comparison, params);
    score.ep = phase_score(reference, params, score.shift);
    score.em = magnitude_score(reference, comparison, params, score.shift);
    score.es = slope_score(reference, comparison, params, score.shift);
    score.r = params.w_z * score.z + params.w_p * score.ep + params.w_m * score.em + params.w_s * score.es;
    return score;
}

std::pair<ArrayView, ArrayView> validate_inputs(
    const py::array_t<double, py::array::forcecast>& x,
    const py::array_t<double, py::array::forcecast>& y
) {
    const ArrayView x_view = view_from_array(x, "x");
    const ArrayView y_view = view_from_array(y, "y");
    if (x_view.n != y_view.n) {
        throw std::invalid_argument("ISO DTW expects equal-length arrays");
    }
    if (x_view.n <= 0) {
        throw std::invalid_argument("ISO DTW expects 1D arrays");
    }
    return {x_view, y_view};
}

py::array_t<py::ssize_t> warp_path(
    py::array_t<double, py::array::forcecast> x,
    py::array_t<double, py::array::forcecast> y,
    double window_size
) {
    const auto views = validate_inputs(x, y);
    const auto n = views.first.n;

    DtwState state;
    std::vector<std::pair<py::ssize_t, py::ssize_t>> pairs;
    {
        py::gil_scoped_release release;
        state = compute_directions(views.first, views.second, n, window_size);
        pairs = backtrack_pairs(state);
    }

    py::array_t<py::ssize_t> out({static_cast<py::ssize_t>(pairs.size()), static_cast<py::ssize_t>(2)});
    auto mutable_out = out.mutable_unchecked<2>();
    for (py::ssize_t k = 0; k < static_cast<py::ssize_t>(pairs.size()); ++k) {
        mutable_out(k, 0) = pairs[static_cast<std::size_t>(k)].first;
        mutable_out(k, 1) = pairs[static_cast<std::size_t>(k)].second;
    }
    return out;
}

double magnitude_ratio(
    py::array_t<double, py::array::forcecast> x,
    py::array_t<double, py::array::forcecast> y,
    double window_size
) {
    const auto views = validate_inputs(x, y);

    double ratio = 0.0;
    {
        py::gil_scoped_release release;
        ratio = magnitude_ratio_from_views(views.first, views.second, window_size);
    }

    return ratio;
}

py::dict score_components(
    py::array_t<double, py::array::forcecast> reference_curve,
    py::array_t<double, py::array::forcecast> comparison_curve,
    py::dict params
) {
    const auto views = validate_curves(reference_curve, comparison_curve);
    const ScoreParams score_params = score_params_from_dict(params);

    ScoreResult score;
    {
        py::gil_scoped_release release;
        score = score_components_native(views.first, views.second, score_params);
    }

    py::dict out;
    out["Z"] = score.z;
    out["EP"] = score.ep;
    out["EM"] = score.em;
    out["ES"] = score.es;
    out["R"] = score.r;
    out["n_eps"] = score.shift.n_eps;
    out["rho_e"] = score.shift.rho_e;
    out["reference_start"] = score.shift.reference_start;
    out["comparison_start"] = score.shift.comparison_start;
    out["shift_length"] = score.shift.length;
    return out;
}

py::dict backend_info() {
    py::dict info;
    info["name"] = "local_iso_native";
    info["language"] = "C++17";
    info["window"] = "abs(i-j) < max(1, ceil(window_size*n))";
    info["tie_order"] = "vertical,horizontal,diagonal";
    info["cost"] = "squared";
    return info;
}

}  // namespace

PYBIND11_MODULE(_core, m) {
    m.doc() = "Clean-room native ISO/TS 18571 DTW backend";
    m.def("backend_info", &backend_info);
    m.def("warp_path", &warp_path, py::arg("x"), py::arg("y"), py::arg("window_size"));
    m.def("magnitude_ratio", &magnitude_ratio, py::arg("x"), py::arg("y"), py::arg("window_size"));
    m.def("score_components", &score_components, py::arg("reference_curve"), py::arg("comparison_curve"), py::arg("params") = py::dict());
}
