#include "fft.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <vector>

#ifndef FFT_IMPL_SUFFIX
#error "FFT_IMPL_SUFFIX must be defined before including fft.cpp"
#endif

#define FFT_PASTE_INNER(a, b) a##b
#define FFT_PASTE(a, b) FFT_PASTE_INNER(a, b)
#define FFT_VARIANT(name) FFT_PASTE(name, FFT_IMPL_SUFFIX)

#if defined(__GNUC__)
#define FFT_RESTRICT __restrict__
#elif defined(_MSC_VER)
#define FFT_RESTRICT __restrict
#else
#define FFT_RESTRICT
#endif

namespace {
using std::size_t;

// Always use std:: for <cmath> functions.
template<typename T>
T cos(T) = delete;
template<typename T>
T sin(T) = delete;
template<typename T>
T sqrt(T) = delete;

inline void* aligned_alloc (size_t align, size_t size) {
    // aligned_alloc() requires that the requested size is a multiple of "align".
    void* ptr = ::aligned_alloc(align, (size + align - 1) & (~(align - 1)));
    if (!ptr)
        throw std::bad_alloc();
    return ptr;
}
inline void aligned_dealloc (void* ptr) { free(ptr); }

template<typename T>
class arr {
  private:
    T*     p;
    size_t sz;

    static T* ralloc (size_t num) {
        if (num == 0)
            return nullptr;
        void* ptr = aligned_alloc(64, num * sizeof(T));
        return static_cast<T*>(ptr);
    }
    static void dealloc (T* ptr) { aligned_dealloc(ptr); }

  public:
    arr (): p(0), sz(0) {}
    arr (size_t n): p(ralloc(n)), sz(n) {}
    arr (arr&& other): p(other.p), sz(other.sz) {
        other.p  = nullptr;
        other.sz = 0;
    }
    ~arr () { dealloc(p); }

    void resize (size_t n) {
        if (n == sz)
            return;
        dealloc(p);
        p  = ralloc(n);
        sz = n;
    }

    T&       operator [](size_t idx) { return p[idx]; }
    const T& operator [](size_t idx) const { return p[idx]; }

    T*       data () { return p; }
    const T* data () const { return p; }

    size_t size () const { return sz; }
};

template<typename T>
struct cmplx {
    T r, i;
    cmplx () {}
    cmplx (T r_, T i_): r(r_), i(i_) {}
    void Set (T r_, T i_) {
        r = r_;
        i = i_;
    }
    void Set (T r_) {
        r = r_;
        i = T(0);
    }
    cmplx& operator +=(const cmplx& other) {
        r += other.r;
        i += other.i;
        return *this;
    }
    template<typename T2>
    cmplx& operator *=(T2 other) {
        r *= other;
        i *= other;
        return *this;
    }
    template<typename T2>
    auto operator *(const T2& other) const -> cmplx<decltype(r * other)> {
        return {r * other, i * other};
    }
    template<typename T2>
    auto operator +(const cmplx<T2>& other) const -> cmplx<decltype(r + other.r)> {
        return {r + other.r, i + other.i};
    }
    template<typename T2>
    auto operator -(const cmplx<T2>& other) const -> cmplx<decltype(r + other.r)> {
        return {r - other.r, i - other.i};
    }
};

template<typename T>
inline void PM (T& a, T& b, T c, T d) {
    a = c + d;
    b = c - d;
}

template<typename T>
inline void PMINPLACE (T& a, T& b) {
    T t = a;
    a += b;
    b = t - b;
}

template<bool fwd, typename T, typename T2>
void special_mul (const cmplx<T>& v1, const cmplx<T2>& v2, cmplx<T>& res) {
    res = fwd ? cmplx<T>(v1.r * v2.r + v1.i * v2.i, v1.i * v2.r - v1.r * v2.i)
              : cmplx<T>(v1.r * v2.r - v1.i * v2.i, v1.r * v2.i + v1.i * v2.r);
}

template<bool fwd, typename T>
void ROTX90 (cmplx<T>& a) {
    auto tmp_ = fwd ? -a.r : a.r;
    a.r       = fwd ? a.i : -a.i;
    a.i       = tmp_;
}

template<typename T>
class sincos_2pibyn {
  private:
    using Thigh = typename std::conditional<(sizeof(T) > sizeof(double)), T, double>::type;
    size_t            N, mask, shift;
    arr<cmplx<Thigh>> v1, v2;

