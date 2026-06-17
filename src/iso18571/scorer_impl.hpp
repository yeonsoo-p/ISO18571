#include "scorer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

#ifndef ISO18571_IMPL_SUFFIX
#error "ISO18571_IMPL_SUFFIX must be defined before including scorer_impl.hpp"
#endif

#define ISO18571_PASTE_INNER(a, b) a##b
#define ISO18571_PASTE(a, b) ISO18571_PASTE_INNER(a, b)
#define ISO18571_VARIANT(name) ISO18571_PASTE(name, ISO18571_IMPL_SUFFIX)

namespace {

using iso18571::ArrayView;
using iso18571::CurveView;
using iso18571::Index;
using iso18571::ScoreParams;
using iso18571::ScoreResult;
using iso18571::ShiftResult;

constexpr std::uint8_t DIR_NONE = 0;
constexpr std::uint8_t DIR_VERTICAL = 1;
constexpr std::uint8_t DIR_HORIZONTAL = 2;
constexpr std::uint8_t DIR_DIAGONAL = 3;

struct DtwState {
    Index n = 0;
    Index radius = 0;
    Index band_width = 0;
    std::vector<std::uint8_t> directions;
};

struct PhaseCache {
    std::vector<double> reference_sum;
    std::vector<double> comparison_sum;
    std::vector<double> reference_square_sum;
    std::vector<double> comparison_square_sum;
};

Index window_radius(Index n, double window_size) {
    if (n <= 0) {
        throw std::invalid_argument("DTW input arrays must not be empty");
    }
    if (!std::isfinite(window_size) || window_size < 0.0) {
        throw std::invalid_argument("DTW window_size must be a finite non-negative value");
    }
    if (window_size >= 1.0) {
        return n;
    }
    const auto raw = static_cast<Index>(std::ceil(window_size * static_cast<double>(n)));
    return std::min<Index>(n, std::max<Index>(1, raw));
}

Index direction_index(Index i, Index j, Index radius, Index band_width) {
    const Index row_start = i - radius + 1;
    return i * band_width + (j - row_start);
}

DtwState compute_directions_index_incremental(const ArrayView& x, const ArrayView& y, double window_size) {
    DtwState state;
    state.n = x.n;
    state.radius = window_radius(x.n, window_size);
    state.band_width = 2 * state.radius - 1;
    state.directions.assign(static_cast<std::size_t>(state.n * state.band_width), DIR_NONE);

    const double inf = std::numeric_limits<double>::infinity();
    std::vector<double> previous(static_cast<std::size_t>(state.n), inf);
    std::vector<double> current(static_cast<std::size_t>(state.n), inf);

    for (Index i = 0; i < state.n; ++i) {
        const Index j_start = std::max<Index>(0, i - state.radius + 1);
        const Index j_stop = std::min<Index>(state.n, i + state.radius);
        const Index direction_offset = j_start - (i - state.radius + 1);
        const Index direction_base = i * state.band_width + direction_offset;
        const Index previous_start = i > 0 ? std::max<Index>(0, i - state.radius) : 0;
        const Index previous_stop = i > 0 ? std::min<Index>(state.n, i + state.radius - 1) : 0;

        Index direction_idx = direction_base;
        for (Index j = j_start; j < j_stop; ++j, ++direction_idx) {
            const double delta = x.value(i) - y.value(j);
            const double local_cost = delta * delta;
            double accumulated = inf;
            std::uint8_t direction = DIR_NONE;

            if (i == 0 && j == 0) {
                accumulated = local_cost;
            } else {
                double best_previous = inf;

                if (i > 0 && j >= previous_start && j < previous_stop) {
                    best_previous = previous[static_cast<std::size_t>(j)];
                    direction = DIR_VERTICAL;
                }
                if (j > j_start) {
                    const double candidate = current[static_cast<std::size_t>(j - 1)];
                    if (candidate < best_previous) {
                        best_previous = candidate;
                        direction = DIR_HORIZONTAL;
                    }
                }
                if (i > 0 && j > 0 && j - 1 >= previous_start && j - 1 < previous_stop) {
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
            state.directions[static_cast<std::size_t>(direction_idx)] = direction;
        }
        std::swap(previous, current);
    }

    if (!std::isfinite(previous[static_cast<std::size_t>(state.n - 1)])) {
        throw std::runtime_error("No valid ISO DTW path found");
    }
    return state;
}

double magnitude_ratio_from_state(const ArrayView& x, const ArrayView& y, const DtwState& state) {
    double numerator = 0.0;
    double denominator = 0.0;
    Index i = state.n - 1;
    Index j = state.n - 1;

    while (true) {
        numerator += std::abs(x.value(i) - y.value(j));
        denominator += std::abs(y.value(j));
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

double magnitude_ratio_impl(const ArrayView& x, const ArrayView& y, double window_size) {
    const DtwState state = compute_directions_index_incremental(x, y, window_size);
    return magnitude_ratio_from_state(x, y, state);
}

std::vector<double> copy_values(const ArrayView& values) {
    std::vector<double> out(static_cast<std::size_t>(values.n));
    for (Index idx = 0; idx < values.n; ++idx) {
        out[static_cast<std::size_t>(idx)] = values.value(idx);
    }
    return out;
}

ArrayView view_from_vector(const std::vector<double>& values) {
    return {
        reinterpret_cast<const char*>(values.data()),
        static_cast<Index>(sizeof(double)),
        static_cast<Index>(values.size()),
    };
}

ArrayView value_view_from_curve(const CurveView& curve, Index start, Index length) {
    return {curve.data + start * curve.row_stride + curve.column_stride, curve.row_stride, length};
}

template <typename Series>
PhaseCache build_phase_cache(const Series& reference, const Series& comparison) {
    PhaseCache cache;
    const std::size_t size = static_cast<std::size_t>(reference.n + 1);
    cache.reference_sum.assign(size, 0.0);
    cache.comparison_sum.assign(size, 0.0);
    cache.reference_square_sum.assign(size, 0.0);
    cache.comparison_square_sum.assign(size, 0.0);

    for (Index idx = 0; idx < reference.n; ++idx) {
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

double prefix_range(const std::vector<double>& values, Index start, Index length) {
    return values[static_cast<std::size_t>(start + length)] - values[static_cast<std::size_t>(start)];
}

template <typename Series>
double correlation_for_shift(
    const Series& reference,
    const Series& comparison,
    Index reference_start,
    Index comparison_start,
    Index length
) {
    double reference_sum = 0.0;
    double comparison_sum = 0.0;
    for (Index idx = 0; idx < length; ++idx) {
        reference_sum += reference.value(reference_start + idx);
        comparison_sum += comparison.value(comparison_start + idx);
    }

    const double n = static_cast<double>(length);
    const double reference_mean = reference_sum / n;
    const double comparison_mean = comparison_sum / n;
    double reference_cov = 0.0;
    double comparison_cov = 0.0;
    double cross_cov = 0.0;
    for (Index idx = 0; idx < length; ++idx) {
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

template <typename Series>
double product_sum_for_shift(
    const Series& reference,
    const Series& comparison,
    Index reference_start,
    Index comparison_start,
    Index length
) {
    double out = 0.0;
    for (Index idx = 0; idx < length; ++idx) {
        out += reference.value(reference_start + idx) * comparison.value(comparison_start + idx);
    }
    return out;
}

double correlation_from_cached_product(
    const PhaseCache& cache,
    Index reference_start,
    Index comparison_start,
    Index length,
    double product_sum
) {
    const double n = static_cast<double>(length);
    const double reference_sum = prefix_range(cache.reference_sum, reference_start, length);
    const double comparison_sum = prefix_range(cache.comparison_sum, comparison_start, length);
    const double reference_square_sum = prefix_range(cache.reference_square_sum, reference_start, length);
    const double comparison_square_sum = prefix_range(cache.comparison_square_sum, comparison_start, length);

    const double numerator = product_sum - (reference_sum * comparison_sum / n);
    const double reference_var = reference_square_sum - (reference_sum * reference_sum / n);
    const double comparison_var = comparison_square_sum - (comparison_sum * comparison_sum / n);
    const double reference_scale = std::max(reference_square_sum, std::abs(reference_sum * reference_sum / n));
    const double comparison_scale = std::max(comparison_square_sum, std::abs(comparison_sum * comparison_sum / n));
    const double reference_tol = std::numeric_limits<double>::epsilon() * std::max(1.0, reference_scale) * 64.0;
    const double comparison_tol = std::numeric_limits<double>::epsilon() * std::max(1.0, comparison_scale) * 64.0;
    if (reference_var <= reference_tol || comparison_var <= comparison_tol) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    double correlation = numerator / std::sqrt(reference_var * comparison_var);
    if (correlation > 1.0) {
        correlation = 1.0;
    } else if (correlation < -1.0) {
        correlation = -1.0;
    }
    return correlation;
}

template <typename Series>
std::pair<double, double> dual_correlations_for_shift(
    const Series& reference,
    const Series& comparison,
    const PhaseCache& cache,
    Index shift,
    Index length
) {
    if (length < 32) {
        return {
            correlation_for_shift(reference, comparison, 0, shift, length),
            correlation_for_shift(reference, comparison, shift, 0, length),
        };
    }

    const double left_product_sum = product_sum_for_shift(reference, comparison, 0, shift, length);
    const double right_product_sum = product_sum_for_shift(reference, comparison, shift, 0, length);
    double left = correlation_from_cached_product(cache, 0, shift, length, left_product_sum);
    double right = correlation_from_cached_product(cache, shift, 0, length, right_product_sum);
    if (std::isnan(left)) {
        left = correlation_for_shift(reference, comparison, 0, shift, length);
    }
    if (std::isnan(right)) {
        right = correlation_for_shift(reference, comparison, shift, 0, length);
    }
    return {left, right};
}

template <typename Series>
ShiftResult compute_shift_dual_product(const Series& reference, const Series& comparison, const ScoreParams& params) {
    ShiftResult shift;
    shift.length = reference.n;
    shift.max_shift = std::round((1.0 - params.init_min) * 100.0) / 100.0;
    const Index window_size = static_cast<Index>(std::floor(static_cast<double>(comparison.n) * shift.max_shift) + 1.0);
    const Index bounded_window_size = std::min(window_size, reference.n);
    const PhaseCache cache = build_phase_cache(reference, comparison);

    double ccr_max = correlation_for_shift(reference, comparison, 0, 0, reference.n);
    shift.rho_e = ccr_max;

    for (Index idx = 1; idx < bounded_window_size; ++idx) {
        const Index length = reference.n - idx;
        const auto correlations = dual_correlations_for_shift(reference, comparison, cache, idx, length);
        const double ccr_left = correlations.first;
        if (ccr_left > ccr_max) {
            ccr_max = ccr_left;
            shift.n_eps = idx;
            shift.reference_start = 0;
            shift.comparison_start = idx;
            shift.length = length;
            shift.rho_e = ccr_left;
        }

        const double ccr_right = correlations.second;
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
    for (Index idx = 0; idx < reference.n; ++idx) {
        t_norm = std::max(t_norm, std::abs(reference.value(idx)));
    }

    const double inner_corridor = params.a_0 * t_norm;
    const double outer_corridor = params.b_0 * t_norm;
    double sum = 0.0;
    for (Index idx = 0; idx < reference.n; ++idx) {
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

double magnitude_score_from_values(
    const ArrayView& reference_values,
    const ArrayView& comparison_values,
    const ScoreParams& params
) {
    const double e_mag = magnitude_ratio_impl(comparison_values, reference_values, 0.1);
    if (e_mag == 0.0) {
        return 1.0;
    }
    if (e_mag > params.eps_m) {
        return 0.0;
    }
    return std::pow((params.eps_m - e_mag) / params.eps_m, static_cast<double>(params.k_m));
}

void gradient_values(const ArrayView& values, double dt, std::vector<double>& gradient) {
    const Index n = values.n;
    gradient.assign(static_cast<std::size_t>(n), 0.0);
    gradient[0] = (values.value(1) - values.value(0)) / dt;
    for (Index idx = 1; idx < n - 1; ++idx) {
        gradient[static_cast<std::size_t>(idx)] = (values.value(idx + 1) - values.value(idx - 1)) / (2.0 * dt);
    }
    gradient[static_cast<std::size_t>(n - 1)] = (values.value(n - 1) - values.value(n - 2)) / dt;
}

double smoothed_slope_at(const std::vector<double>& gradient, Index idx) {
    const Index n = static_cast<Index>(gradient.size());
    if (idx < 4) {
        const Index windows[4] = {1, 3, 5, 7};
        const Index nr = windows[idx];
        double sum = 0.0;
        for (Index j = 0; j < nr; ++j) {
            sum += gradient[static_cast<std::size_t>(j)];
        }
        return sum / static_cast<double>(nr);
    }
    if (idx >= n - 4) {
        const Index edge_idx = n - idx - 1;
        const Index windows[4] = {1, 3, 5, 7};
        const Index nr = windows[edge_idx];
        double sum = 0.0;
        for (Index j = 0; j < nr; ++j) {
            sum += gradient[static_cast<std::size_t>(n - nr + j)];
        }
        return sum / static_cast<double>(nr);
    }

    double sum = 0.0;
    for (Index j = idx - 4; j <= idx + 4; ++j) {
        sum += gradient[static_cast<std::size_t>(j)];
    }
    return sum / 9.0;
}

double fused_slope_score_from_values(
    const ArrayView& reference_values,
    const ArrayView& comparison_values,
    const ScoreParams& params
) {
    if (reference_values.n < 9) {
        throw std::invalid_argument("Shifted curves must have at least 9 samples for slope rating");
    }

    std::vector<double> comparison_gradient;
    std::vector<double> reference_gradient;
    gradient_values(comparison_values, params.dt, comparison_gradient);
    gradient_values(reference_values, params.dt, reference_gradient);

    double numerator = 0.0;
    double denominator = 0.0;
    for (Index idx = 0; idx < reference_values.n; ++idx) {
        const double comparison_smoothed = smoothed_slope_at(comparison_gradient, idx);
        const double reference_smoothed = smoothed_slope_at(reference_gradient, idx);
        numerator += std::abs(comparison_smoothed - reference_smoothed);
        denominator += std::abs(reference_smoothed);
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

ScoreResult score_components_impl(const CurveView& reference, const CurveView& comparison, const ScoreParams& params) {
    ScoreResult score;
    score.shift = compute_shift_dual_product(reference, comparison, params);
    score.z = corridor_score(reference, comparison, params);
    score.ep = phase_score(reference, params, score.shift);

    const ArrayView comparison_values = value_view_from_curve(
        comparison,
        score.shift.comparison_start,
        score.shift.length
    );
    const ArrayView reference_values = value_view_from_curve(reference, score.shift.reference_start, score.shift.length);
    const std::vector<double> contiguous_comparison = copy_values(comparison_values);
    const std::vector<double> contiguous_reference = copy_values(reference_values);
    const ArrayView contiguous_comparison_view = view_from_vector(contiguous_comparison);
    const ArrayView contiguous_reference_view = view_from_vector(contiguous_reference);

    score.em = magnitude_score_from_values(contiguous_reference_view, contiguous_comparison_view, params);
    score.es = fused_slope_score_from_values(contiguous_reference_view, contiguous_comparison_view, params);
    score.r = params.w_z * score.z + params.w_p * score.ep + params.w_m * score.em + params.w_s * score.es;
    return score;
}

}  // namespace

namespace iso18571 {

ScoreResult ISO18571_VARIANT(score_components)(
    const CurveView& reference,
    const CurveView& comparison,
    const ScoreParams& params
) {
    return score_components_impl(reference, comparison, params);
}

}  // namespace iso18571

#undef ISO18571_VARIANT
#undef ISO18571_PASTE
#undef ISO18571_PASTE_INNER
