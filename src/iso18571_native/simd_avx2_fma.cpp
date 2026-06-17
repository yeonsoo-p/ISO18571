#include "simd.hpp"

#include <cmath>
#include <cstddef>

#if defined(__AVX2__) && defined(__FMA__)
#include <immintrin.h>
#define ISO18571_HAS_AVX2_FMA_INTRINSICS 1
#else
#error "simd_avx2_fma.cpp must be compiled with AVX2 and FMA enabled"
#endif

namespace iso18571_native {

void gradient_contiguous_avx2_fma(const double* values, std::size_t n, double dt, double* out) {
    out[0] = (values[1] - values[0]) / dt;

    const __m256d scale = _mm256_set1_pd(2.0 * dt);
    std::size_t idx = 1;
    for (; idx + 3 < n - 1; idx += 4) {
        const __m256d hi = _mm256_loadu_pd(values + idx + 1);
        const __m256d lo = _mm256_loadu_pd(values + idx - 1);
        const __m256d gradient = _mm256_div_pd(_mm256_sub_pd(hi, lo), scale);
        _mm256_storeu_pd(out + idx, gradient);
    }
    for (; idx < n - 1; ++idx) {
        out[idx] = (values[idx + 1] - values[idx - 1]) / (2.0 * dt);
    }
    out[n - 1] = (values[n - 1] - values[n - 2]) / dt;
}

double dot_product_contiguous_avx2_fma(const double* x, const double* y, std::size_t n) {
    __m256d acc = _mm256_setzero_pd();
    std::size_t idx = 0;
    for (; idx + 3 < n; idx += 4) {
        const __m256d vx = _mm256_loadu_pd(x + idx);
        const __m256d vy = _mm256_loadu_pd(y + idx);
        acc = _mm256_fmadd_pd(vx, vy, acc);
    }
    double lanes[4];
    _mm256_storeu_pd(lanes, acc);
    double out = lanes[0] + lanes[1] + lanes[2] + lanes[3];
    for (; idx < n; ++idx) {
        out += x[idx] * y[idx];
    }
    return out;
}

void local_cost_contiguous_avx2_fma(double x_value, const double* y, std::size_t n, double* out) {
    local_cost_contiguous_avx2(x_value, y, n, out);
}

void smooth9_contiguous_avx2_fma(const double* gradient, std::size_t n, double* out) {
    smooth9_contiguous_avx2(gradient, n, out);
}

L1Sums l1_pair_contiguous_avx2_fma(const double* x, const double* y, std::size_t n) {
    return l1_pair_contiguous_avx2(x, y, n);
}

L1Sums l1_x_constant_contiguous_avx2_fma(const double* x, double y_value, std::size_t n) {
    return l1_x_constant_contiguous_avx2(x, y_value, n);
}

L1Sums l1_constant_y_contiguous_avx2_fma(double x_value, const double* y, std::size_t n) {
    return l1_constant_y_contiguous_avx2(x_value, y, n);
}

}  // namespace iso18571_native