    static cmplx<Thigh> calc (size_t x, size_t n, Thigh ang) {
        x <<= 3;
        if (x < 4 * n) {
            if (x < 2 * n) {
                if (x < n)
                    return cmplx<Thigh>(std::cos(Thigh(x) * ang), std::sin(Thigh(x) * ang));
                return cmplx<Thigh>(std::sin(Thigh(2 * n - x) * ang), std::cos(Thigh(2 * n - x) * ang));
            }
            x -= 2 * n;
            if (x < n)
                return cmplx<Thigh>(-std::sin(Thigh(x) * ang), std::cos(Thigh(x) * ang));
            return cmplx<Thigh>(-std::cos(Thigh(2 * n - x) * ang), std::sin(Thigh(2 * n - x) * ang));
        }
        x = 8 * n - x;
        if (x < 2 * n) {
            if (x < n)
                return cmplx<Thigh>(std::cos(Thigh(x) * ang), -std::sin(Thigh(x) * ang));
            return cmplx<Thigh>(std::sin(Thigh(2 * n - x) * ang), -std::cos(Thigh(2 * n - x) * ang));
        }
        x -= 2 * n;
        if (x < n)
            return cmplx<Thigh>(-std::sin(Thigh(x) * ang), -std::cos(Thigh(x) * ang));
        return cmplx<Thigh>(-std::cos(Thigh(2 * n - x) * ang), -std::sin(Thigh(2 * n - x) * ang));
    }

  public:
    sincos_2pibyn (size_t n): N(n) {
        constexpr auto pi   = 3.141592653589793238462643383279502884197L;
        Thigh          ang  = Thigh(0.25L * pi / n);
        size_t         nval = (n + 2) / 2;
        shift               = 1;
        while ((size_t(1) << shift) * (size_t(1) << shift) < nval)
            ++shift;
        mask = (size_t(1) << shift) - 1;
        v1.resize(mask + 1);
        v1[0].Set(Thigh(1), Thigh(0));
        for (size_t i = 1; i < v1.size(); ++i)
            v1[i] = calc(i, n, ang);
        v2.resize((nval + mask) / (mask + 1));
        v2[0].Set(Thigh(1), Thigh(0));
        for (size_t i = 1; i < v2.size(); ++i)
            v2[i] = calc(i * (mask + 1), n, ang);
    }

    cmplx<T> operator [](size_t idx) const {
        if (2 * idx <= N) {
            auto x1 = v1[idx & mask], x2 = v2[idx >> shift];
            return cmplx<T>(T(x1.r * x2.r - x1.i * x2.i), T(x1.r * x2.i + x1.i * x2.r));
        }
        idx     = N - idx;
        auto x1 = v1[idx & mask], x2 = v2[idx >> shift];
        return cmplx<T>(T(x1.r * x2.r - x1.i * x2.i), -T(x1.r * x2.i + x1.i * x2.r));
    }
};

template<typename T0>
class cfftp {
  private:
    struct fctdata {
        size_t     fct;
        cmplx<T0>* tw;
    };

    size_t               length;
    arr<cmplx<T0>>       mem;
    std::vector<fctdata> fact;

    void add_factor (size_t factor) { fact.push_back({factor, nullptr}); }

