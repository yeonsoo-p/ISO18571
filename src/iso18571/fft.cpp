#include "fft.h"
#include "dispatch.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

#ifndef IMPL_SUFFIX
#error "IMPL_SUFFIX must be defined before including fft.cpp"
#endif

namespace {

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

void sincos_2pi_by_n_init (Sincos2PiByN& table, size_t n) {
    constexpr long double pi   = 3.141592653589793238462643383279502884197L;
    const double          ang  = static_cast<double>(0.25L * pi / static_cast<long double>(n));
    const size_t          nval = (n + 2) / 2;
    table.length               = n;
    table.shift                = 1;
    while ((size_t(1) << table.shift) * (size_t(1) << table.shift) < nval) {
        ++table.shift;
    }
    table.mask = (size_t(1) << table.shift) - 1;
    table.v1.reserve(table.mask + 1);
    table.v1.push_back({1.0, 0.0});
    for (size_t i = 1; i < table.mask + 1; ++i) {
        table.v1.push_back(sincos_2pi_by_n_calc(i, n, ang));
    }
    const size_t v2_size = (nval + table.mask) / (table.mask + 1);
    table.v2.reserve(v2_size);
    table.v2.push_back({1.0, 0.0});
    for (size_t i = 1; i < v2_size; ++i) {
        table.v2.push_back(sincos_2pi_by_n_calc(i * (table.mask + 1), n, ang));
    }
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

void fft_plan_factorize (FftPlan& plan) {
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
}

size_t fft_plan_twiddle_size (const FftPlan& plan) {
    size_t twiddle_size = 0, l1 = 1;
    for (size_t k = 0; k < plan.fact.size(); ++k) {
        const size_t ip  = plan.fact[k].fct;
        const size_t ido = plan.length / (l1 * ip);
        twiddle_size += (ip - 1) * (ido - 1);
        l1 *= ip;
    }
    return twiddle_size;
}

void fft_plan_compute_twiddles (FftPlan& plan) {
    Sincos2PiByN twiddle = {};
    sincos_2pi_by_n_init(twiddle, plan.length);
    size_t l1 = 1;
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
}

void fft_plan_init (FftPlan& plan, size_t length) {
    plan.length = length;
    if (plan.length == 0) {
        throw std::runtime_error("zero-length FFT requested");
    }
    if (plan.length == 1) {
        return;
    }
    fft_plan_factorize(plan);
    plan.mem.reserve(fft_plan_twiddle_size(plan));
    fft_plan_compute_twiddles(plan);
}

template<bool fwd>
void fft_plan_pass_all (const FftPlan& plan, Complex c[], double fct) {
    if (plan.length == 1) {
        c[0] *= fct;
        return;
    }
    size_t               l1 = 1;
    std::vector<Complex> ch(plan.length);
    Complex*             p1 = c;
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
    if (p1 != c) {
        if (fct != 1.) {
            for (size_t i = 0; i < plan.length; ++i) {
                c[i] = ch[i] * fct;
            }
        } else {
            std::copy_n(p1, plan.length, c);
        }
    } else if (fct != 1.) {
        for (size_t i = 0; i < plan.length; ++i) {
            c[i] *= fct;
        }
    }
}

void fft_plan_exec (const FftPlan& plan, Complex c[], double fct, bool fwd) {
    fwd ? fft_plan_pass_all<true>(plan, c, fct) : fft_plan_pass_all<false>(plan, c, fct);
}

} // namespace

namespace fft {

void VARIANT (c2c_power_of_two)(Complex* data, std::size_t length, bool forward, double fct) {
    if (length == 0)
        return;
    if ((length & (length - 1U)) != 0)
        throw std::invalid_argument("complex FFT length must be a power of two");
    FftPlan plan = {};
    fft_plan_init(plan, length);
    fft_plan_exec(plan, data, fct, forward);
}

} // namespace fft
