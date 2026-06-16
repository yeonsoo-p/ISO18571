#include "simd.hpp"

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

}  // namespace iso18571_native
