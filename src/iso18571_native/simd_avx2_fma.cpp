#include "simd.hpp"

#include <cstddef>

#if defined(__AVX2__) && defined(__FMA__)
#include <immintrin.h>
#define ISO18571_HAS_AVX2_FMA_INTRINSICS 1
#endif

namespace iso18571_native {

void gradient_contiguous_avx2_fma(const double* values, std::size_t n, double dt, double* out) {
#if defined(ISO18571_HAS_AVX2_FMA_INTRINSICS)
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
#else
    gradient_contiguous_scalar(values, n, dt, out);
#endif
}

}  // namespace iso18571_native
