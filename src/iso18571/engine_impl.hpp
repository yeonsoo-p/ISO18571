#include "engine.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <fft.hpp>

#ifndef ISO18571_IMPL_SUFFIX
#error "ISO18571_IMPL_SUFFIX must be defined before including engine_impl.hpp"
#endif

#define ISO18571_PASTE_INNER(a, b) a##b
#define ISO18571_PASTE(a, b) ISO18571_PASTE_INNER(a, b)
#define ISO18571_VARIANT(name) ISO18571_PASTE(name, ISO18571_IMPL_SUFFIX)

namespace {

using iso18571::Diagnostic;
using iso18571::DiagnosticCode;
using iso18571::DiagnosticComponent;
using iso18571::DiagnosticSeverity;
using iso18571::DoubleSpan;
using iso18571::Index;
using iso18571::MagnitudeResult;
using iso18571::PhaseAlignment;
using iso18571::PhaseResult;
using iso18571::ScoreParams;
using iso18571::ScoreResult;
using iso18571::SlopeResult;

constexpr double CORRELATION_TIE_TOLERANCE = 1.0e-12;
constexpr double CORRELATION_REFINE_MARGIN = 1.0e-9;

std::size_t offset (Index index) { return static_cast<std::size_t>(index); }

Index span_size (DoubleSpan values) { return static_cast<Index>(values.size()); }

double value_at (DoubleSpan values, Index index) { return values[offset(index)]; }

struct PhaseCache {
    std::vector<double> reference_sum;
    std::vector<double> comparison_sum;
    std::vector<double> reference_square_sum;
    std::vector<double> comparison_square_sum;
};

struct PhaseProductSums {
    std::vector<double> products;
};

void append_warning (std::vector<Diagnostic>& diagnostics, DiagnosticComponent component, DiagnosticCode code) {
    diagnostics.push_back({DiagnosticSeverity::Warning, component, code});
}

Index window_radius (Index n, double window_size) {
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

std::pair<double, double> magnitude_error_from_dtw (DoubleSpan x, DoubleSpan y, double window_size) {
    const Index  n      = span_size(x);
    const Index  radius = window_radius(n, window_size);
    const double inf    = std::numeric_limits<double>::infinity();

    std::vector<double> previous_cost(static_cast<std::size_t>(n), inf);
    std::vector<double> current_cost(static_cast<std::size_t>(n), inf);
    std::vector<double> previous_numerator(static_cast<std::size_t>(n), 0.0);
    std::vector<double> current_numerator(static_cast<std::size_t>(n), 0.0);
    std::vector<double> previous_denominator(static_cast<std::size_t>(n), 0.0);
    std::vector<double> current_denominator(static_cast<std::size_t>(n), 0.0);

    for (Index i = 0; i < n; ++i) {
        const Index previous_start = i > 0 ? std::max<Index>(0, i - radius) : 0;
        const Index previous_stop  = i > 0 ? std::min<Index>(n, i + radius - 1) : 0;
        const Index j_start        = std::max<Index>(0, i - radius + 1);
        const Index j_stop         = std::min<Index>(n, i + radius);

        for (Index j = j_start; j < j_stop; ++j) {
            const double delta             = value_at(x, i) - value_at(y, j);
            const double local_cost        = delta * delta;
            const double local_numerator   = std::abs(delta);
            const double local_denominator = std::abs(value_at(y, j));
            double       accumulated       = inf;
            double       numerator         = 0.0;
            double       denominator       = 0.0;

            if (i == 0 && j == 0) {
                accumulated = local_cost;
                numerator   = local_numerator;
                denominator = local_denominator;
            } else {
                double best_previous      = inf;
                double best_numerator     = 0.0;
                double best_denominator   = 0.0;
                auto   select_predecessor = [&best_previous, &best_numerator, &best_denominator] (
                                                double cost, double candidate_numerator, double candidate_denominator) {
                    best_previous    = cost;
                    best_numerator   = candidate_numerator;
                    best_denominator = candidate_denominator;
                };

                if (i > 0 && j >= previous_start && j < previous_stop) {
                    const std::size_t index = static_cast<std::size_t>(j);
                    select_predecessor(previous_cost[index], previous_numerator[index], previous_denominator[index]);
                }
                if (j > j_start) {
                    const std::size_t index     = static_cast<std::size_t>(j - 1);
                    const double      candidate = current_cost[index];
                    if (candidate < best_previous) {
                        select_predecessor(candidate, current_numerator[index], current_denominator[index]);
                    }
                }
                if (i > 0 && j > 0 && j - 1 >= previous_start && j - 1 < previous_stop) {
                    const std::size_t index     = static_cast<std::size_t>(j - 1);
                    const double      candidate = previous_cost[index];
                    if (candidate < best_previous) {
                        select_predecessor(candidate, previous_numerator[index], previous_denominator[index]);
                    }
                }

                if (std::isfinite(best_previous)) {
                    accumulated = local_cost + best_previous;
                    numerator   = local_numerator + best_numerator;
                    denominator = local_denominator + best_denominator;
                }
            }

            const std::size_t index    = static_cast<std::size_t>(j);
            current_cost[index]        = accumulated;
            current_numerator[index]   = numerator;
            current_denominator[index] = denominator;
        }

        std::swap(previous_cost, current_cost);
        std::swap(previous_numerator, current_numerator);
        std::swap(previous_denominator, current_denominator);
    }

    const std::size_t final_index = static_cast<std::size_t>(n - 1);
    if (!std::isfinite(previous_cost[final_index])) {
        throw std::runtime_error("No valid ISO DTW path found");
    }
    return {previous_numerator[final_index], previous_denominator[final_index]};
}

PhaseCache build_phase_cache (DoubleSpan reference, DoubleSpan comparison) {
    PhaseCache        cache;
    const Index       n    = span_size(reference);
    const std::size_t size = static_cast<std::size_t>(n + 1);
    cache.reference_sum.assign(size, 0.0);
    cache.comparison_sum.assign(size, 0.0);
    cache.reference_square_sum.assign(size, 0.0);
    cache.comparison_square_sum.assign(size, 0.0);

    for (Index idx = 0; idx < n; ++idx) {
        const std::size_t current            = static_cast<std::size_t>(idx + 1);
        const std::size_t previous           = static_cast<std::size_t>(idx);
        const double      x                  = value_at(reference, idx);
        const double      y                  = value_at(comparison, idx);
        cache.reference_sum[current]         = cache.reference_sum[previous] + x;
        cache.comparison_sum[current]        = cache.comparison_sum[previous] + y;
        cache.reference_square_sum[current]  = cache.reference_square_sum[previous] + x * x;
        cache.comparison_square_sum[current] = cache.comparison_square_sum[previous] + y * y;
    }
    return cache;
}

double prefix_range (const std::vector<double>& values, Index start, Index length) {
    return values[static_cast<std::size_t>(start + length)] - values[static_cast<std::size_t>(start)];
}

bool values_equal_for_shift (DoubleSpan reference, DoubleSpan comparison, Index reference_start, Index comparison_start,
                             Index length) {
    for (Index idx = 0; idx < length; ++idx) {
        if (value_at(reference, reference_start + idx) != value_at(comparison, comparison_start + idx)) {
            return false;
        }
    }
    return true;
}

double correlation_for_shift (DoubleSpan reference, DoubleSpan comparison, Index reference_start,
                              Index comparison_start, Index length, std::vector<Diagnostic>& diagnostics) {
    double reference_sum  = 0.0;
    double comparison_sum = 0.0;
    for (Index idx = 0; idx < length; ++idx) {
        reference_sum += value_at(reference, reference_start + idx);
        comparison_sum += value_at(comparison, comparison_start + idx);
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
        const double x = value_at(reference, reference_start + idx) - reference_mean;
        const double y = value_at(comparison, comparison_start + idx) - comparison_mean;
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

double product_sum_for_shift (DoubleSpan reference, DoubleSpan comparison, Index reference_start,
                              Index comparison_start, Index length) {
    double out = 0.0;
    for (Index idx = 0; idx < length; ++idx) {
        out += value_at(reference, reference_start + idx) * value_at(comparison, comparison_start + idx);
    }
    return out;
}

std::size_t next_power_of_two (std::size_t value) {
    std::size_t out = 1;
    while (out < value) {
        out <<= 1U;
    }
    return out;
}

PhaseProductSums fft_product_sums (DoubleSpan reference, DoubleSpan comparison) {
    const auto        n         = static_cast<std::size_t>(span_size(reference));
    const std::size_t conv_size = 2U * n - 1U;
    const std::size_t fft_size  = next_power_of_two(conv_size);

    std::vector<std::complex<double>> reference_fft(fft_size);
    std::vector<std::complex<double>> comparison_fft(fft_size);
    for (std::size_t idx = 0; idx < n; ++idx) {
        reference_fft[idx]  = {value_at(reference, static_cast<Index>(idx)), 0.0};
        comparison_fft[idx] = {value_at(comparison, static_cast<Index>(n - idx - 1U)), 0.0};
    }

    const fft::shape_t  shape {fft_size};
    const fft::stride_t stride {static_cast<std::ptrdiff_t>(sizeof(std::complex<double>))};
    const fft::shape_t  axes {0};
    fft::c2c(shape, stride, stride, axes, fft::FORWARD, reference_fft.data(), reference_fft.data(), 1.0);
    fft::c2c(shape, stride, stride, axes, fft::FORWARD, comparison_fft.data(), comparison_fft.data(), 1.0);
    for (std::size_t idx = 0; idx < fft_size; ++idx) {
        reference_fft[idx] *= comparison_fft[idx];
    }
    fft::c2c(shape, stride, stride, axes, fft::BACKWARD, reference_fft.data(), reference_fft.data(),
             1.0 / static_cast<double>(fft_size));

    PhaseProductSums sums;
    sums.products.assign(conv_size, 0.0);
    for (std::size_t idx = 0; idx < conv_size; ++idx) {
        sums.products[idx] = reference_fft[idx].real();
    }
    return sums;
}

double product_sum_from_fft (const PhaseProductSums& sums, Index n, Index reference_start, Index comparison_start) {
    const Index lag = reference_start - comparison_start;
    return sums.products[static_cast<std::size_t>(n - 1 + lag)];
}

double correlation_from_cached_product (const PhaseCache& cache, Index reference_start, Index comparison_start,
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

PhaseResult phase_candidate_for_shift (DoubleSpan reference, DoubleSpan comparison, Index reference_start,
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

PhaseResult phase_candidate_from_correlation (Index reference_start, Index comparison_start, Index length, Index n_eps,
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

PhaseResult phase_candidate_from_fft_product (DoubleSpan reference, DoubleSpan comparison, const PhaseCache& cache,
                                              const PhaseProductSums& sums, Index n, Index reference_start,
                                              Index comparison_start, Index length, Index n_eps, double max_shift) {
    if (length < 32) {
        return phase_candidate_for_shift(reference, comparison, reference_start, comparison_start, length, n_eps,
                                         max_shift);
    }
    const double product_sum = product_sum_from_fft(sums, n, reference_start, comparison_start);
    const double rho_e = correlation_from_cached_product(cache, reference_start, comparison_start, length, product_sum);
    if (std::isnan(rho_e)) {
        return phase_candidate_for_shift(reference, comparison, reference_start, comparison_start, length, n_eps,
                                         max_shift);
    }
    return phase_candidate_from_correlation(reference_start, comparison_start, length, n_eps, max_shift, rho_e);
}

void select_phase_candidate (PhaseResult& result, double& ccr_max, const PhaseResult& candidate) {
    if (candidate.correlation.rho_e > ccr_max + CORRELATION_TIE_TOLERANCE) {
        ccr_max = candidate.correlation.rho_e;
        result  = candidate;
    }
}

PhaseResult refine_fft_phase_result (DoubleSpan reference, DoubleSpan comparison, const PhaseCache& cache,
                                     const PhaseProductSums& sums, Index bounded_window_size, double max_shift,
                                     double fft_ccr_max) {
    PhaseResult refined = phase_candidate_for_shift(reference, comparison, 0, 0, span_size(reference), 0, max_shift);
    double      refined_ccr = refined.correlation.rho_e;

    for (Index idx = 1; idx < bounded_window_size; ++idx) {
        const Index length = span_size(reference) - idx;
        if (length < 32) {
            select_phase_candidate(refined, refined_ccr,
                                   phase_candidate_for_shift(reference, comparison, 0, idx, length, idx, max_shift));
            select_phase_candidate(refined, refined_ccr,
                                   phase_candidate_for_shift(reference, comparison, idx, 0, length, idx, max_shift));
            continue;
        }

        const double left_product_sum = product_sum_from_fft(sums, span_size(reference), 0, idx);
        const double left             = correlation_from_cached_product(cache, 0, idx, length, left_product_sum);
        if (left >= fft_ccr_max - CORRELATION_REFINE_MARGIN) {
            select_phase_candidate(refined, refined_ccr,
                                   phase_candidate_for_shift(reference, comparison, 0, idx, length, idx, max_shift));
        }

        const double right_product_sum = product_sum_from_fft(sums, span_size(reference), idx, 0);
        const double right             = correlation_from_cached_product(cache, idx, 0, length, right_product_sum);
        if (right >= fft_ccr_max - CORRELATION_REFINE_MARGIN) {
            select_phase_candidate(refined, refined_ccr,
                                   phase_candidate_for_shift(reference, comparison, idx, 0, length, idx, max_shift));
        }
    }

    return refined;
}

PhaseResult compute_phase_alignment (DoubleSpan reference, DoubleSpan comparison, const ScoreParams& params) {
    const Index  reference_n  = span_size(reference);
    const Index  comparison_n = span_size(comparison);
    const double max_shift    = std::round((1.0 - params.init_min) * 100.0) / 100.0;
    PhaseResult  result       = phase_candidate_for_shift(reference, comparison, 0, 0, reference_n, 0, max_shift);
    if (result.correlation.rho_e == 1.0) {
        return result;
    }

    const Index      window_size = static_cast<Index>(std::floor(static_cast<double>(comparison_n) * max_shift) + 1.0);
    const Index      bounded_window_size = std::min(window_size, reference_n);
    const PhaseCache cache               = build_phase_cache(reference, comparison);
    double           ccr_max             = result.correlation.rho_e;

    const PhaseProductSums sums = fft_product_sums(reference, comparison);
    for (Index idx = 1; idx < bounded_window_size; ++idx) {
        const Index       length         = reference_n - idx;
        const PhaseResult left_candidate = phase_candidate_from_fft_product(
            reference, comparison, cache, sums, reference_n, 0, idx, length, idx, max_shift);
        select_phase_candidate(result, ccr_max, left_candidate);

        const PhaseResult right_candidate = phase_candidate_from_fft_product(
            reference, comparison, cache, sums, reference_n, idx, 0, length, idx, max_shift);
        select_phase_candidate(result, ccr_max, right_candidate);
    }

    result = refine_fft_phase_result(reference, comparison, cache, sums, bounded_window_size, max_shift, ccr_max);

    if (result.alignment.length < 9) {
        result = phase_candidate_for_shift(reference, comparison, 0, 0, reference_n, 0, max_shift);
        append_warning(result.diagnostics, DiagnosticComponent::Phase, DiagnosticCode::PhaseShiftClampedToUnshifted);
    }

    return result;
}

double corridor_score (DoubleSpan reference, DoubleSpan comparison, const ScoreParams& params) {
    const Index n      = span_size(reference);
    double      t_norm = 0.0;
    for (Index idx = 0; idx < n; ++idx) {
        t_norm = std::max(t_norm, std::abs(value_at(reference, idx)));
    }

    if (t_norm == 0.0) {
        double sum = 0.0;
        for (Index idx = 0; idx < n; ++idx) {
            if (value_at(reference, idx) == value_at(comparison, idx)) {
                sum += 1.0;
            }
        }
        return sum / static_cast<double>(n);
    }

    const double inner_corridor = params.a_0 * t_norm;
    const double outer_corridor = params.b_0 * t_norm;
    double       sum            = 0.0;
    for (Index idx = 0; idx < n; ++idx) {
        const double diff = std::abs(value_at(reference, idx) - value_at(comparison, idx));
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
    return sum / static_cast<double>(n);
}

double phase_score (DoubleSpan reference, const ScoreParams& params, const PhaseAlignment& alignment) {
    const double max_allowable_time_shift_threshold = static_cast<double>(span_size(reference)) * alignment.max_shift;
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

MagnitudeResult magnitude_score_from_values (DoubleSpan reference_values, DoubleSpan comparison_values,
                                             const ScoreParams& params) {
    const auto [numerator, denominator] = magnitude_error_from_dtw(comparison_values, reference_values, 0.1);
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

void gradient_values (DoubleSpan values, double dt, std::vector<double>& gradient) {
    const Index n = span_size(values);
    gradient.assign(static_cast<std::size_t>(n), 0.0);
    gradient[0] = (value_at(values, 1) - value_at(values, 0)) / dt;
    for (Index idx = 1; idx < n - 1; ++idx) {
        gradient[static_cast<std::size_t>(idx)] = (value_at(values, idx + 1) - value_at(values, idx - 1)) / (2.0 * dt);
    }
    gradient[static_cast<std::size_t>(n - 1)] = (value_at(values, n - 1) - value_at(values, n - 2)) / dt;
}

double smoothed_slope_at (const std::vector<double>& gradient, Index idx) {
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

SlopeResult fused_slope_score_from_values (DoubleSpan reference_values, DoubleSpan comparison_values,
                                           const ScoreParams& params, double dt) {
    const Index n = span_size(reference_values);
    if (n < 9) {
        throw std::invalid_argument("Shifted curves must have at least 9 samples for slope rating");
    }

    std::vector<double> comparison_gradient;
    std::vector<double> reference_gradient;
    gradient_values(comparison_values, dt, comparison_gradient);
    gradient_values(reference_values, dt, reference_gradient);

    double numerator   = 0.0;
    double denominator = 0.0;
    for (Index idx = 0; idx < n; ++idx) {
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

ScoreResult score_components_impl (DoubleSpan reference, DoubleSpan comparison, const ScoreParams& params, double dt) {
    ScoreResult result;
    result.phase          = compute_phase_alignment(reference, comparison, params);
    result.corridor.score = corridor_score(reference, comparison, params);
    result.phase.score    = phase_score(reference, params, result.phase.alignment);

    const DoubleSpan aligned_comparison =
        comparison.subspan(offset(result.phase.alignment.comparison_start), offset(result.phase.alignment.length));
    const DoubleSpan aligned_reference =
        reference.subspan(offset(result.phase.alignment.reference_start), offset(result.phase.alignment.length));

    result.magnitude = magnitude_score_from_values(aligned_reference, aligned_comparison, params);
    result.slope     = fused_slope_score_from_values(aligned_reference, aligned_comparison, params, dt);
    result.overall   = params.w_z * result.corridor.score + params.w_p * result.phase.score +
                       params.w_m * result.magnitude.score + params.w_s * result.slope.score;
    return result;
}

} // namespace

namespace iso18571 {

ScoreResult ISO18571_VARIANT (score_components)(DoubleSpan reference, DoubleSpan comparison, const ScoreParams& params,
                                                double dt) {
    return score_components_impl(reference, comparison, params, dt);
}

} // namespace iso18571

#undef ISO18571_VARIANT
#undef ISO18571_PASTE
#undef ISO18571_PASTE_INNER
