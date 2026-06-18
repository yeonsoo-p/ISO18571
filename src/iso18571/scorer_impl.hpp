#include "scorer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
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
using iso18571::Diagnostic;
using iso18571::DiagnosticCode;
using iso18571::DiagnosticComponent;
using iso18571::DiagnosticSeverity;
using iso18571::Index;
using iso18571::MagnitudeResult;
using iso18571::PhaseAlignment;
using iso18571::PhaseResult;
using iso18571::ScoreParams;
using iso18571::ScoreResult;
using iso18571::SlopeResult;

constexpr std::uint8_t DIR_NONE       = 0;
constexpr std::uint8_t DIR_VERTICAL   = 1;
constexpr std::uint8_t DIR_HORIZONTAL = 2;
constexpr std::uint8_t DIR_DIAGONAL   = 3;

struct DtwState {
    Index                     n          = 0;
    Index                     radius     = 0;
    Index                     band_width = 0;
    std::vector<std::uint8_t> directions;
};

struct PhaseCache {
    std::vector<double> reference_sum;
    std::vector<double> comparison_sum;
    std::vector<double> reference_square_sum;
    std::vector<double> comparison_square_sum;
};

void append_warning(std::vector<Diagnostic>& diagnostics, DiagnosticComponent component, DiagnosticCode code) {
    diagnostics.push_back({DiagnosticSeverity::Warning, component, code});
}

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
    state.n          = x.n;
    state.radius     = window_radius(x.n, window_size);
    state.band_width = 2 * state.radius - 1;
    state.directions.assign(static_cast<std::size_t>(state.n * state.band_width), DIR_NONE);

    const double        inf = std::numeric_limits<double>::infinity();
    std::vector<double> previous(static_cast<std::size_t>(state.n), inf);
    std::vector<double> current(static_cast<std::size_t>(state.n), inf);

    for (Index i = 0; i < state.n; ++i) {
        const Index j_start          = std::max<Index>(0, i - state.radius + 1);
        const Index j_stop           = std::min<Index>(state.n, i + state.radius);
        const Index direction_offset = j_start - (i - state.radius + 1);
        const Index direction_base   = i * state.band_width + direction_offset;
        const Index previous_start   = i > 0 ? std::max<Index>(0, i - state.radius) : 0;
        const Index previous_stop    = i > 0 ? std::min<Index>(state.n, i + state.radius - 1) : 0;

        Index direction_idx = direction_base;
        for (Index j = j_start; j < j_stop; ++j, ++direction_idx) {
            const double delta       = x.value(i) - y.value(j);
            const double local_cost  = delta * delta;
            double       accumulated = inf;
            std::uint8_t direction   = DIR_NONE;

            if (i == 0 && j == 0) {
                accumulated = local_cost;
            } else {
                double best_previous = inf;

                if (i > 0 && j >= previous_start && j < previous_stop) {
                    best_previous = previous[static_cast<std::size_t>(j)];
                    direction     = DIR_VERTICAL;
                }
                if (j > j_start) {
                    const double candidate = current[static_cast<std::size_t>(j - 1)];
                    if (candidate < best_previous) {
                        best_previous = candidate;
                        direction     = DIR_HORIZONTAL;
                    }
                }
                if (i > 0 && j > 0 && j - 1 >= previous_start && j - 1 < previous_stop) {
                    const double candidate = previous[static_cast<std::size_t>(j - 1)];
                    if (candidate < best_previous) {
                        best_previous = candidate;
                        direction     = DIR_DIAGONAL;
                    }
                }

                if (std::isfinite(best_previous)) {
                    accumulated = local_cost + best_previous;
                } else {
                    direction = DIR_NONE;
                }
            }

            current[static_cast<std::size_t>(j)]                      = accumulated;
            state.directions[static_cast<std::size_t>(direction_idx)] = direction;
        }
        std::swap(previous, current);
    }

    if (!std::isfinite(previous[static_cast<std::size_t>(state.n - 1)])) {
        throw std::runtime_error("No valid ISO DTW path found");
    }
    return state;
}

