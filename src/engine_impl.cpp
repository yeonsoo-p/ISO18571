#include "dispatch.h"
#include "engine.h"
#include "validation.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <limits>
#include <numbers>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#ifndef IMPL_SUFFIX
#error "IMPL_SUFFIX must be defined before compiling engine_impl.cpp"
#endif

namespace {

using engine::CorridorResult;
using engine::DiagnosticCode;
using engine::DiagnosticComponent;
using engine::DiagnosticSeverity;
using engine::Index;
using engine::MagnitudeResult;
using engine::PhaseResult;
using engine::ScoreParams;
using engine::ScoreResult;
using engine::SlopeResult;
using std::size_t;

inline void sum_difference (c128& a, c128& b, c128 c, c128 d) {
    a = c + d;
    b = c - d;
}

inline void sum_difference_in_place (c128& a, c128& b) {
    c128 t = a;
    a += b;
    b = t - b;
}

template<bool fwd>
void special_mul (const c128& v1, const c128& v2, c128& res) {
    if constexpr (fwd) {
        res = v1 * std::conj(v2);
    } else {
        res = v1 * v2;
    }
}

template<bool fwd>
void rotate_x90 (c128& a) {
    if constexpr (fwd) {
        const f64 tmp_ = -a.real();
        a.real(a.imag());
        a.imag(tmp_);
    } else {
        const f64 tmp_ = a.real();
        a.real(-a.imag());
        a.imag(tmp_);
    }
}

c128& ch_at (std::span<c128> ch, size_t ido, size_t l1, size_t a, size_t b, size_t c) {
    return ch[a + ido * (b + l1 * c)];
}

const c128& cc_at (std::span<const c128> cc, size_t ido, size_t radix, size_t a, size_t b, size_t c) {
    return cc[a + ido * (b + radix * c)];
}

const c128& wa_at (std::span<const c128> wa, size_t ido, size_t x, size_t i) { return wa[i - 1 + x * (ido - 1)]; }

template<bool fwd>
void pass2 (size_t ido, size_t l1, std::span<const c128> cc, std::span<c128> ch, std::span<const c128> wa) {
    if (ido == 1) {
        for (size_t k = 0; k < l1; ++k) {
            ch_at(ch, ido, l1, 0, k, 0) = cc_at(cc, ido, 2, 0, 0, k) + cc_at(cc, ido, 2, 0, 1, k);
            ch_at(ch, ido, l1, 0, k, 1) = cc_at(cc, ido, 2, 0, 0, k) - cc_at(cc, ido, 2, 0, 1, k);
        }
    } else {
        for (size_t k = 0; k < l1; ++k) {
            ch_at(ch, ido, l1, 0, k, 0) = cc_at(cc, ido, 2, 0, 0, k) + cc_at(cc, ido, 2, 0, 1, k);
            ch_at(ch, ido, l1, 0, k, 1) = cc_at(cc, ido, 2, 0, 0, k) - cc_at(cc, ido, 2, 0, 1, k);
            for (size_t i = 1; i < ido; ++i) {
                ch_at(ch, ido, l1, i, k, 0) = cc_at(cc, ido, 2, i, 0, k) + cc_at(cc, ido, 2, i, 1, k);
                special_mul<fwd>(cc_at(cc, ido, 2, i, 0, k) - cc_at(cc, ido, 2, i, 1, k), wa_at(wa, ido, 0, i),
                                 ch_at(ch, ido, l1, i, k, 1));
            }
        }
    }
}

template<bool fwd>
void pass4 (size_t ido, size_t l1, std::span<const c128> cc, std::span<c128> ch, std::span<const c128> wa) {
    if (ido == 1) {
        for (size_t k = 0; k < l1; ++k) {
            c128 t1, t2, t3, t4;
            sum_difference(t2, t1, cc_at(cc, ido, 4, 0, 0, k), cc_at(cc, ido, 4, 0, 2, k));
            sum_difference(t3, t4, cc_at(cc, ido, 4, 0, 1, k), cc_at(cc, ido, 4, 0, 3, k));
            rotate_x90<fwd>(t4);
            sum_difference(ch_at(ch, ido, l1, 0, k, 0), ch_at(ch, ido, l1, 0, k, 2), t2, t3);
            sum_difference(ch_at(ch, ido, l1, 0, k, 1), ch_at(ch, ido, l1, 0, k, 3), t1, t4);
        }
    } else {
        for (size_t k = 0; k < l1; ++k) {
            {
                c128 t1, t2, t3, t4;
                sum_difference(t2, t1, cc_at(cc, ido, 4, 0, 0, k), cc_at(cc, ido, 4, 0, 2, k));
                sum_difference(t3, t4, cc_at(cc, ido, 4, 0, 1, k), cc_at(cc, ido, 4, 0, 3, k));
                rotate_x90<fwd>(t4);
                sum_difference(ch_at(ch, ido, l1, 0, k, 0), ch_at(ch, ido, l1, 0, k, 2), t2, t3);
                sum_difference(ch_at(ch, ido, l1, 0, k, 1), ch_at(ch, ido, l1, 0, k, 3), t1, t4);
            }
            for (size_t i = 1; i < ido; ++i) {
                c128       t1, t2, t3, t4;
                const c128 cc0 = cc_at(cc, ido, 4, i, 0, k);
                const c128 cc1 = cc_at(cc, ido, 4, i, 1, k);
                const c128 cc2 = cc_at(cc, ido, 4, i, 2, k);
                const c128 cc3 = cc_at(cc, ido, 4, i, 3, k);
                sum_difference(t2, t1, cc0, cc2);
                sum_difference(t3, t4, cc1, cc3);
                rotate_x90<fwd>(t4);
                ch_at(ch, ido, l1, i, k, 0) = t2 + t3;
                special_mul<fwd>(t1 + t4, wa_at(wa, ido, 0, i), ch_at(ch, ido, l1, i, k, 1));
                special_mul<fwd>(t2 - t3, wa_at(wa, ido, 1, i), ch_at(ch, ido, l1, i, k, 2));
                special_mul<fwd>(t1 - t4, wa_at(wa, ido, 2, i), ch_at(ch, ido, l1, i, k, 3));
            }
        }
    }
}

template<bool fwd>
void rotate_x45 (c128& a) {
    constexpr f64 hsqt2 = 1.0 / std::numbers::sqrt2_v<f64>;
    if constexpr (fwd) {
        const f64 tmp_ = a.real();
        a.real(hsqt2 * (a.real() + a.imag()));
        a.imag(hsqt2 * (a.imag() - tmp_));
    } else {
        const f64 tmp_ = a.real();
        a.real(hsqt2 * (a.real() - a.imag()));
        a.imag(hsqt2 * (a.imag() + tmp_));
    }
}

template<bool fwd>
void rotate_x135 (c128& a) {
    constexpr f64 hsqt2 = 1.0 / std::numbers::sqrt2_v<f64>;
    if constexpr (fwd) {
        const f64 tmp_ = a.real();
        a.real(hsqt2 * (a.imag() - a.real()));
        a.imag(hsqt2 * (-tmp_ - a.imag()));
    } else {
        const f64 tmp_ = a.real();
        a.real(hsqt2 * (-a.real() - a.imag()));
        a.imag(hsqt2 * (tmp_ - a.imag()));
    }
}

template<bool fwd>
void pass8 (size_t ido, size_t l1, std::span<const c128> cc, std::span<c128> ch, std::span<const c128> wa) {
    if (ido == 1) {
        for (size_t k = 0; k < l1; ++k) {
            c128 a0, a1, a2, a3, a4, a5, a6, a7;
            sum_difference(a1, a5, cc_at(cc, ido, 8, 0, 1, k), cc_at(cc, ido, 8, 0, 5, k));
            sum_difference(a3, a7, cc_at(cc, ido, 8, 0, 3, k), cc_at(cc, ido, 8, 0, 7, k));
            sum_difference_in_place(a1, a3);
            rotate_x90<fwd>(a3);

            rotate_x90<fwd>(a7);
            sum_difference_in_place(a5, a7);
            rotate_x45<fwd>(a5);
            rotate_x135<fwd>(a7);

            sum_difference(a0, a4, cc_at(cc, ido, 8, 0, 0, k), cc_at(cc, ido, 8, 0, 4, k));
            sum_difference(a2, a6, cc_at(cc, ido, 8, 0, 2, k), cc_at(cc, ido, 8, 0, 6, k));
            sum_difference(ch_at(ch, ido, l1, 0, k, 0), ch_at(ch, ido, l1, 0, k, 4), a0 + a2, a1);
            sum_difference(ch_at(ch, ido, l1, 0, k, 2), ch_at(ch, ido, l1, 0, k, 6), a0 - a2, a3);
            rotate_x90<fwd>(a6);
            sum_difference(ch_at(ch, ido, l1, 0, k, 1), ch_at(ch, ido, l1, 0, k, 5), a4 + a6, a5);
            sum_difference(ch_at(ch, ido, l1, 0, k, 3), ch_at(ch, ido, l1, 0, k, 7), a4 - a6, a7);
        }
    } else {
        for (size_t k = 0; k < l1; ++k) {
            {
                c128 a0, a1, a2, a3, a4, a5, a6, a7;
                sum_difference(a1, a5, cc_at(cc, ido, 8, 0, 1, k), cc_at(cc, ido, 8, 0, 5, k));
                sum_difference(a3, a7, cc_at(cc, ido, 8, 0, 3, k), cc_at(cc, ido, 8, 0, 7, k));
                sum_difference_in_place(a1, a3);
                rotate_x90<fwd>(a3);

                rotate_x90<fwd>(a7);
                sum_difference_in_place(a5, a7);
                rotate_x45<fwd>(a5);
                rotate_x135<fwd>(a7);

                sum_difference(a0, a4, cc_at(cc, ido, 8, 0, 0, k), cc_at(cc, ido, 8, 0, 4, k));
                sum_difference(a2, a6, cc_at(cc, ido, 8, 0, 2, k), cc_at(cc, ido, 8, 0, 6, k));
                sum_difference(ch_at(ch, ido, l1, 0, k, 0), ch_at(ch, ido, l1, 0, k, 4), a0 + a2, a1);
                sum_difference(ch_at(ch, ido, l1, 0, k, 2), ch_at(ch, ido, l1, 0, k, 6), a0 - a2, a3);
                rotate_x90<fwd>(a6);
                sum_difference(ch_at(ch, ido, l1, 0, k, 1), ch_at(ch, ido, l1, 0, k, 5), a4 + a6, a5);
                sum_difference(ch_at(ch, ido, l1, 0, k, 3), ch_at(ch, ido, l1, 0, k, 7), a4 - a6, a7);
            }
            for (size_t i = 1; i < ido; ++i) {
                c128 a0, a1, a2, a3, a4, a5, a6, a7;
                sum_difference(a1, a5, cc_at(cc, ido, 8, i, 1, k), cc_at(cc, ido, 8, i, 5, k));
                sum_difference(a3, a7, cc_at(cc, ido, 8, i, 3, k), cc_at(cc, ido, 8, i, 7, k));
                rotate_x90<fwd>(a7);
                sum_difference_in_place(a1, a3);
                rotate_x90<fwd>(a3);
                sum_difference_in_place(a5, a7);
                rotate_x45<fwd>(a5);
                rotate_x135<fwd>(a7);
                sum_difference(a0, a4, cc_at(cc, ido, 8, i, 0, k), cc_at(cc, ido, 8, i, 4, k));
                sum_difference(a2, a6, cc_at(cc, ido, 8, i, 2, k), cc_at(cc, ido, 8, i, 6, k));
                sum_difference_in_place(a0, a2);
                ch_at(ch, ido, l1, i, k, 0) = a0 + a1;
                special_mul<fwd>(a0 - a1, wa_at(wa, ido, 3, i), ch_at(ch, ido, l1, i, k, 4));
                special_mul<fwd>(a2 + a3, wa_at(wa, ido, 1, i), ch_at(ch, ido, l1, i, k, 2));
                special_mul<fwd>(a2 - a3, wa_at(wa, ido, 5, i), ch_at(ch, ido, l1, i, k, 6));
                rotate_x90<fwd>(a6);
                sum_difference_in_place(a4, a6);
                special_mul<fwd>(a4 + a5, wa_at(wa, ido, 0, i), ch_at(ch, ido, l1, i, k, 1));
                special_mul<fwd>(a4 - a5, wa_at(wa, ido, 4, i), ch_at(ch, ido, l1, i, k, 5));
                special_mul<fwd>(a6 + a7, wa_at(wa, ido, 2, i), ch_at(ch, ido, l1, i, k, 3));
                special_mul<fwd>(a6 - a7, wa_at(wa, ido, 6, i), ch_at(ch, ido, l1, i, k, 7));
            }
        }
    }
}

constexpr f64 CORRELATION_TIE_TOLERANCE = 1.0e-12;

std::size_t offset (Index index) { return static_cast<std::size_t>(index); }

Index span_size (std::span<const f64> values) { return static_cast<Index>(values.size()); }

f64 value_at (std::span<const f64> values, Index index) { return values[offset(index)]; }

f64 integer_power (f64 value, int exponent) {
    if (exponent == 1) {
        return value;
    }
    if (exponent == 2) {
        return value * value;
    }
    if (exponent == 3) {
        return value * value * value;
    }

    f64 result = 1.0;
    f64 base   = value;
    int power  = exponent;
    while (power > 0) {
        if ((power & 1) != 0) {
            result *= base;
        }
        base *= base;
        power >>= 1;
    }
    return result;
}

void select_dtw_predecessor (f64 cost, f64 candidate_numerator, f64 candidate_denominator, f64& best_previous,
                             f64& best_numerator, f64& best_denominator) {
    best_previous    = cost;
    best_numerator   = candidate_numerator;
    best_denominator = candidate_denominator;
}

Index window_radius (Index n, f64 window_size) {
    if (n <= 0) {
        throw std::invalid_argument("DTW input arrays must not be empty");
    }
    if (!std::isfinite(window_size) || window_size < 0.0) {
        throw std::invalid_argument("DTW window_size must be a finite non-negative value");
    }
    if (window_size >= 1.0) {
        return n;
    }
    const Index raw = static_cast<Index>(std::ceil(window_size * static_cast<f64>(n)));
    return std::min<Index>(n, std::max<Index>(1, raw));
}

std::pair<f64, f64> magnitude_error_from_dtw (MagnitudeResult& result, std::span<const f64> x, std::span<const f64> y,
                                              f64 window_size) {
    const Index n      = span_size(x);
    const Index radius = window_radius(n, window_size);
    const f64   inf    = std::numeric_limits<f64>::infinity();

    std::vector<f64> previous_cost(static_cast<std::size_t>(n), inf);
    std::vector<f64> current_cost(static_cast<std::size_t>(n), inf);
    std::vector<f64> previous_numerator(static_cast<std::size_t>(n), 0.0);
    std::vector<f64> current_numerator(static_cast<std::size_t>(n), 0.0);
    std::vector<f64> previous_denominator(static_cast<std::size_t>(n), 0.0);
    std::vector<f64> current_denominator(static_cast<std::size_t>(n), 0.0);
    std::vector<f64> abs_y(static_cast<std::size_t>(n), 0.0);

    for (Index idx = 0; idx < n; ++idx) {
        abs_y[offset(idx)] = std::abs(value_at(y, idx));
    }

    for (Index i = 0; i < n; ++i) {
        const Index previous_start = i > 0 ? std::max<Index>(0, i - radius) : 0;
        const Index previous_stop  = i > 0 ? std::min<Index>(n, i + radius - 1) : 0;
        const Index j_start        = std::max<Index>(0, i - radius + 1);
        const Index j_stop         = std::min<Index>(n, i + radius);
        const f64   x_i            = value_at(x, i);
        Index       interior_start = j_stop;
        Index       interior_stop  = j_stop;

        if (i > 0) {
            interior_start = std::max<Index>(j_start + 1, previous_start + 1);
            interior_stop  = std::min<Index>(j_stop, previous_stop);
            if (interior_start >= interior_stop) {
                interior_start = j_stop;
                interior_stop  = j_stop;
            }
        }

        for (Index j = j_start; j < interior_start; ++j) {
            const std::size_t index             = static_cast<std::size_t>(j);
            const f64         delta             = x_i - value_at(y, j);
            const f64         local_cost        = delta * delta;
            const f64         local_numerator   = std::abs(delta);
            const f64         local_denominator = abs_y[index];
            f64               accumulated       = inf;
            f64               numerator         = 0.0;
            f64               denominator       = 0.0;

            if (i == 0 && j == 0) {
                accumulated = local_cost;
                numerator   = local_numerator;
                denominator = local_denominator;
            } else {
                f64 best_previous    = inf;
                f64 best_numerator   = 0.0;
                f64 best_denominator = 0.0;

                if (i > 0 && j >= previous_start && j < previous_stop) {
                    select_dtw_predecessor(previous_cost[index], previous_numerator[index], previous_denominator[index],
                                           best_previous, best_numerator, best_denominator);
                }
                if (j > j_start) {
                    const std::size_t previous_index = static_cast<std::size_t>(j - 1);
                    const f64         candidate      = current_cost[previous_index];
                    if (candidate < best_previous) {
                        select_dtw_predecessor(candidate, current_numerator[previous_index],
                                               current_denominator[previous_index], best_previous, best_numerator,
                                               best_denominator);
                    }
                }
                if (i > 0 && j > 0 && j - 1 >= previous_start && j - 1 < previous_stop) {
                    const std::size_t previous_index = static_cast<std::size_t>(j - 1);
                    const f64         candidate      = previous_cost[previous_index];
                    if (candidate < best_previous) {
                        select_dtw_predecessor(candidate, previous_numerator[previous_index],
                                               previous_denominator[previous_index], best_previous, best_numerator,
                                               best_denominator);
                    }
                }

                if (std::isfinite(best_previous)) {
                    accumulated = local_cost + best_previous;
                    numerator   = local_numerator + best_numerator;
                    denominator = local_denominator + best_denominator;
                }
            }

            current_cost[index]        = accumulated;
            current_numerator[index]   = numerator;
            current_denominator[index] = denominator;
        }

        for (Index j = interior_start; j < interior_stop; ++j) {
            const std::size_t index             = static_cast<std::size_t>(j);
            const std::size_t previous_index    = static_cast<std::size_t>(j - 1);
            const f64         delta             = x_i - value_at(y, j);
            const f64         local_cost        = delta * delta;
            const f64         local_numerator   = std::abs(delta);
            const f64         local_denominator = abs_y[index];
            f64               best_previous     = previous_cost[index];
            f64               best_numerator    = previous_numerator[index];
            f64               best_denominator  = previous_denominator[index];
            const f64         horizontal        = current_cost[previous_index];
            if (horizontal < best_previous) {
                best_previous    = horizontal;
                best_numerator   = current_numerator[previous_index];
                best_denominator = current_denominator[previous_index];
            }
            const f64 diagonal = previous_cost[previous_index];
            if (diagonal < best_previous) {
                best_previous    = diagonal;
                best_numerator   = previous_numerator[previous_index];
                best_denominator = previous_denominator[previous_index];
            }
            if (std::isfinite(best_previous)) {
                current_cost[index]        = local_cost + best_previous;
                current_numerator[index]   = local_numerator + best_numerator;
                current_denominator[index] = local_denominator + best_denominator;
            } else {
                current_cost[index]        = inf;
                current_numerator[index]   = 0.0;
                current_denominator[index] = 0.0;
            }
        }

        for (Index j = interior_stop; j < j_stop; ++j) {
            const std::size_t index             = static_cast<std::size_t>(j);
            const f64         delta             = x_i - value_at(y, j);
            const f64         local_cost        = delta * delta;
            const f64         local_numerator   = std::abs(delta);
            const f64         local_denominator = abs_y[index];
            f64               accumulated       = inf;
            f64               numerator         = 0.0;
            f64               denominator       = 0.0;

            if (i == 0 && j == 0) {
                accumulated = local_cost;
                numerator   = local_numerator;
                denominator = local_denominator;
            } else {
                f64 best_previous    = inf;
                f64 best_numerator   = 0.0;
                f64 best_denominator = 0.0;

                if (i > 0 && j >= previous_start && j < previous_stop) {
                    select_dtw_predecessor(previous_cost[index], previous_numerator[index], previous_denominator[index],
                                           best_previous, best_numerator, best_denominator);
                }
                if (j > j_start) {
                    const std::size_t previous_index = static_cast<std::size_t>(j - 1);
                    const f64         candidate      = current_cost[previous_index];
                    if (candidate < best_previous) {
                        select_dtw_predecessor(candidate, current_numerator[previous_index],
                                               current_denominator[previous_index], best_previous, best_numerator,
                                               best_denominator);
                    }
                }
                if (i > 0 && j > 0 && j - 1 >= previous_start && j - 1 < previous_stop) {
                    const std::size_t previous_index = static_cast<std::size_t>(j - 1);
                    const f64         candidate      = previous_cost[previous_index];
                    if (candidate < best_previous) {
                        select_dtw_predecessor(candidate, previous_numerator[previous_index],
                                               previous_denominator[previous_index], best_previous, best_numerator,
                                               best_denominator);
                    }
                }

                if (std::isfinite(best_previous)) {
                    accumulated = local_cost + best_previous;
                    numerator   = local_numerator + best_numerator;
                    denominator = local_denominator + best_denominator;
                }
            }

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

bool values_equal_for_shift (std::span<const f64> reference, std::span<const f64> comparison, Index reference_start,
                             Index comparison_start, Index length) {
    for (Index idx = 0; idx < length; ++idx) {
        if (value_at(reference, reference_start + idx) != value_at(comparison, comparison_start + idx)) {
            return false;
        }
    }
    return true;
}

std::size_t next_power_of_two (std::size_t value) {
    std::size_t out = 1;
    while (out < value) {
        out <<= 1U;
    }
    return out;
}
void phase_score (PhaseResult& result, std::span<const f64> reference, std::span<const f64> comparison,
                  const ScoreParams& params) {
    const Index reference_n = span_size(reference);
    const f64   max_shift   = std::round((1.0 - params.init_min) * 100.0) / 100.0;

    result.reference_start  = 0;
    result.comparison_start = 0;
    result.length           = reference_n;
    result.n_eps            = 0;
    result.max_shift        = max_shift;
    result.diagnostics.clear();

    f64 baseline_reference_sum  = 0.0;
    f64 baseline_comparison_sum = 0.0;
    for (Index value_idx = 0; value_idx < reference_n; ++value_idx) {
        baseline_reference_sum += value_at(reference, value_idx);
        baseline_comparison_sum += value_at(comparison, value_idx);
    }

    const f64 baseline_n              = static_cast<f64>(reference_n);
    const f64 baseline_reference_avg  = baseline_reference_sum / baseline_n;
    const f64 baseline_comparison_avg = baseline_comparison_sum / baseline_n;
    f64       baseline_reference_cov  = 0.0;
    f64       baseline_comparison_cov = 0.0;
    f64       baseline_cross_cov      = 0.0;
    for (Index value_idx = 0; value_idx < reference_n; ++value_idx) {
        const f64 x = value_at(reference, value_idx) - baseline_reference_avg;
        const f64 y = value_at(comparison, value_idx) - baseline_comparison_avg;
        baseline_reference_cov += x * x;
        baseline_comparison_cov += y * y;
        baseline_cross_cov += x * y;
    }

    if (baseline_reference_cov <= 0.0 || baseline_comparison_cov <= 0.0) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::Warning, DiagnosticComponent::Phase, DiagnosticCode::PhaseUndefinedCorrelation});
        result.rho_e = values_equal_for_shift(reference, comparison, 0, 0, reference_n) ? 1.0 : 0.0;
    } else {
        f64 baseline_correlation = baseline_cross_cov / std::sqrt(baseline_reference_cov);
        baseline_correlation /= std::sqrt(baseline_comparison_cov);
        if (baseline_correlation > 1.0) {
            baseline_correlation = 1.0;
        } else if (baseline_correlation < -1.0) {
            baseline_correlation = -1.0;
        }
        result.rho_e = baseline_correlation;
    }
    const PhaseResult unshifted_result = result;
    if (result.rho_e == 1.0) {
        result.score = 1.0;
        return;
    }
    if (reference_n < 9) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::Warning, DiagnosticComponent::Phase, DiagnosticCode::PhaseShiftClampedToUnshifted});
        result.score = 1.0;
        return;
    }

    const Index window_size         = static_cast<Index>(std::floor(static_cast<f64>(reference_n) * max_shift) + 1.0);
    const Index bounded_window_size = std::min(window_size, reference_n);
    if (bounded_window_size <= 1) {
        result.score = 1.0;
        return;
    }

    const std::size_t prefix_size = static_cast<std::size_t>(reference_n + 1);
    std::vector<f64>  reference_prefix_sum;
    std::vector<f64>  comparison_prefix_sum;
    std::vector<f64>  reference_square_prefix_sum;
    std::vector<f64>  comparison_square_prefix_sum;
    reference_prefix_sum.reserve(prefix_size);
    comparison_prefix_sum.reserve(prefix_size);
    reference_square_prefix_sum.reserve(prefix_size);
    comparison_square_prefix_sum.reserve(prefix_size);
    reference_prefix_sum.push_back(0.0);
    comparison_prefix_sum.push_back(0.0);
    reference_square_prefix_sum.push_back(0.0);
    comparison_square_prefix_sum.push_back(0.0);

    f64 reference_running_sum         = 0.0;
    f64 comparison_running_sum        = 0.0;
    f64 reference_square_running_sum  = 0.0;
    f64 comparison_square_running_sum = 0.0;
    for (Index idx = 0; idx < reference_n; ++idx) {
        const f64 x = value_at(reference, idx);
        const f64 y = value_at(comparison, idx);
        reference_running_sum += x;
        comparison_running_sum += y;
        reference_square_running_sum += x * x;
        comparison_square_running_sum += y * y;
        reference_prefix_sum.push_back(reference_running_sum);
        comparison_prefix_sum.push_back(comparison_running_sum);
        reference_square_prefix_sum.push_back(reference_square_running_sum);
        comparison_square_prefix_sum.push_back(comparison_square_running_sum);
    }

    const std::size_t   phase_n        = static_cast<std::size_t>(reference_n);
    const std::size_t   phase_fft_size = next_power_of_two(2U * phase_n - 1U);
    std::vector<size_t> phase_fft_factors;
    std::vector<c128>   phase_fft_twiddles;
    size_t              len = phase_fft_size;
    while ((len & 7U) == 0U) {
        phase_fft_factors.push_back(8);
        len >>= 3U;
    }
    while ((len & 3U) == 0U) {
        phase_fft_factors.push_back(4);
        len >>= 2U;
    }
    if (len == 2U) {
        phase_fft_factors.push_back(2);
        std::swap(phase_fft_factors[0], phase_fft_factors.back());
    }

    size_t twiddle_size = 0;
    size_t l1           = 1;
    for (const size_t ip : phase_fft_factors) {
        const size_t ido = phase_fft_size / (l1 * ip);
        twiddle_size += (ip - 1) * (ido - 1);
        l1 *= ip;
    }
    phase_fft_twiddles.reserve(twiddle_size);

    constexpr f128 pi = std::numbers::pi_v<f128>;
    l1                = 1;
    for (const size_t ip : phase_fft_factors) {
        const size_t ido = phase_fft_size / (l1 * ip);
        for (size_t j = 1; j < ip; ++j) {
            for (size_t i = 1; i < ido; ++i) {
                const size_t twiddle_idx = j * l1 * i;
                const f64    angle =
                    static_cast<f64>(2.0L * pi * static_cast<f128>(twiddle_idx) / static_cast<f128>(phase_fft_size));
                phase_fft_twiddles.emplace_back(std::cos(angle), std::sin(angle));
            }
        }
        l1 *= ip;
    }

    std::vector<c128> reference_fft(phase_fft_size);
    std::vector<c128> comparison_fft(phase_fft_size);
    for (std::size_t idx = 0; idx < phase_n; ++idx) {
        reference_fft[idx]  = {value_at(reference, static_cast<Index>(idx)), 0.0};
        comparison_fft[idx] = {value_at(comparison, static_cast<Index>(phase_n - idx - 1U)), 0.0};
    }

    std::vector<c128> phase_fft_work(phase_fft_size);
    size_t            reference_fft_l1             = 1;
    size_t            reference_fft_twiddle_offset = 0;
    std::span<c128>   reference_fft_p1(reference_fft);
    std::span<c128>   reference_fft_p2(phase_fft_work);
    for (const size_t ip : phase_fft_factors) {
        const size_t                l2            = ip * reference_fft_l1;
        const size_t                ido           = phase_fft_size / l2;
        const size_t                twiddle_count = (ip - 1) * (ido - 1);
        const std::span<const c128> twiddle_data =
            std::span<const c128>(phase_fft_twiddles).subspan(reference_fft_twiddle_offset, twiddle_count);
        if (ip == 4) {
            pass4<fft::kForward>(ido, reference_fft_l1, reference_fft_p1, reference_fft_p2, twiddle_data);
        } else if (ip == 8) {
            pass8<fft::kForward>(ido, reference_fft_l1, reference_fft_p1, reference_fft_p2, twiddle_data);
        } else if (ip == 2) {
            pass2<fft::kForward>(ido, reference_fft_l1, reference_fft_p1, reference_fft_p2, twiddle_data);
        }
        std::swap(reference_fft_p1, reference_fft_p2);
        reference_fft_l1 = l2;
        reference_fft_twiddle_offset += twiddle_count;
    }
    if (reference_fft_p1.data() != reference_fft.data()) {
        std::swap(reference_fft, phase_fft_work);
    }

    size_t          comparison_fft_l1             = 1;
    size_t          comparison_fft_twiddle_offset = 0;
    std::span<c128> comparison_fft_p1(comparison_fft);
    std::span<c128> comparison_fft_p2(phase_fft_work);
    for (const size_t ip : phase_fft_factors) {
        const size_t                l2            = ip * comparison_fft_l1;
        const size_t                ido           = phase_fft_size / l2;
        const size_t                twiddle_count = (ip - 1) * (ido - 1);
        const std::span<const c128> twiddle_data =
            std::span<const c128>(phase_fft_twiddles).subspan(comparison_fft_twiddle_offset, twiddle_count);
        if (ip == 4) {
            pass4<fft::kForward>(ido, comparison_fft_l1, comparison_fft_p1, comparison_fft_p2, twiddle_data);
        } else if (ip == 8) {
            pass8<fft::kForward>(ido, comparison_fft_l1, comparison_fft_p1, comparison_fft_p2, twiddle_data);
        } else if (ip == 2) {
            pass2<fft::kForward>(ido, comparison_fft_l1, comparison_fft_p1, comparison_fft_p2, twiddle_data);
        }
        std::swap(comparison_fft_p1, comparison_fft_p2);
        comparison_fft_l1 = l2;
        comparison_fft_twiddle_offset += twiddle_count;
    }
    for (std::size_t idx = 0; idx < phase_fft_size; ++idx) {
        reference_fft[idx] *= comparison_fft_p1[idx];
    }

    const f64       inverse_fft_scale          = 1.0 / static_cast<f64>(phase_fft_size);
    size_t          inverse_fft_l1             = 1;
    size_t          inverse_fft_twiddle_offset = 0;
    std::span<c128> inverse_fft_p1(reference_fft);
    std::span<c128> inverse_fft_p2(comparison_fft);
    for (const size_t ip : phase_fft_factors) {
        const size_t                l2            = ip * inverse_fft_l1;
        const size_t                ido           = phase_fft_size / l2;
        const size_t                twiddle_count = (ip - 1) * (ido - 1);
        const std::span<const c128> twiddle_data =
            std::span<const c128>(phase_fft_twiddles).subspan(inverse_fft_twiddle_offset, twiddle_count);
        if (ip == 4) {
            pass4<fft::kBackward>(ido, inverse_fft_l1, inverse_fft_p1, inverse_fft_p2, twiddle_data);
        } else if (ip == 8) {
            pass8<fft::kBackward>(ido, inverse_fft_l1, inverse_fft_p1, inverse_fft_p2, twiddle_data);
        } else if (ip == 2) {
            pass2<fft::kBackward>(ido, inverse_fft_l1, inverse_fft_p1, inverse_fft_p2, twiddle_data);
        }
        std::swap(inverse_fft_p1, inverse_fft_p2);
        inverse_fft_l1 = l2;
        inverse_fft_twiddle_offset += twiddle_count;
    }
    if (inverse_fft_p1.data() != reference_fft.data()) {
        for (size_t i = 0; i < phase_fft_size; ++i) {
            reference_fft[i] = inverse_fft_p1[i] * inverse_fft_scale;
        }
    } else {
        for (size_t i = 0; i < phase_fft_size; ++i) {
            reference_fft[i] *= inverse_fft_scale;
        }
    }

    f64 refined_ccr = result.rho_e;

    for (Index idx = 1; idx < bounded_window_size; ++idx) {
        const Index length = reference_n - idx;
        const f64   n      = static_cast<f64>(length);
        const f64   inv_n  = 1.0 / n;

        for (int side = 0; side < 2; ++side) {
            const bool  left_side                  = side == 0;
            const Index candidate_reference_start  = left_side ? 0 : idx;
            const Index candidate_comparison_start = left_side ? idx : 0;
            const Index lag                        = candidate_reference_start - candidate_comparison_start;
            const f64   product_sum = reference_fft[static_cast<std::size_t>(reference_n - 1 + lag)].real();
            const f64   reference_sum =
                reference_prefix_sum[static_cast<std::size_t>(candidate_reference_start + length)] -
                reference_prefix_sum[static_cast<std::size_t>(candidate_reference_start)];
            const f64 comparison_sum =
                comparison_prefix_sum[static_cast<std::size_t>(candidate_comparison_start + length)] -
                comparison_prefix_sum[static_cast<std::size_t>(candidate_comparison_start)];
            const f64 reference_square_sum =
                reference_square_prefix_sum[static_cast<std::size_t>(candidate_reference_start + length)] -
                reference_square_prefix_sum[static_cast<std::size_t>(candidate_reference_start)];
            const f64 comparison_square_sum =
                comparison_square_prefix_sum[static_cast<std::size_t>(candidate_comparison_start + length)] -
                comparison_square_prefix_sum[static_cast<std::size_t>(candidate_comparison_start)];
            const f64  reference_mean_square  = reference_sum * reference_sum * inv_n;
            const f64  comparison_mean_square = comparison_sum * comparison_sum * inv_n;
            const f64  numerator              = product_sum - reference_sum * comparison_sum * inv_n;
            const f64  reference_var          = reference_square_sum - reference_mean_square;
            const f64  comparison_var         = comparison_square_sum - comparison_mean_square;
            const f64  reference_scale        = std::max(reference_square_sum, std::abs(reference_mean_square));
            const f64  comparison_scale       = std::max(comparison_square_sum, std::abs(comparison_mean_square));
            const f64  reference_tol  = std::numeric_limits<f64>::epsilon() * std::max(1.0, reference_scale) * 64.0;
            const f64  comparison_tol = std::numeric_limits<f64>::epsilon() * std::max(1.0, comparison_scale) * 64.0;
            const bool undefined_candidate = !(reference_var > reference_tol && comparison_var > comparison_tol);
            f64        candidate_rho       = 0.0;
            if (!undefined_candidate) {
                candidate_rho = numerator / std::sqrt(reference_var * comparison_var);
                if (candidate_rho > 1.0) {
                    candidate_rho = 1.0;
                } else if (candidate_rho < -1.0) {
                    candidate_rho = -1.0;
                }
            }

            if (candidate_rho > refined_ccr + CORRELATION_TIE_TOLERANCE) {
                refined_ccr             = candidate_rho;
                result.reference_start  = candidate_reference_start;
                result.comparison_start = candidate_comparison_start;
                result.length           = length;
                result.n_eps            = idx;
                result.rho_e            = candidate_rho;
                result.diagnostics.clear();
                if (undefined_candidate) {
                    result.diagnostics.push_back({DiagnosticSeverity::Warning, DiagnosticComponent::Phase,
                                                  DiagnosticCode::PhaseUndefinedCorrelation});
                }
            }
        }
    }

    if (result.length < 9) {
        result = unshifted_result;
        result.diagnostics.push_back(
            {DiagnosticSeverity::Warning, DiagnosticComponent::Phase, DiagnosticCode::PhaseShiftClampedToUnshifted});
    }

    if (result.n_eps == 0) {
        result.score = 1.0;
        return;
    }
    const f64 max_allowable_time_shift_threshold = static_cast<f64>(reference_n) * result.max_shift;
    const f64 n_eps                              = static_cast<f64>(result.n_eps);
    if (n_eps >= max_allowable_time_shift_threshold) {
        result.score = 0.0;
        return;
    }
    result.score =
        integer_power((max_allowable_time_shift_threshold - n_eps) / max_allowable_time_shift_threshold, params.k_p);
}

