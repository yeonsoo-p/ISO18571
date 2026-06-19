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

inline void PM (Complex& a, Complex& b, Complex c, Complex d) {
    a = c + d;
    b = c - d;
}

inline void PMINPLACE (Complex& a, Complex& b) {
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
void ROTX90 (Complex& a) {
    const auto tmp_ = fwd ? -a.real() : a.real();
    a.real(fwd ? a.imag() : -a.imag());
    a.imag(tmp_);
}

class sincos_2pibyn {
  private:
    size_t               N, mask, shift;
    std::vector<Complex> v1, v2;

    static Complex calc (size_t x, size_t n, double ang) {
        x <<= 3;
        if (x < 4 * n) {
            if (x < 2 * n) {
                if (x < n)
                    return {std::cos(static_cast<double>(x) * ang), std::sin(static_cast<double>(x) * ang)};
                return {std::sin(static_cast<double>(2 * n - x) * ang), std::cos(static_cast<double>(2 * n - x) * ang)};
            }
            x -= 2 * n;
            if (x < n)
                return {-std::sin(static_cast<double>(x) * ang), std::cos(static_cast<double>(x) * ang)};
            return {-std::cos(static_cast<double>(2 * n - x) * ang), std::sin(static_cast<double>(2 * n - x) * ang)};
        }
        x = 8 * n - x;
        if (x < 2 * n) {
            if (x < n)
                return {std::cos(static_cast<double>(x) * ang), -std::sin(static_cast<double>(x) * ang)};
            return {std::sin(static_cast<double>(2 * n - x) * ang), -std::cos(static_cast<double>(2 * n - x) * ang)};
        }
        x -= 2 * n;
        if (x < n)
            return {-std::sin(static_cast<double>(x) * ang), -std::cos(static_cast<double>(x) * ang)};
        return {-std::cos(static_cast<double>(2 * n - x) * ang), -std::sin(static_cast<double>(2 * n - x) * ang)};
    }

  public:
    sincos_2pibyn (size_t n): N(n) {
        constexpr auto pi   = 3.141592653589793238462643383279502884197L;
        double         ang  = static_cast<double>(0.25L * pi / n);
        size_t         nval = (n + 2) / 2;
        shift               = 1;
        while ((size_t(1) << shift) * (size_t(1) << shift) < nval)
            ++shift;
        mask = (size_t(1) << shift) - 1;
        v1.reserve(mask + 1);
        v1.push_back({1.0, 0.0});
        for (size_t i = 1; i < mask + 1; ++i)
            v1.push_back(calc(i, n, ang));
        const size_t v2_size = (nval + mask) / (mask + 1);
        v2.reserve(v2_size);
        v2.push_back({1.0, 0.0});
        for (size_t i = 1; i < v2_size; ++i)
            v2.push_back(calc(i * (mask + 1), n, ang));
    }

    Complex operator [](size_t idx) const {
        if (2 * idx <= N) {
            const auto product = v1[idx & mask] * v2[idx >> shift];
            return {product.real(), product.imag()};
        }
        idx                = N - idx;
        const auto product = v1[idx & mask] * v2[idx >> shift];
        return {product.real(), -product.imag()};
    }
};

class cfftp {
  private:
    struct fctdata {
        size_t fct;
        size_t tw_offset;
    };

    size_t               length;
    std::vector<Complex> mem;
    std::vector<fctdata> fact;

    void add_factor (size_t factor) { fact.push_back({factor, 0}); }

    const Complex* twiddle_data (const fctdata& factor) const {
        return factor.tw_offset < mem.size() ? mem.data() + factor.tw_offset : nullptr;
    }

    template<bool fwd>
    void pass2 (size_t ido, size_t l1, const Complex* cc, Complex* ch, const Complex* wa) const {
        auto CH = [ch, ido, l1] (size_t a, size_t b, size_t c) -> Complex& { return ch[a + ido * (b + l1 * c)]; };
        auto CC = [cc, ido] (size_t a, size_t b, size_t c) -> const Complex& { return cc[a + ido * (b + 2 * c)]; };
        auto WA = [wa, ido] (size_t x, size_t i) { return wa[i - 1 + x * (ido - 1)]; };

        if (ido == 1)
            for (size_t k = 0; k < l1; ++k) {
                CH(0, k, 0) = CC(0, 0, k) + CC(0, 1, k);
                CH(0, k, 1) = CC(0, 0, k) - CC(0, 1, k);
            }
        else
            for (size_t k = 0; k < l1; ++k) {
                CH(0, k, 0) = CC(0, 0, k) + CC(0, 1, k);
                CH(0, k, 1) = CC(0, 0, k) - CC(0, 1, k);
                for (size_t i = 1; i < ido; ++i) {
                    CH(i, k, 0) = CC(i, 0, k) + CC(i, 1, k);
                    special_mul<fwd>(CC(i, 0, k) - CC(i, 1, k), WA(0, i), CH(i, k, 1));
                }
            }
    }

    template<bool fwd>
    void pass4 (size_t ido, size_t l1, const Complex* cc, Complex* ch, const Complex* wa) const {
        auto CH = [ch, ido, l1] (size_t a, size_t b, size_t c) -> Complex& { return ch[a + ido * (b + l1 * c)]; };
        auto CC = [cc, ido] (size_t a, size_t b, size_t c) -> const Complex& { return cc[a + ido * (b + 4 * c)]; };
        auto WA = [wa, ido] (size_t x, size_t i) { return wa[i - 1 + x * (ido - 1)]; };

        if (ido == 1)
            for (size_t k = 0; k < l1; ++k) {
                Complex t1, t2, t3, t4;
                PM(t2, t1, CC(0, 0, k), CC(0, 2, k));
                PM(t3, t4, CC(0, 1, k), CC(0, 3, k));
                ROTX90<fwd>(t4);
                PM(CH(0, k, 0), CH(0, k, 2), t2, t3);
                PM(CH(0, k, 1), CH(0, k, 3), t1, t4);
            }
        else
            for (size_t k = 0; k < l1; ++k) {
                {
                    Complex t1, t2, t3, t4;
                    PM(t2, t1, CC(0, 0, k), CC(0, 2, k));
                    PM(t3, t4, CC(0, 1, k), CC(0, 3, k));
                    ROTX90<fwd>(t4);
                    PM(CH(0, k, 0), CH(0, k, 2), t2, t3);
                    PM(CH(0, k, 1), CH(0, k, 3), t1, t4);
                }
                for (size_t i = 1; i < ido; ++i) {
                    Complex t1, t2, t3, t4;
                    Complex cc0 = CC(i, 0, k), cc1 = CC(i, 1, k), cc2 = CC(i, 2, k), cc3 = CC(i, 3, k);
                    PM(t2, t1, cc0, cc2);
                    PM(t3, t4, cc1, cc3);
                    ROTX90<fwd>(t4);
                    CH(i, k, 0) = t2 + t3;
                    special_mul<fwd>(t1 + t4, WA(0, i), CH(i, k, 1));
                    special_mul<fwd>(t2 - t3, WA(1, i), CH(i, k, 2));
                    special_mul<fwd>(t1 - t4, WA(2, i), CH(i, k, 3));
                }
            }
    }

    template<bool fwd>
    void ROTX45 (Complex& a) const {
        constexpr double hsqt2 = 0.707106781186547524400844362104849;
        if (fwd) {
            const auto tmp_ = a.real();
            a.real(hsqt2 * (a.real() + a.imag()));
            a.imag(hsqt2 * (a.imag() - tmp_));
        } else {
            const auto tmp_ = a.real();
            a.real(hsqt2 * (a.real() - a.imag()));
            a.imag(hsqt2 * (a.imag() + tmp_));
        }
    }
    template<bool fwd>
    void ROTX135 (Complex& a) const {
        constexpr double hsqt2 = 0.707106781186547524400844362104849;
        if (fwd) {
            const auto tmp_ = a.real();
            a.real(hsqt2 * (a.imag() - a.real()));
            a.imag(hsqt2 * (-tmp_ - a.imag()));
        } else {
            const auto tmp_ = a.real();
            a.real(hsqt2 * (-a.real() - a.imag()));
            a.imag(hsqt2 * (tmp_ - a.imag()));
        }
    }

    template<bool fwd>
    void pass8 (size_t ido, size_t l1, const Complex* cc, Complex* ch, const Complex* wa) const {
        auto CH = [ch, ido, l1] (size_t a, size_t b, size_t c) -> Complex& { return ch[a + ido * (b + l1 * c)]; };
        auto CC = [cc, ido] (size_t a, size_t b, size_t c) -> const Complex& { return cc[a + ido * (b + 8 * c)]; };
        auto WA = [wa, ido] (size_t x, size_t i) { return wa[i - 1 + x * (ido - 1)]; };

        if (ido == 1)
            for (size_t k = 0; k < l1; ++k) {
                Complex a0, a1, a2, a3, a4, a5, a6, a7;
                PM(a1, a5, CC(0, 1, k), CC(0, 5, k));
                PM(a3, a7, CC(0, 3, k), CC(0, 7, k));
                PMINPLACE(a1, a3);
                ROTX90<fwd>(a3);

                ROTX90<fwd>(a7);
                PMINPLACE(a5, a7);
                ROTX45<fwd>(a5);
                ROTX135<fwd>(a7);

                PM(a0, a4, CC(0, 0, k), CC(0, 4, k));
                PM(a2, a6, CC(0, 2, k), CC(0, 6, k));
                PM(CH(0, k, 0), CH(0, k, 4), a0 + a2, a1);
                PM(CH(0, k, 2), CH(0, k, 6), a0 - a2, a3);
                ROTX90<fwd>(a6);
                PM(CH(0, k, 1), CH(0, k, 5), a4 + a6, a5);
                PM(CH(0, k, 3), CH(0, k, 7), a4 - a6, a7);
            }
        else
            for (size_t k = 0; k < l1; ++k) {
                {
                    Complex a0, a1, a2, a3, a4, a5, a6, a7;
                    PM(a1, a5, CC(0, 1, k), CC(0, 5, k));
                    PM(a3, a7, CC(0, 3, k), CC(0, 7, k));
                    PMINPLACE(a1, a3);
                    ROTX90<fwd>(a3);

                    ROTX90<fwd>(a7);
                    PMINPLACE(a5, a7);
                    ROTX45<fwd>(a5);
                    ROTX135<fwd>(a7);

                    PM(a0, a4, CC(0, 0, k), CC(0, 4, k));
                    PM(a2, a6, CC(0, 2, k), CC(0, 6, k));
                    PM(CH(0, k, 0), CH(0, k, 4), a0 + a2, a1);
                    PM(CH(0, k, 2), CH(0, k, 6), a0 - a2, a3);
                    ROTX90<fwd>(a6);
                    PM(CH(0, k, 1), CH(0, k, 5), a4 + a6, a5);
                    PM(CH(0, k, 3), CH(0, k, 7), a4 - a6, a7);
                }
                for (size_t i = 1; i < ido; ++i) {
                    Complex a0, a1, a2, a3, a4, a5, a6, a7;
                    PM(a1, a5, CC(i, 1, k), CC(i, 5, k));
                    PM(a3, a7, CC(i, 3, k), CC(i, 7, k));
                    ROTX90<fwd>(a7);
                    PMINPLACE(a1, a3);
                    ROTX90<fwd>(a3);
                    PMINPLACE(a5, a7);
                    ROTX45<fwd>(a5);
                    ROTX135<fwd>(a7);
                    PM(a0, a4, CC(i, 0, k), CC(i, 4, k));
                    PM(a2, a6, CC(i, 2, k), CC(i, 6, k));
                    PMINPLACE(a0, a2);
                    CH(i, k, 0) = a0 + a1;
                    special_mul<fwd>(a0 - a1, WA(3, i), CH(i, k, 4));
                    special_mul<fwd>(a2 + a3, WA(1, i), CH(i, k, 2));
                    special_mul<fwd>(a2 - a3, WA(5, i), CH(i, k, 6));
                    ROTX90<fwd>(a6);
                    PMINPLACE(a4, a6);
                    special_mul<fwd>(a4 + a5, WA(0, i), CH(i, k, 1));
                    special_mul<fwd>(a4 - a5, WA(4, i), CH(i, k, 5));
                    special_mul<fwd>(a6 + a7, WA(2, i), CH(i, k, 3));
                    special_mul<fwd>(a6 - a7, WA(6, i), CH(i, k, 7));
                }
            }
    }

    template<bool fwd>
    void pass_all (Complex c[], double fct) const {
        if (length == 1) {
            c[0] *= fct;
            return;
        }
        size_t               l1 = 1;
        std::vector<Complex> ch(length);
        Complex *            p1 = c, *p2 = ch.data();

        for (size_t k1 = 0; k1 < fact.size(); k1++) {
            const fctdata& factor = fact[k1];
            size_t         ip     = factor.fct;
            size_t         l2     = ip * l1;
            size_t         ido    = length / l2;
            if (ip == 4)
                pass4<fwd>(ido, l1, p1, p2, twiddle_data(factor));
            else if (ip == 8)
                pass8<fwd>(ido, l1, p1, p2, twiddle_data(factor));
            else if (ip == 2)
                pass2<fwd>(ido, l1, p1, p2, twiddle_data(factor));
            else
                throw std::logic_error("unexpected non-power-of-two FFT factor");
            std::swap(p1, p2);
            l1 = l2;
        }
        if (p1 != c) {
            if (fct != 1.)
                for (size_t i = 0; i < length; ++i)
                    c[i] = ch[i] * fct;
            else
                std::copy_n(p1, length, c);
        } else if (fct != 1.)
            for (size_t i = 0; i < length; ++i)
                c[i] *= fct;
    }

  public:
    void exec (Complex c[], double fct, bool fwd) const { fwd ? pass_all<true>(c, fct) : pass_all<false>(c, fct); }

  private:
    void factorize () {
        size_t len = length;
        while ((len & 7) == 0) {
            add_factor(8);
            len >>= 3;
        }
        while ((len & 3) == 0) {
            add_factor(4);
            len >>= 2;
        }
        if ((len & 1) == 0) {
            len >>= 1;
            // Factor 2 should be at the front of the factor list.
            add_factor(2);
            std::swap(fact[0].fct, fact.back().fct);
        }
        if (len != 1)
            throw std::invalid_argument("complex FFT length must be a power of two");
    }

    size_t twsize () const {
        size_t twsize = 0, l1 = 1;
        for (size_t k = 0; k < fact.size(); ++k) {
            size_t ip = fact[k].fct, ido = length / (l1 * ip);
            twsize += (ip - 1) * (ido - 1);
            l1 *= ip;
        }
        return twsize;
    }

    void comp_twiddle () {
        sincos_2pibyn twiddle(length);
        size_t        l1 = 1;
        for (size_t k = 0; k < fact.size(); ++k) {
            size_t ip = fact[k].fct, ido = length / (l1 * ip);
            fact[k].tw_offset = mem.size();
            for (size_t j = 1; j < ip; ++j)
                for (size_t i = 1; i < ido; ++i)
                    mem.push_back(twiddle[j * l1 * i]);
            l1 *= ip;
        }
    }

  public:
    cfftp (size_t length_): length(length_) {
        if (length == 0)
            throw std::runtime_error("zero-length FFT requested");
        if (length == 1)
            return;
        factorize();
        mem.reserve(twsize());
        comp_twiddle();
    }
};

} // namespace

namespace fft {

void VARIANT (c2c_power_of_two)(Complex* data, std::size_t length, bool forward, double fct) {
    if (length == 0)
        return;
    if ((length & (length - 1U)) != 0)
        throw std::invalid_argument("complex FFT length must be a power of two");
    cfftp plan(length);
    plan.exec(data, fct, forward);
}

} // namespace fft