std::pair<double, double> magnitude_error_from_state(const ArrayView& x, const ArrayView& y, const DtwState& state) {
    double numerator   = 0.0;
    double denominator = 0.0;
    Index  i           = state.n - 1;
    Index  j           = state.n - 1;

    while (true) {
        numerator += std::abs(x.value(i) - y.value(j));
        denominator += std::abs(y.value(j));
        if (i == 0 && j == 0) {
            break;
        }

        const auto direction =
            state.directions[static_cast<std::size_t>(direction_index(i, j, state.radius, state.band_width))];
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
    return {numerator, denominator};
}

std::pair<double, double> magnitude_error_impl(const ArrayView& x, const ArrayView& y, double window_size) {
    const DtwState state = compute_directions_index_incremental(x, y, window_size);
    return magnitude_error_from_state(x, y, state);
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

template<typename Series> PhaseCache build_phase_cache(const Series& reference, const Series& comparison) {
    PhaseCache        cache;
    const std::size_t size = static_cast<std::size_t>(reference.n + 1);
    cache.reference_sum.assign(size, 0.0);
    cache.comparison_sum.assign(size, 0.0);
    cache.reference_square_sum.assign(size, 0.0);
    cache.comparison_square_sum.assign(size, 0.0);

    for (Index idx = 0; idx < reference.n; ++idx) {
        const std::size_t current            = static_cast<std::size_t>(idx + 1);
        const std::size_t previous           = static_cast<std::size_t>(idx);
        const double      x                  = reference.value(idx);
        const double      y                  = comparison.value(idx);
        cache.reference_sum[current]         = cache.reference_sum[previous] + x;
        cache.comparison_sum[current]        = cache.comparison_sum[previous] + y;
        cache.reference_square_sum[current]  = cache.reference_square_sum[previous] + x * x;
        cache.comparison_square_sum[current] = cache.comparison_square_sum[previous] + y * y;
    }
    return cache;
}

double prefix_range(const std::vector<double>& values, Index start, Index length) {
    return values[static_cast<std::size_t>(start + length)] - values[static_cast<std::size_t>(start)];
}

template<typename Series>
bool values_equal_for_shift(const Series& reference, const Series& comparison, Index reference_start,
                            Index comparison_start, Index length) {
    for (Index idx = 0; idx < length; ++idx) {
        if (reference.value(reference_start + idx) != comparison.value(comparison_start + idx)) {
            return false;
        }
    }
    return true;
}

template<typename Series>
double correlation_for_shift(const Series& reference, const Series& comparison, Index reference_start,
                             Index comparison_start, Index length, std::vector<Diagnostic>& diagnostics) {
    double reference_sum  = 0.0;
    double comparison_sum = 0.0;
    for (Index idx = 0; idx < length; ++idx) {
        reference_sum += reference.value(reference_start + idx);
        comparison_sum += comparison.value(comparison_start + idx);
    }

    if (length < 2) {
        append_warning(diagnostics, DiagnosticComponent::Phase, DiagnosticCode::PhaseUndefinedCorrelation);
        return values_equal_for_shift(reference, comparison, reference_start, comparison_start, length) ? 1.0 : 0.0;
    }

    const double n               = static_cast<double>(length);
    const double reference_mean  = reference_sum / n;
    const double comparison_mean = comparison_sum / n;
    double       reference_cov   = 0.0;
    double       comparison_cov  = 0.0;
    double       cross_cov       = 0.0;
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

    if (reference_cov <= 0.0 || comparison_cov <= 0.0) {
        append_warning(diagnostics, DiagnosticComponent::Phase, DiagnosticCode::PhaseUndefinedCorrelation);
        return values_equal_for_shift(reference, comparison, reference_start, comparison_start, length) ? 1.0 : 0.0;
    }

    double correlation = cross_cov / std::sqrt(reference_cov);
    correlation /= std::sqrt(comparison_cov);
    if (correlation > 1.0) {
        correlation = 1.0;
    } else if (correlation < -1.0) {
        correlation = -1.0;
    }
    return correlation;
}

template<typename Series>
double product_sum_for_shift(const Series& reference, const Series& comparison, Index reference_start,
                             Index comparison_start, Index length) {
    double out = 0.0;
    for (Index idx = 0; idx < length; ++idx) {
        out += reference.value(reference_start + idx) * comparison.value(comparison_start + idx);
    }
    return out;
}

double correlation_from_cached_product(const PhaseCache& cache, Index reference_start, Index comparison_start,
                                       Index length, double product_sum) {
    const double n                     = static_cast<double>(length);
    const double reference_sum         = prefix_range(cache.reference_sum, reference_start, length);
    const double comparison_sum        = prefix_range(cache.comparison_sum, comparison_start, length);
    const double reference_square_sum  = prefix_range(cache.reference_square_sum, reference_start, length);
    const double comparison_square_sum = prefix_range(cache.comparison_square_sum, comparison_start, length);

    const double numerator        = product_sum - (reference_sum * comparison_sum / n);
    const double reference_var    = reference_square_sum - (reference_sum * reference_sum / n);
    const double comparison_var   = comparison_square_sum - (comparison_sum * comparison_sum / n);
    const double reference_scale  = std::max(reference_square_sum, std::abs(reference_sum * reference_sum / n));
    const double comparison_scale = std::max(comparison_square_sum, std::abs(comparison_sum * comparison_sum / n));
    const double reference_tol    = std::numeric_limits<double>::epsilon() * std::max(1.0, reference_scale) * 64.0;
    const double comparison_tol   = std::numeric_limits<double>::epsilon() * std::max(1.0, comparison_scale) * 64.0;
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

template<typename Series>
PhaseResult phase_candidate_for_shift(const Series& reference, const Series& comparison, Index reference_start,
                                      Index comparison_start, Index length, Index n_eps, double max_shift) {
    PhaseResult result;
    result.alignment.reference_start  = reference_start;
    result.alignment.comparison_start = comparison_start;
    result.alignment.length           = length;
    result.alignment.n_eps            = n_eps;
    result.alignment.max_shift        = max_shift;
    result.correlation.rho_e =
        correlation_for_shift(reference, comparison, reference_start, comparison_start, length, result.diagnostics);
    return result;
}

PhaseResult phase_candidate_from_correlation(Index reference_start, Index comparison_start, Index length, Index n_eps,
                                             double max_shift, double rho_e) {
    PhaseResult result;
    result.alignment.reference_start  = reference_start;
    result.alignment.comparison_start = comparison_start;
    result.alignment.length           = length;
    result.alignment.n_eps            = n_eps;
    result.alignment.max_shift        = max_shift;
    result.correlation.rho_e          = rho_e;
    return result;
}

template<typename Series>
std::pair<PhaseResult, PhaseResult> dual_phase_candidates_for_shift(const Series& reference, const Series& comparison,
                                                                    const PhaseCache& cache, Index shift, Index length,
                                                                    double max_shift) {
    if (length < 32) {
        return {
            phase_candidate_for_shift(reference, comparison, 0, shift, length, shift, max_shift),
            phase_candidate_for_shift(reference, comparison, shift, 0, length, shift, max_shift),
        };
    }

    const double left_product_sum  = product_sum_for_shift(reference, comparison, 0, shift, length);
    const double right_product_sum = product_sum_for_shift(reference, comparison, shift, 0, length);
    double       left              = correlation_from_cached_product(cache, 0, shift, length, left_product_sum);
    double       right             = correlation_from_cached_product(cache, shift, 0, length, right_product_sum);
    PhaseResult  left_result;
    PhaseResult  right_result;
    if (std::isnan(left)) {
        left_result = phase_candidate_for_shift(reference, comparison, 0, shift, length, shift, max_shift);
    } else {
        left_result = phase_candidate_from_correlation(0, shift, length, shift, max_shift, left);
    }
    if (std::isnan(right)) {
        right_result = phase_candidate_for_shift(reference, comparison, shift, 0, length, shift, max_shift);
    } else {
        right_result = phase_candidate_from_correlation(shift, 0, length, shift, max_shift, right);
    }
    return {left_result, right_result};
}

template<typename Series>
PhaseResult compute_phase_alignment(const Series& reference, const Series& comparison, const ScoreParams& params) {
    const double max_shift = std::round((1.0 - params.init_min) * 100.0) / 100.0;
    PhaseResult  result    = phase_candidate_for_shift(reference, comparison, 0, 0, reference.n, 0, max_shift);
    if (result.correlation.rho_e == 1.0) {
        return result;
    }

    const Index      window_size = static_cast<Index>(std::floor(static_cast<double>(comparison.n) * max_shift) + 1.0);
    const Index      bounded_window_size = std::min(window_size, reference.n);
    const PhaseCache cache               = build_phase_cache(reference, comparison);
    double           ccr_max             = result.correlation.rho_e;

    for (Index idx = 1; idx < bounded_window_size; ++idx) {
        const Index length     = reference.n - idx;
        const auto  candidates = dual_phase_candidates_for_shift(reference, comparison, cache, idx, length, max_shift);
        const PhaseResult left_candidate = candidates.first;
        if (left_candidate.correlation.rho_e > ccr_max) {
            ccr_max = left_candidate.correlation.rho_e;
            result  = left_candidate;
        }

        const PhaseResult right_candidate = candidates.second;
        if (right_candidate.correlation.rho_e > ccr_max) {
            ccr_max = right_candidate.correlation.rho_e;
            result  = right_candidate;
        }
    }

    if (result.alignment.length < 9) {
        result = phase_candidate_for_shift(reference, comparison, 0, 0, reference.n, 0, max_shift);
        append_warning(result.diagnostics, DiagnosticComponent::Phase, DiagnosticCode::PhaseShiftClampedToUnshifted);
    }

    return result;
}

double corridor_score(const CurveView& reference, const CurveView& comparison, const ScoreParams& params) {
    double t_norm = 0.0;
    for (Index idx = 0; idx < reference.n; ++idx) {
        t_norm = std::max(t_norm, std::abs(reference.value(idx)));
    }

    if (t_norm == 0.0) {
        double sum = 0.0;
        for (Index idx = 0; idx < reference.n; ++idx) {
            if (reference.value(idx) == comparison.value(idx)) {
                sum += 1.0;
            }
        }
        return sum / static_cast<double>(reference.n);
    }

    const double inner_corridor = params.a_0 * t_norm;
    const double outer_corridor = params.b_0 * t_norm;
    double       sum            = 0.0;
    for (Index idx = 0; idx < reference.n; ++idx) {
        const double diff = std::abs(reference.value(idx) - comparison.value(idx));
        double       c_i =
            std::pow((outer_corridor - diff) / (outer_corridor - inner_corridor), static_cast<double>(params.k_z));
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

double phase_score(const CurveView& reference, const ScoreParams& params, const PhaseAlignment& alignment) {
    const double max_allowable_time_shift_threshold = static_cast<double>(reference.n) * alignment.max_shift;
    if (alignment.n_eps == 0) {
        return 1.0;
    }
    if (std::abs(static_cast<double>(alignment.n_eps)) >= max_allowable_time_shift_threshold) {
        return 0.0;
    }
    return std::pow((max_allowable_time_shift_threshold - std::abs(static_cast<double>(alignment.n_eps))) /
                        max_allowable_time_shift_threshold,
                    static_cast<double>(params.k_p));
}

MagnitudeResult magnitude_score_from_values(const ArrayView& reference_values, const ArrayView& comparison_values,
                                            const ScoreParams& params) {
    const auto [numerator, denominator] = magnitude_error_impl(comparison_values, reference_values, 0.1);
    if (denominator == 0.0) {
        MagnitudeResult result;
        result.score = numerator == 0.0 ? 1.0 : 0.0;
        append_warning(result.diagnostics, DiagnosticComponent::Magnitude,
                       DiagnosticCode::MagnitudeZeroReferenceDenominator);
        return result;
    }

    const double e_mag = numerator / denominator;
    if (e_mag == 0.0) {
        return {1.0, {}};
    }
    if (e_mag > params.eps_m) {
        return {0.0, {}};
    }
    return {
        std::pow((params.eps_m - e_mag) / params.eps_m, static_cast<double>(params.k_m)),
        {},
    };
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
        const Index nr         = windows[idx];
        double      sum        = 0.0;
        for (Index j = 0; j < nr; ++j) {
            sum += gradient[static_cast<std::size_t>(j)];
        }
        return sum / static_cast<double>(nr);
    }
    if (idx >= n - 4) {
        const Index edge_idx   = n - idx - 1;
        const Index windows[4] = {1, 3, 5, 7};
        const Index nr         = windows[edge_idx];
        double      sum        = 0.0;
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

SlopeResult fused_slope_score_from_values(const ArrayView& reference_values, const ArrayView& comparison_values,
                                          const ScoreParams& params) {
    if (reference_values.n < 9) {
        throw std::invalid_argument("Shifted curves must have at least 9 samples for slope rating");
    }

    std::vector<double> comparison_gradient;
    std::vector<double> reference_gradient;
    gradient_values(comparison_values, params.dt, comparison_gradient);
    gradient_values(reference_values, params.dt, reference_gradient);

    double numerator   = 0.0;
    double denominator = 0.0;
    for (Index idx = 0; idx < reference_values.n; ++idx) {
        const double comparison_smoothed = smoothed_slope_at(comparison_gradient, idx);
        const double reference_smoothed  = smoothed_slope_at(reference_gradient, idx);
        numerator += std::abs(comparison_smoothed - reference_smoothed);
        denominator += std::abs(reference_smoothed);
    }

    if (denominator == 0.0) {
        SlopeResult result;
        result.score = numerator == 0.0 ? 1.0 : 0.0;
        append_warning(result.diagnostics, DiagnosticComponent::Slope, DiagnosticCode::SlopeZeroReferenceDenominator);
        return result;
    }

    const double e_slope = numerator / denominator;
    if (e_slope <= 0.0) {
        return {1.0, {}};
    }
    if (e_slope >= params.e_s) {
        return {0.0, {}};
    }
    return {(params.e_s - e_slope) / params.e_s, {}};
}

ScoreResult score_components_impl(const CurveView& reference, const CurveView& comparison, const ScoreParams& params) {
    ScoreResult result;
    result.phase          = compute_phase_alignment(reference, comparison, params);
    result.corridor.score = corridor_score(reference, comparison, params);
    result.phase.score    = phase_score(reference, params, result.phase.alignment);

    const ArrayView comparison_values =
        value_view_from_curve(comparison, result.phase.alignment.comparison_start, result.phase.alignment.length);
    const ArrayView reference_values =
        value_view_from_curve(reference, result.phase.alignment.reference_start, result.phase.alignment.length);
    const std::vector<double> contiguous_comparison      = copy_values(comparison_values);
    const std::vector<double> contiguous_reference       = copy_values(reference_values);
    const ArrayView           contiguous_comparison_view = view_from_vector(contiguous_comparison);
    const ArrayView           contiguous_reference_view  = view_from_vector(contiguous_reference);

    result.magnitude = magnitude_score_from_values(contiguous_reference_view, contiguous_comparison_view, params);
    result.slope     = fused_slope_score_from_values(contiguous_reference_view, contiguous_comparison_view, params);
    result.overall   = params.w_z * result.corridor.score + params.w_p * result.phase.score +
                       params.w_m * result.magnitude.score + params.w_s * result.slope.score;
    return result;
}

} // namespace

namespace iso18571 {

ScoreResult ISO18571_VARIANT(score_components)(const CurveView& reference, const CurveView& comparison,
                                               const ScoreParams& params) {
    return score_components_impl(reference, comparison, params);
}

} // namespace iso18571

#undef ISO18571_VARIANT
#undef ISO18571_PASTE
#undef ISO18571_PASTE_INNER