void corridor_score (CorridorResult& result, std::span<const f64> reference, std::span<const f64> comparison,
                     const ScoreParams& params) {
    const Index n      = span_size(reference);
    f64         t_norm = 0.0;
    for (Index idx = 0; idx < n; ++idx) {
        t_norm = std::max(t_norm, std::abs(value_at(reference, idx)));
    }

    const f64 inner_corridor = params.a_0 * t_norm;
    const f64 outer_corridor = params.b_0 * t_norm;

    result.t_norm           = t_norm;
    result.inner_half_width = inner_corridor;
    result.outer_half_width = outer_corridor;

    if (t_norm == 0.0) {
        f64 sum = 0.0;
        for (Index idx = 0; idx < n; ++idx) {
            if (value_at(reference, idx) == value_at(comparison, idx)) {
                sum += 1.0;
            }
        }
        result.score = sum / static_cast<f64>(n);
        return;
    }

    f64 sum = 0.0;
    for (Index idx = 0; idx < n; ++idx) {
        const f64 diff = std::abs(value_at(reference, idx) - value_at(comparison, idx));
        f64       c_i  = integer_power((outer_corridor - diff) / (outer_corridor - inner_corridor), params.k_z);
        if (diff < inner_corridor) {
            c_i = 1.0;
        }
        if (diff > outer_corridor) {
            c_i = 0.0;
        }
        sum += c_i;
    }
    result.score = sum / static_cast<f64>(n);
}

