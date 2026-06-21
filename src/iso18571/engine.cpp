#include "engine.h"
#include "dispatch.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#if defined(ISO18571_ENGINE_DISPATCH)

namespace engine {

namespace {

DispatchTable make_dispatch_table () {
    switch (dispatch::best_x86_64_level(dispatch::compiled_levels())) {
    case dispatch::X86_64Level::V4:
#if defined(ISO18571_COMPILED_X86_64_V4)
        return {score_components_v4, dispatch::level_name(dispatch::X86_64Level::V4)};
#else
        break;
#endif
    case dispatch::X86_64Level::V3:
#if defined(ISO18571_COMPILED_X86_64_V3)
        return {score_components_v3, dispatch::level_name(dispatch::X86_64Level::V3)};
#else
        break;
#endif
    case dispatch::X86_64Level::V2:
#if defined(ISO18571_COMPILED_X86_64_V2)
        return {score_components_v2, dispatch::level_name(dispatch::X86_64Level::V2)};
#else
        break;
#endif
    case dispatch::X86_64Level::V1:
        return {score_components_v1, dispatch::level_name(dispatch::X86_64Level::V1)};
    }
    return {score_components_v1, dispatch::level_name(dispatch::X86_64Level::V1)};
}

} // namespace

const DispatchTable& dispatch_table () {
    static const DispatchTable table = make_dispatch_table();
    return table;
}

} // namespace engine

#else

#ifndef IMPL_SUFFIX
#error "IMPL_SUFFIX must be defined before including engine.cpp"
#endif