    template<bool fwd, typename T>
    void pass2 (size_t ido, size_t l1, const T* FFT_RESTRICT cc, T* FFT_RESTRICT ch,
                const cmplx<T0>* FFT_RESTRICT wa) const {
        auto CH = [ch, ido, l1] (size_t a, size_t b, size_t c) -> T& { return ch[a + ido * (b + l1 * c)]; };
        auto CC = [cc, ido] (size_t a, size_t b, size_t c) -> const T& { return cc[a + ido * (b + 2 * c)]; };
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

    template<bool fwd, typename T>
    void pass4 (size_t ido, size_t l1, const T* FFT_RESTRICT cc, T* FFT_RESTRICT ch,
                const cmplx<T0>* FFT_RESTRICT wa) const {
        auto CH = [ch, ido, l1] (size_t a, size_t b, size_t c) -> T& { return ch[a + ido * (b + l1 * c)]; };
        auto CC = [cc, ido] (size_t a, size_t b, size_t c) -> const T& { return cc[a + ido * (b + 4 * c)]; };
        auto WA = [wa, ido] (size_t x, size_t i) { return wa[i - 1 + x * (ido - 1)]; };

        if (ido == 1)
            for (size_t k = 0; k < l1; ++k) {
                T t1, t2, t3, t4;
                PM(t2, t1, CC(0, 0, k), CC(0, 2, k));
                PM(t3, t4, CC(0, 1, k), CC(0, 3, k));
                ROTX90<fwd>(t4);
                PM(CH(0, k, 0), CH(0, k, 2), t2, t3);
                PM(CH(0, k, 1), CH(0, k, 3), t1, t4);
            }
        else
            for (size_t k = 0; k < l1; ++k) {
                {
                    T t1, t2, t3, t4;
                    PM(t2, t1, CC(0, 0, k), CC(0, 2, k));
                    PM(t3, t4, CC(0, 1, k), CC(0, 3, k));
                    ROTX90<fwd>(t4);
                    PM(CH(0, k, 0), CH(0, k, 2), t2, t3);
                    PM(CH(0, k, 1), CH(0, k, 3), t1, t4);
                }
                for (size_t i = 1; i < ido; ++i) {
                    T t1, t2, t3, t4;
                    T cc0 = CC(i, 0, k), cc1 = CC(i, 1, k), cc2 = CC(i, 2, k), cc3 = CC(i, 3, k);
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

    template<bool fwd, typename T>
    void ROTX45 (T& a) const {
        constexpr T0 hsqt2 = T0(0.707106781186547524400844362104849L);
        if (fwd) {
            auto tmp_ = a.r;
            a.r       = hsqt2 * (a.r + a.i);
            a.i       = hsqt2 * (a.i - tmp_);
        } else {
            auto tmp_ = a.r;
            a.r       = hsqt2 * (a.r - a.i);
            a.i       = hsqt2 * (a.i + tmp_);
        }
    }
    template<bool fwd, typename T>
    void ROTX135 (T& a) const {
        constexpr T0 hsqt2 = T0(0.707106781186547524400844362104849L);
        if (fwd) {
            auto tmp_ = a.r;
            a.r       = hsqt2 * (a.i - a.r);
            a.i       = hsqt2 * (-tmp_ - a.i);
        } else {
            auto tmp_ = a.r;
            a.r       = hsqt2 * (-a.r - a.i);
            a.i       = hsqt2 * (tmp_ - a.i);
        }
    }

    template<bool fwd, typename T>
    void pass8 (size_t ido, size_t l1, const T* FFT_RESTRICT cc, T* FFT_RESTRICT ch,
                const cmplx<T0>* FFT_RESTRICT wa) const {
        auto CH = [ch, ido, l1] (size_t a, size_t b, size_t c) -> T& { return ch[a + ido * (b + l1 * c)]; };
        auto CC = [cc, ido] (size_t a, size_t b, size_t c) -> const T& { return cc[a + ido * (b + 8 * c)]; };
        auto WA = [wa, ido] (size_t x, size_t i) { return wa[i - 1 + x * (ido - 1)]; };

        if (ido == 1)
            for (size_t k = 0; k < l1; ++k) {
                T a0, a1, a2, a3, a4, a5, a6, a7;
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
                    T a0, a1, a2, a3, a4, a5, a6, a7;
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
                    T a0, a1, a2, a3, a4, a5, a6, a7;
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

    template<bool fwd, typename T>
    void pass_all (T c[], T0 fct) const {
        if (length == 1) {
            c[0] *= fct;
            return;
        }
        size_t l1 = 1;
        arr<T> ch(length);
        T *    p1 = c, *p2 = ch.data();

        for (size_t k1 = 0; k1 < fact.size(); k1++) {
            size_t ip  = fact[k1].fct;
            size_t l2  = ip * l1;
            size_t ido = length / l2;
            if (ip == 4)
                pass4<fwd>(ido, l1, p1, p2, fact[k1].tw);
            else if (ip == 8)
                pass8<fwd>(ido, l1, p1, p2, fact[k1].tw);
            else if (ip == 2)
                pass2<fwd>(ido, l1, p1, p2, fact[k1].tw);
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
    template<typename T>
    void exec (T c[], T0 fct, bool fwd) const {
        fwd ? pass_all<true>(c, fct) : pass_all<false>(c, fct);
    }

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
        sincos_2pibyn<T0> twiddle(length);
        size_t            l1     = 1;
        size_t            memofs = 0;
        for (size_t k = 0; k < fact.size(); ++k) {
            size_t ip = fact[k].fct, ido = length / (l1 * ip);
            fact[k].tw = mem.data() + memofs;
            memofs += (ip - 1) * (ido - 1);
            for (size_t j = 1; j < ip; ++j)
                for (size_t i = 1; i < ido; ++i)
                    fact[k].tw[(j - 1) * (ido - 1) + i - 1] = twiddle[j * l1 * i];
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
        mem.resize(twsize());
        comp_twiddle();
    }
};

} // namespace

namespace fft {

void FFT_VARIANT (c2c_power_of_two)(std::complex<double>* data, std::size_t length, bool forward, double fct) {
    if (length == 0)
        return;
    if ((length & (length - 1U)) != 0)
        throw std::invalid_argument("complex FFT length must be a power of two");
    static_assert(sizeof(std::complex<double>) == sizeof(cmplx<double>),
                  "std::complex storage is incompatible with the local FFT representation");
    cfftp<double> plan(length);
    plan.exec(reinterpret_cast<cmplx<double>*>(data), fct, forward);
}

} // namespace fft

#undef FFT_RESTRICT
#undef FFT_VARIANT
#undef FFT_PASTE
#undef FFT_PASTE_INNER