void magnitude_score (MagnitudeResult& result, std::span<const f64> reference_values,
                      std::span<const f64> comparison_values, const ScoreParams& params) {
    const std::pair<f64, f64> magnitude_error =
        magnitude_error_from_dtw(result, comparison_values, reference_values, 0.1);
    const f64 numerator   = magnitude_error.first;
    const f64 denominator = magnitude_error.second;

    result.denominator = denominator;
    result.numerator   = numerator;
    if (denominator == 0.0) {
        result.score = numerator == 0.0 ? 1.0 : 0.0;
        result.error = std::numeric_limits<f64>::quiet_NaN();
        result.diagnostics.push_back({DiagnosticSeverity::Warning, DiagnosticComponent::Magnitude,
                                      DiagnosticCode::MagnitudeZeroReferenceDenominator});
        return;
    }

    const f64 e_mag = numerator / denominator;
    result.error    = e_mag;
    if (e_mag == 0.0) {
        result.score = 1.0;
        return;
    }
    if (e_mag > params.eps_m) {
        result.score = 0.0;
        return;
    }
    result.score = integer_power((params.eps_m - e_mag) / params.eps_m, params.k_m);
}

void gradient_values (std::vector<f64>& gradient, std::span<const f64> values, f64 dt) {
    const Index n = span_size(values);
    gradient.assign(static_cast<std::size_t>(n), 0.0);
    gradient[0] = (value_at(values, 1) - value_at(values, 0)) / dt;
    for (Index idx = 1; idx < n - 1; ++idx) {
        gradient[static_cast<std::size_t>(idx)] = (value_at(values, idx + 1) - value_at(values, idx - 1)) / (2.0 * dt);
    }
    gradient[static_cast<std::size_t>(n - 1)] = (value_at(values, n - 1) - value_at(values, n - 2)) / dt;
}

