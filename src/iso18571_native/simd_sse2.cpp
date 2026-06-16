#include "simd.hpp"

#include <cmath>
#include <cstddef>

#if defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#include <emmintrin.h>
#define ISO18571_HAS_SSE2_INTRINSICS 1
#endif

namespace iso18571_native {

void gradient_contiguous_sse2(const double* values, std::size_t n, double dt, double* out) {
#if defined(ISO18571_HAS_SSE2_INTRINSICS)
    out[0] = (values[1] - values[0]) / dt;

    const __m128d scale = _mm_set1_pd(2.0 * dt);
    std::size_t idx = 1;
    for (; idx + 1 < n - 1; idx += 2) {
        const __m128d hi = _mm_loadu_pd(values + idx + 1);
        const __m128d lo = _mm_loadu_pd(values + idx - 1);
        const __m128d gradient = _mm_div_pd(_mm_sub_pd(hi, lo), scale);
        _mm_storeu_pd(out + idx, gradient);
    }
    for (; idx < n - 1; ++idx) {
        out[idx] = (values[idx + 1] - values[idx - 1]) / (2.0 * dt);
    }
    out[n - 1] = (values[n - 1] - values[n - 2]) / dt;
#else
    gradient_contiguous_scalar(values, n, dt, out);
#endif
}

double dot_product_contiguous_sse2(const double* x, const double* y, std::size_t n) {
#if defined(ISO18571_HAS_SSE2_INTRINSICS)
    __m128d acc = _mm_setzero_pd();
    std::size_t idx = 0;
    for (; idx + 1 < n; idx += 2) {
        const __m128d vx = _mm_loadu_pd(x + idx);
        const __m128d vy = _mm_loadu_pd(y + idx);
        acc = _mm_add_pd(acc, _mm_mul_pd(vx, vy));
    }
    double lanes[2];
    _mm_storeu_pd(lanes, acc);
    double out = lanes[0] + lanes[1];
    for (; idx < n; ++idx) {
        out += x[idx] * y[idx];
    }
    return out;
#else
    return dot_product_contiguous_scalar(x, y, n);
#endif
}

void local_cost_contiguous_sse2(double x_value, const double* y, std::size_t n, double* out) {
#if defined(ISO18571_HAS_SSE2_INTRINSICS)
    const __m128d vx = _mm_set1_pd(x_value);
    std::size_t idx = 0;
    for (; idx + 1 < n; idx += 2) {
        const __m128d vy = _mm_loadu_pd(y + idx);
        const __m128d delta = _mm_sub_pd(vx, vy);
        _mm_storeu_pd(out + idx, _mm_mul_pd(delta, delta));
    }
    for (; idx < n; ++idx) {
        const double delta = x_value - y[idx];
        out[idx] = delta * delta;
    }
#else
    local_cost_contiguous_scalar(x_value, y, n, out);
#endif
}

void smooth9_contiguous_sse2(const double* gradient, std::size_t n, double* out) {
#if defined(ISO18571_HAS_SSE2_INTRINSICS)
    const __m128d scale = _mm_set1_pd(1.0 / 9.0);
    std::size_t idx = 4;
    for (; idx + 5 < n; idx += 2) {
        __m128d sum = _mm_loadu_pd(gradient + idx - 4);
        sum = _mm_add_pd(sum, _mm_loadu_pd(gradient + idx - 3));
        sum = _mm_add_pd(sum, _mm_loadu_pd(gradient + idx - 2));
        sum = _mm_add_pd(sum, _mm_loadu_pd(gradient + idx - 1));
        sum = _mm_add_pd(sum, _mm_loadu_pd(gradient + idx));
        sum = _mm_add_pd(sum, _mm_loadu_pd(gradient + idx + 1));
        sum = _mm_add_pd(sum, _mm_loadu_pd(gradient + idx + 2));
        sum = _mm_add_pd(sum, _mm_loadu_pd(gradient + idx + 3));
        sum = _mm_add_pd(sum, _mm_loadu_pd(gradient + idx + 4));
        _mm_storeu_pd(out + idx, _mm_mul_pd(sum, scale));
    }
    for (; idx + 4 < n; ++idx) {
        double sum = 0.0;
        for (std::size_t offset = idx - 4; offset <= idx + 4; ++offset) {
            sum += gradient[offset];
        }
        out[idx] = sum / 9.0;
    }
#else
    smooth9_contiguous_scalar(gradient, n, out);
#endif
}

L1Sums l1_pair_contiguous_sse2(const double* x, const double* y, std::size_t n) {
#if defined(ISO18571_HAS_SSE2_INTRINSICS)
    const __m128d sign = _mm_set1_pd(-0.0);
    __m128d numerator = _mm_setzero_pd();
    __m128d denominator = _mm_setzero_pd();
    std::size_t idx = 0;
    for (; idx + 1 < n; idx += 2) {
        const __m128d vx = _mm_loadu_pd(x + idx);
        const __m128d vy = _mm_loadu_pd(y + idx);
        numerator = _mm_add_pd(numerator, _mm_andnot_pd(sign, _mm_sub_pd(vx, vy)));
        denominator = _mm_add_pd(denominator, _mm_andnot_pd(sign, vy));
    }
    double num_lanes[2];
    double den_lanes[2];
    _mm_storeu_pd(num_lanes, numerator);
    _mm_storeu_pd(den_lanes, denominator);
    L1Sums out{num_lanes[0] + num_lanes[1], den_lanes[0] + den_lanes[1]};
    for (; idx < n; ++idx) {
        out.numerator += std::abs(x[idx] - y[idx]);
        out.denominator += std::abs(y[idx]);
    }
    return out;
#else
    return l1_pair_contiguous_scalar(x, y, n);
#endif
}

L1Sums l1_x_constant_contiguous_sse2(const double* x, double y_value, std::size_t n) {
#if defined(ISO18571_HAS_SSE2_INTRINSICS)
    const __m128d sign = _mm_set1_pd(-0.0);
    const __m128d vy = _mm_set1_pd(y_value);
    __m128d numerator = _mm_setzero_pd();
    std::size_t idx = 0;
    for (; idx + 1 < n; idx += 2) {
        const __m128d vx = _mm_loadu_pd(x + idx);
        numerator = _mm_add_pd(numerator, _mm_andnot_pd(sign, _mm_sub_pd(vx, vy)));
    }
    double lanes[2];
    _mm_storeu_pd(lanes, numerator);
    L1Sums out{lanes[0] + lanes[1], std::abs(y_value) * static_cast<double>(idx)};
    for (; idx < n; ++idx) {
        out.numerator += std::abs(x[idx] - y_value);
        out.denominator += std::abs(y_value);
    }
    return out;
#else
    return l1_x_constant_contiguous_scalar(x, y_value, n);
#endif
}

L1Sums l1_constant_y_contiguous_sse2(double x_value, const double* y, std::size_t n) {
#if defined(ISO18571_HAS_SSE2_INTRINSICS)
    const __m128d sign = _mm_set1_pd(-0.0);
    const __m128d vx = _mm_set1_pd(x_value);
    __m128d numerator = _mm_setzero_pd();
    __m128d denominator = _mm_setzero_pd();
    std::size_t idx = 0;
    for (; idx + 1 < n; idx += 2) {
        const __m128d vy = _mm_loadu_pd(y + idx);
        numerator = _mm_add_pd(numerator, _mm_andnot_pd(sign, _mm_sub_pd(vx, vy)));
        denominator = _mm_add_pd(denominator, _mm_andnot_pd(sign, vy));
    }
    double num_lanes[2];
    double den_lanes[2];
    _mm_storeu_pd(num_lanes, numerator);
    _mm_storeu_pd(den_lanes, denominator);
    L1Sums out{num_lanes[0] + num_lanes[1], den_lanes[0] + den_lanes[1]};
    for (; idx < n; ++idx) {
        out.numerator += std::abs(x_value - y[idx]);
        out.denominator += std::abs(y[idx]);
    }
    return out;
#else
    return l1_constant_y_contiguous_scalar(x_value, y, n);
#endif
}

}  // namespace iso18571_native