namespace {

using engine::Diagnostic;
using engine::DiagnosticCode;
using engine::DiagnosticComponent;
using engine::DiagnosticSeverity;
using engine::DoubleSpan;
using engine::Index;
using engine::MagnitudeResult;
using engine::PhaseAlignment;
using engine::PhaseResult;
using engine::ScoreParams;
using engine::ScoreResult;
using engine::SlopeResult;
using fft::Complex;
using std::size_t;

inline void sum_difference (Complex& a, Complex& b, Complex c, Complex d) {
    a = c + d;
    b = c - d;
}

inline void sum_difference_in_place (Complex& a, Complex& b) {
    Complex t = a;
    a += b;
    b = t - b;
}

template<bool fwd>
void special_mul (const Complex& v1, const Complex& v2, Complex& res) {
    res = fwd ? Complex(v1.real() * v2.real() + v1.imag() * v2.imag(), v1.imag() * v2.real() - v1.real() * v2.imag())
              : Complex(v1.real() * v2.real() - v1.imag() * v2.imag(), v1.real() * v2.imag() + v1.imag() * v2.real());
}

template<bool fwd>
void rotate_x90 (Complex& a) {
    const double tmp_ = fwd ? -a.real() : a.real();
    a.real(fwd ? a.imag() : -a.imag());
    a.imag(tmp_);
}

struct Sincos2PiByN {
    size_t               length;
    size_t               mask;
    size_t               shift;
    std::vector<Complex> v1, v2;
};

Complex sincos_2pi_by_n_calc (size_t x, size_t n, double ang) {
    x <<= 3;
    if (x < 4 * n) {
        if (x < 2 * n) {
            if (x < n) {
                return {std::cos(static_cast<double>(x) * ang), std::sin(static_cast<double>(x) * ang)};
            }
            return {std::sin(static_cast<double>(2 * n - x) * ang), std::cos(static_cast<double>(2 * n - x) * ang)};
        }
        x -= 2 * n;
        if (x < n) {
            return {-std::sin(static_cast<double>(x) * ang), std::cos(static_cast<double>(x) * ang)};
        }
        return {-std::cos(static_cast<double>(2 * n - x) * ang), std::sin(static_cast<double>(2 * n - x) * ang)};
    }
    x = 8 * n - x;
    if (x < 2 * n) {
        if (x < n) {
            return {std::cos(static_cast<double>(x) * ang), -std::sin(static_cast<double>(x) * ang)};
        }
        return {std::sin(static_cast<double>(2 * n - x) * ang), -std::cos(static_cast<double>(2 * n - x) * ang)};
    }
    x -= 2 * n;
    if (x < n) {
        return {-std::sin(static_cast<double>(x) * ang), -std::cos(static_cast<double>(x) * ang)};
    }
    return {-std::cos(static_cast<double>(2 * n - x) * ang), -std::sin(static_cast<double>(2 * n - x) * ang)};
}

Complex sincos_2pi_by_n_lookup (const Sincos2PiByN& table, size_t idx) {
    if (2 * idx <= table.length) {
        const Complex product = table.v1[idx & table.mask] * table.v2[idx >> table.shift];
        return {product.real(), product.imag()};
    }
    idx                   = table.length - idx;
    const Complex product = table.v1[idx & table.mask] * table.v2[idx >> table.shift];
    return {product.real(), -product.imag()};
}

struct FactorData {
    size_t fct;
    size_t tw_offset;
};

struct FftPlan {
    size_t                  length;
    std::vector<Complex>    mem;
    std::vector<FactorData> fact;
};

void fft_plan_add_factor (FftPlan& plan, size_t factor) { plan.fact.push_back({factor, 0}); }

const Complex* fft_plan_twiddle_data (const FftPlan& plan, const FactorData& factor) {
    return factor.tw_offset < plan.mem.size() ? plan.mem.data() + factor.tw_offset : nullptr;
}

Complex& ch_at (Complex* ch, size_t ido, size_t l1, size_t a, size_t b, size_t c) { return ch[a + ido * (b + l1 * c)]; }

const Complex& cc_at (const Complex* cc, size_t ido, size_t radix, size_t a, size_t b, size_t c) {
    return cc[a + ido * (b + radix * c)];
}

const Complex& wa_at (const Complex* wa, size_t ido, size_t x, size_t i) { return wa[i - 1 + x * (ido - 1)]; }

template<bool fwd>
void pass2 (size_t ido, size_t l1, const Complex* cc, Complex* ch, const Complex* wa) {
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
void pass4 (size_t ido, size_t l1, const Complex* cc, Complex* ch, const Complex* wa) {
    if (ido == 1) {
        for (size_t k = 0; k < l1; ++k) {
            Complex t1, t2, t3, t4;
            sum_difference(t2, t1, cc_at(cc, ido, 4, 0, 0, k), cc_at(cc, ido, 4, 0, 2, k));
            sum_difference(t3, t4, cc_at(cc, ido, 4, 0, 1, k), cc_at(cc, ido, 4, 0, 3, k));
            rotate_x90<fwd>(t4);
            sum_difference(ch_at(ch, ido, l1, 0, k, 0), ch_at(ch, ido, l1, 0, k, 2), t2, t3);
            sum_difference(ch_at(ch, ido, l1, 0, k, 1), ch_at(ch, ido, l1, 0, k, 3), t1, t4);
        }
    } else {
        for (size_t k = 0; k < l1; ++k) {
            {
                Complex t1, t2, t3, t4;
                sum_difference(t2, t1, cc_at(cc, ido, 4, 0, 0, k), cc_at(cc, ido, 4, 0, 2, k));
                sum_difference(t3, t4, cc_at(cc, ido, 4, 0, 1, k), cc_at(cc, ido, 4, 0, 3, k));
                rotate_x90<fwd>(t4);
                sum_difference(ch_at(ch, ido, l1, 0, k, 0), ch_at(ch, ido, l1, 0, k, 2), t2, t3);
                sum_difference(ch_at(ch, ido, l1, 0, k, 1), ch_at(ch, ido, l1, 0, k, 3), t1, t4);
            }
            for (size_t i = 1; i < ido; ++i) {
                Complex       t1, t2, t3, t4;
                const Complex cc0 = cc_at(cc, ido, 4, i, 0, k);
                const Complex cc1 = cc_at(cc, ido, 4, i, 1, k);
                const Complex cc2 = cc_at(cc, ido, 4, i, 2, k);
                const Complex cc3 = cc_at(cc, ido, 4, i, 3, k);
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
void rotate_x45 (Complex& a) {
    constexpr double hsqt2 = 0.707106781186547524400844362104849;
    if (fwd) {
        const double tmp_ = a.real();
        a.real(hsqt2 * (a.real() + a.imag()));
        a.imag(hsqt2 * (a.imag() - tmp_));
    } else {
        const double tmp_ = a.real();
        a.real(hsqt2 * (a.real() - a.imag()));
        a.imag(hsqt2 * (a.imag() + tmp_));
    }
}

template<bool fwd>
void rotate_x135 (Complex& a) {
    constexpr double hsqt2 = 0.707106781186547524400844362104849;
    if (fwd) {
        const double tmp_ = a.real();
        a.real(hsqt2 * (a.imag() - a.real()));
        a.imag(hsqt2 * (-tmp_ - a.imag()));
    } else {
        const double tmp_ = a.real();
        a.real(hsqt2 * (-a.real() - a.imag()));
        a.imag(hsqt2 * (tmp_ - a.imag()));
    }
}

template<bool fwd>
void pass8 (size_t ido, size_t l1, const Complex* cc, Complex* ch, const Complex* wa) {
    if (ido == 1) {
        for (size_t k = 0; k < l1; ++k) {
            Complex a0, a1, a2, a3, a4, a5, a6, a7;
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
                Complex a0, a1, a2, a3, a4, a5, a6, a7;
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
                Complex a0, a1, a2, a3, a4, a5, a6, a7;
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

FftPlan fft_plan_init (std::size_t length) {
    if (length == 0) {
        return {};
    }
    if ((length & (length - 1U)) != 0) {
        throw std::invalid_argument("complex FFT length must be a power of two");
    }
    FftPlan plan = {};
    plan.length  = length;
    if (plan.length == 1) {
        return plan;
    }

    size_t len = plan.length;
    while ((len & 7) == 0) {
        fft_plan_add_factor(plan, 8);
        len >>= 3;
    }
    while ((len & 3) == 0) {
        fft_plan_add_factor(plan, 4);
        len >>= 2;
    }
    if ((len & 1) == 0) {
        len >>= 1;
        // Factor 2 should be at the front of the factor list.
        fft_plan_add_factor(plan, 2);
        std::swap(plan.fact[0].fct, plan.fact.back().fct);
    }
    if (len != 1) {
        throw std::invalid_argument("complex FFT length must be a power of two");
    }

    size_t twiddle_size = 0;
    size_t l1           = 1;
    for (size_t k = 0; k < plan.fact.size(); ++k) {
        const size_t ip  = plan.fact[k].fct;
        const size_t ido = plan.length / (l1 * ip);
        twiddle_size += (ip - 1) * (ido - 1);
        l1 *= ip;
    }
    plan.mem.reserve(twiddle_size);

    Sincos2PiByN          twiddle = {};
    constexpr long double pi      = 3.141592653589793238462643383279502884197L;
    const double          ang     = static_cast<double>(0.25L * pi / static_cast<long double>(plan.length));
    const size_t          nval    = (plan.length + 2) / 2;
    twiddle.length                = plan.length;
    twiddle.shift                 = 1;
    while ((size_t(1) << twiddle.shift) * (size_t(1) << twiddle.shift) < nval) {
        ++twiddle.shift;
    }
    twiddle.mask = (size_t(1) << twiddle.shift) - 1;
    twiddle.v1.reserve(twiddle.mask + 1);
    twiddle.v1.push_back({1.0, 0.0});
    for (size_t i = 1; i < twiddle.mask + 1; ++i) {
        twiddle.v1.push_back(sincos_2pi_by_n_calc(i, plan.length, ang));
    }
    const size_t v2_size = (nval + twiddle.mask) / (twiddle.mask + 1);
    twiddle.v2.reserve(v2_size);
    twiddle.v2.push_back({1.0, 0.0});
    for (size_t i = 1; i < v2_size; ++i) {
        twiddle.v2.push_back(sincos_2pi_by_n_calc(i * (twiddle.mask + 1), plan.length, ang));
    }

    l1 = 1;
    for (size_t k = 0; k < plan.fact.size(); ++k) {
        const size_t ip        = plan.fact[k].fct;
        const size_t ido       = plan.length / (l1 * ip);
        plan.fact[k].tw_offset = plan.mem.size();
        for (size_t j = 1; j < ip; ++j) {
            for (size_t i = 1; i < ido; ++i) {
                plan.mem.push_back(sincos_2pi_by_n_lookup(twiddle, j * l1 * i));
            }
        }
        l1 *= ip;
    }

    return plan;
}

template<bool fwd>
void fft_plan_exec (const FftPlan& plan, Complex* data, double fct) {
    if (plan.length == 0) {
        return;
    }
    if (plan.length == 1) {
        data[0] *= fct;
        return;
    }

    size_t               l1 = 1;
    std::vector<Complex> ch(plan.length);
    Complex*             p1 = data;
    Complex*             p2 = ch.data();

    for (size_t k1 = 0; k1 < plan.fact.size(); k1++) {
        const FactorData& factor = plan.fact[k1];
        const size_t      ip     = factor.fct;
        const size_t      l2     = ip * l1;
        const size_t      ido    = plan.length / l2;
        if (ip == 4) {
            pass4<fwd>(ido, l1, p1, p2, fft_plan_twiddle_data(plan, factor));
        } else if (ip == 8) {
            pass8<fwd>(ido, l1, p1, p2, fft_plan_twiddle_data(plan, factor));
        } else if (ip == 2) {
            pass2<fwd>(ido, l1, p1, p2, fft_plan_twiddle_data(plan, factor));
        } else {
            throw std::logic_error("unexpected non-power-of-two FFT factor");
        }
        std::swap(p1, p2);
        l1 = l2;
    }
    if (p1 != data) {
        if (fct != 1.) {
            for (size_t i = 0; i < plan.length; ++i) {
                data[i] = ch[i] * fct;
            }
        } else {
            std::copy_n(p1, plan.length, data);
        }
    } else if (fct != 1.) {
        for (size_t i = 0; i < plan.length; ++i) {
            data[i] *= fct;
        }
    }
}

constexpr double CORRELATION_TIE_TOLERANCE = 1.0e-12;
constexpr double CORRELATION_REFINE_MARGIN = 1.0e-9;

std::size_t offset (Index index) { return static_cast<std::size_t>(index); }

Index span_size (DoubleSpan values) { return static_cast<Index>(values.size()); }

double value_at (DoubleSpan values, Index index) { return values[offset(index)]; }

double integer_power (double value, int exponent) {
    if (exponent == 1) {
        return value;
    }
    if (exponent == 2) {
        return value * value;
    }
    if (exponent == 3) {
        return value * value * value;
    }

    double result = 1.0;
    double base   = value;
    int    power  = exponent;
    while (power > 0) {
        if ((power & 1) != 0) {
            result *= base;
        }
        base *= base;
        power >>= 1;
    }
    return result;
}

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

void select_dtw_predecessor (double cost, double candidate_numerator, double candidate_denominator,
                             double& best_previous, double& best_numerator, double& best_denominator) {
    best_previous    = cost;
    best_numerator   = candidate_numerator;
    best_denominator = candidate_denominator;
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
    const Index raw = static_cast<Index>(std::ceil(window_size * static_cast<double>(n)));
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
    std::vector<double> abs_y(static_cast<std::size_t>(n), 0.0);

    for (Index idx = 0; idx < n; ++idx) {
        abs_y[offset(idx)] = std::abs(value_at(y, idx));
    }

    for (Index i = 0; i < n; ++i) {
        const Index  previous_start = i > 0 ? std::max<Index>(0, i - radius) : 0;
        const Index  previous_stop  = i > 0 ? std::min<Index>(n, i + radius - 1) : 0;
        const Index  j_start        = std::max<Index>(0, i - radius + 1);
        const Index  j_stop         = std::min<Index>(n, i + radius);
        const double x_i            = value_at(x, i);
        Index        interior_start = j_stop;
        Index        interior_stop  = j_stop;

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
            const double      delta             = x_i - value_at(y, j);
            const double      local_cost        = delta * delta;
            const double      local_numerator   = std::abs(delta);
            const double      local_denominator = abs_y[index];
            double            accumulated       = inf;
            double            numerator         = 0.0;
            double            denominator       = 0.0;

            if (i == 0 && j == 0) {
                accumulated = local_cost;
                numerator   = local_numerator;
                denominator = local_denominator;
            } else {
                double best_previous    = inf;
                double best_numerator   = 0.0;
                double best_denominator = 0.0;

                if (i > 0 && j >= previous_start && j < previous_stop) {
                    select_dtw_predecessor(previous_cost[index], previous_numerator[index], previous_denominator[index],
                                           best_previous, best_numerator, best_denominator);
                }
                if (j > j_start) {
                    const std::size_t previous_index = static_cast<std::size_t>(j - 1);
                    const double      candidate      = current_cost[previous_index];
                    if (candidate < best_previous) {
                        select_dtw_predecessor(candidate, current_numerator[previous_index],
                                               current_denominator[previous_index], best_previous, best_numerator,
                                               best_denominator);
                    }
                }
                if (i > 0 && j > 0 && j - 1 >= previous_start && j - 1 < previous_stop) {
                    const std::size_t previous_index = static_cast<std::size_t>(j - 1);
                    const double      candidate      = previous_cost[previous_index];
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
            const double      delta             = x_i - value_at(y, j);
            const double      local_cost        = delta * delta;
            const double      local_numerator   = std::abs(delta);
            const double      local_denominator = abs_y[index];
            double            best_previous     = previous_cost[index];
            double            best_numerator    = previous_numerator[index];
            double            best_denominator  = previous_denominator[index];
            const double      horizontal        = current_cost[previous_index];
            if (horizontal < best_previous) {
                best_previous    = horizontal;
                best_numerator   = current_numerator[previous_index];
                best_denominator = current_denominator[previous_index];
            }
            const double diagonal = previous_cost[previous_index];
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
            const double      delta             = x_i - value_at(y, j);
            const double      local_cost        = delta * delta;
            const double      local_numerator   = std::abs(delta);
            const double      local_denominator = abs_y[index];
            double            accumulated       = inf;
            double            numerator         = 0.0;
            double            denominator       = 0.0;

            if (i == 0 && j == 0) {
                accumulated = local_cost;
                numerator   = local_numerator;
                denominator = local_denominator;
            } else {
                double best_previous    = inf;
                double best_numerator   = 0.0;
                double best_denominator = 0.0;

                if (i > 0 && j >= previous_start && j < previous_stop) {
                    select_dtw_predecessor(previous_cost[index], previous_numerator[index], previous_denominator[index],
                                           best_previous, best_numerator, best_denominator);
                }
                if (j > j_start) {
                    const std::size_t previous_index = static_cast<std::size_t>(j - 1);
                    const double      candidate      = current_cost[previous_index];
                    if (candidate < best_previous) {
                        select_dtw_predecessor(candidate, current_numerator[previous_index],
                                               current_denominator[previous_index], best_previous, best_numerator,
                                               best_denominator);
                    }
                }
                if (i > 0 && j > 0 && j - 1 >= previous_start && j - 1 < previous_stop) {
                    const std::size_t previous_index = static_cast<std::size_t>(j - 1);
                    const double      candidate      = previous_cost[previous_index];
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

std::size_t next_power_of_two (std::size_t value) {
    std::size_t out = 1;
    while (out < value) {
        out <<= 1U;
    }
    return out;
}

PhaseProductSums fft_product_sums (DoubleSpan reference, DoubleSpan comparison) {
    const std::size_t n         = static_cast<std::size_t>(span_size(reference));
    const std::size_t conv_size = 2U * n - 1U;
    const std::size_t fft_size  = next_power_of_two(conv_size);
    const FftPlan     fft_plan  = fft_plan_init(fft_size);

    std::vector<Complex> reference_fft(fft_size);
    std::vector<Complex> comparison_fft(fft_size);
    for (std::size_t idx = 0; idx < n; ++idx) {
        reference_fft[idx]  = {value_at(reference, static_cast<Index>(idx)), 0.0};
        comparison_fft[idx] = {value_at(comparison, static_cast<Index>(n - idx - 1U)), 0.0};
    }

    fft_plan_exec<fft::kForward>(fft_plan, reference_fft.data(), 1.0);
    fft_plan_exec<fft::kForward>(fft_plan, comparison_fft.data(), 1.0);
    for (std::size_t idx = 0; idx < fft_size; ++idx) {
        reference_fft[idx] *= comparison_fft[idx];
    }
    fft_plan_exec<fft::kBackward>(fft_plan, reference_fft.data(), 1.0 / static_cast<double>(fft_size));

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
                                              Index comparison_start, Index length, Index n_eps, double max_shift,
                                              double& cached_rho) {
    cached_rho = std::numeric_limits<double>::quiet_NaN();
    if (length < 32) {
        return phase_candidate_for_shift(reference, comparison, reference_start, comparison_start, length, n_eps,
                                         max_shift);
    }
    const double product_sum = product_sum_from_fft(sums, n, reference_start, comparison_start);
    const double rho_e = correlation_from_cached_product(cache, reference_start, comparison_start, length, product_sum);
    cached_rho         = rho_e;
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

PhaseResult refine_fft_phase_result (DoubleSpan reference, DoubleSpan comparison, Index bounded_window_size,
                                     double max_shift, double fft_ccr_max, const std::vector<double>& left_cached_rho,
                                     const std::vector<double>& right_cached_rho) {
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

        const double left = left_cached_rho[offset(idx)];
        if (left >= fft_ccr_max - CORRELATION_REFINE_MARGIN) {
            select_phase_candidate(refined, refined_ccr,
                                   phase_candidate_for_shift(reference, comparison, 0, idx, length, idx, max_shift));
        }

        const double right = right_cached_rho[offset(idx)];
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
    std::vector<double>    left_cached_rho(static_cast<std::size_t>(bounded_window_size),
                                           std::numeric_limits<double>::quiet_NaN());
    std::vector<double>    right_cached_rho(static_cast<std::size_t>(bounded_window_size),
                                            std::numeric_limits<double>::quiet_NaN());
    for (Index idx = 1; idx < bounded_window_size; ++idx) {
        const Index       length = reference_n - idx;
        const PhaseResult left_candidate =
            phase_candidate_from_fft_product(reference, comparison, cache, sums, reference_n, 0, idx, length, idx,
                                             max_shift, left_cached_rho[offset(idx)]);
        select_phase_candidate(result, ccr_max, left_candidate);

        const PhaseResult right_candidate =
            phase_candidate_from_fft_product(reference, comparison, cache, sums, reference_n, idx, 0, length, idx,
                                             max_shift, right_cached_rho[offset(idx)]);
        select_phase_candidate(result, ccr_max, right_candidate);
    }

    result = refine_fft_phase_result(reference, comparison, bounded_window_size, max_shift, ccr_max, left_cached_rho,
                                     right_cached_rho);

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
        double       c_i  = integer_power((outer_corridor - diff) / (outer_corridor - inner_corridor), params.k_z);
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
    return integer_power((max_allowable_time_shift_threshold - std::abs(static_cast<double>(alignment.n_eps))) /
                             max_allowable_time_shift_threshold,
                         params.k_p);
}

MagnitudeResult magnitude_score_from_values (DoubleSpan reference_values, DoubleSpan comparison_values,
                                             const ScoreParams& params) {
    const std::pair<double, double> magnitude_error =
        magnitude_error_from_dtw(comparison_values, reference_values, 0.1);
    const double numerator   = magnitude_error.first;
    const double denominator = magnitude_error.second;
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
        integer_power((params.eps_m - e_mag) / params.eps_m, params.k_m),
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

namespace engine {

ScoreResult VARIANT (score_components)(DoubleSpan reference, DoubleSpan comparison, const ScoreParams& params,
                                       double dt) {
    return score_components_impl(reference, comparison, params, dt);
}

} // namespace engine

#endif