f64 smoothed_slope_at (std::span<const f64> gradient, Index idx) {
    const Index n = static_cast<Index>(gradient.size());

    static constexpr std::array<Index, 4> windows = {1, 3, 5, 7};
    if (idx < 4) {
        const Index nr  = windows[offset(idx)];
        f64         sum = 0.0;
        for (Index j = 0; j < nr; ++j) {
            sum += gradient[static_cast<std::size_t>(j)];
        }
        return sum / static_cast<f64>(nr);
    }
    if (idx >= n - 4) {
        const Index edge_idx = n - idx - 1;
        const Index nr       = windows[offset(edge_idx)];
        f64         sum      = 0.0;
        for (Index j = 0; j < nr; ++j) {
            sum += gradient[static_cast<std::size_t>(n - nr + j)];
        }
        return sum / static_cast<f64>(nr);
    }

    f64 sum = 0.0;
    for (Index j = idx - 4; j <= idx + 4; ++j) {
        sum += gradient[static_cast<std::size_t>(j)];
    }
    return sum / 9.0;
}

void slope_score (SlopeResult& result, std::span<const f64> reference_values, std::span<const f64> comparison_values,
                  const ScoreParams& params, f64 dt) {
    const Index n = span_size(reference_values);
    if (n < 9) {
        throw std::invalid_argument("Shifted curves must have at least 9 samples for slope rating");
    }

    std::vector<f64> comparison_gradient;
    std::vector<f64> reference_gradient;
    gradient_values(comparison_gradient, comparison_values, dt);
    gradient_values(reference_gradient, reference_values, dt);

    f64 numerator   = 0.0;
    f64 denominator = 0.0;

    for (Index idx = 0; idx < n; ++idx) {
        const f64 comparison_smoothed = smoothed_slope_at(std::span<const f64>(comparison_gradient), idx);
        const f64 reference_smoothed  = smoothed_slope_at(std::span<const f64>(reference_gradient), idx);
        numerator += std::abs(comparison_smoothed - reference_smoothed);
        denominator += std::abs(reference_smoothed);
    }

    result.denominator = denominator;
    result.numerator   = numerator;

    if (denominator == 0.0) {
        result.score = numerator == 0.0 ? 1.0 : 0.0;
        result.error = std::numeric_limits<f64>::quiet_NaN();
        result.diagnostics.push_back(
            {DiagnosticSeverity::Warning, DiagnosticComponent::Slope, DiagnosticCode::SlopeZeroReferenceDenominator});
        return;
    }

    const f64 e_slope = numerator / denominator;
    result.error      = e_slope;
    if (e_slope <= 0.0) {
        result.score = 1.0;
        return;
    }
    if (e_slope >= params.e_s) {
        result.score = 0.0;
        return;
    }
    // result.score = integer_power((params.e_s - e_slope) / params.e_s, params.k_s);
    result.score = (params.e_s - e_slope) / params.e_s;
}

ScoreResult score_components_impl (std::span<const f64> reference, std::span<const f64> comparison,
                                   const ScoreParams& params, f64 dt) {
    ScoreResult result;
    corridor_score(result.corridor, reference, comparison, params);
    phase_score(result.phase, reference, comparison, params);

    const std::span<const f64> aligned_comparison =
        comparison.subspan(offset(result.phase.comparison_start), offset(result.phase.length));
    const std::span<const f64> aligned_reference =
        reference.subspan(offset(result.phase.reference_start), offset(result.phase.length));

    magnitude_score(result.magnitude, aligned_reference, aligned_comparison, params);
    slope_score(result.slope, aligned_reference, aligned_comparison, params, dt);
    result.overall = params.w_z * result.corridor.score + params.w_p * result.phase.score +
                     params.w_m * result.magnitude.score + params.w_s * result.slope.score;

    return result;
}

} // namespace

namespace engine {

ScoreResult VARIANT (score_components)(std::span<const f64> reference, std::span<const f64> comparison,
                                       const ScoreParams& params, f64 dt) {
    return score_components_impl(reference, comparison, params, dt);
}

} // namespace engine
