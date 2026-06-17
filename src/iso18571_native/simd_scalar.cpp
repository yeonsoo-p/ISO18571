#include "simd.hpp"

#include <algorithm>
#include <cmath>

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_AMD64) || defined(_M_IX86))
#include <intrin.h>
#endif

#if defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
#include <cpuid.h>
#endif

namespace iso18571_native {

namespace {

bool detected_sse2() {
#if defined(_M_X64) || defined(_M_AMD64)
    return true;
#elif defined(_MSC_VER) && defined(_M_IX86)
    int registers[4] = {};
    __cpuid(registers, 1);
    return (registers[3] & (1 << 26)) != 0;
#elif defined(__x86_64__)
    return true;
#elif defined(__GNUC__) && defined(__i386__)
    unsigned int eax = 0;
    unsigned int ebx = 0;
    unsigned int ecx = 0;
    unsigned int edx = 0;
    if (__get_cpuid(1, &eax, &ebx, &ecx, &edx) == 0) {
        return false;
    }
    return (edx & bit_SSE2) != 0;
#else
    return false;
#endif
}

bool detected_avx2() {
#if defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
    __builtin_cpu_init();
    return __builtin_cpu_supports("avx2");
#elif defined(_MSC_VER) && (defined(_M_X64) || defined(_M_AMD64) || defined(_M_IX86))
    int registers[4] = {};
    __cpuid(registers, 1);
    const bool osxsave = (registers[2] & (1 << 27)) != 0;
    const bool avx = (registers[2] & (1 << 28)) != 0;
    if (!osxsave || !avx) {
        return false;
    }
    const unsigned long long mask = _xgetbv(0);
    if ((mask & 0x6) != 0x6) {
        return false;
    }
    __cpuidex(registers, 7, 0);
    return (registers[1] & (1 << 5)) != 0;
#else
    return false;
#endif
}

bool detected_fma() {
#if defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
    __builtin_cpu_init();
    return __builtin_cpu_supports("fma");
#elif defined(_MSC_VER) && (defined(_M_X64) || defined(_M_AMD64) || defined(_M_IX86))
    int registers[4] = {};
    __cpuid(registers, 1);
    return (registers[2] & (1 << 12)) != 0;
#else
    return false;
#endif
}

inline bool available(SimdLevel level, const SimdCapabilities& capabilities) {
    if (level == SimdLevel::Scalar) {
        return true;
    }
    if (level == SimdLevel::Sse2) {
        return capabilities.detected_sse2;
    }
    if (level == SimdLevel::Avx2) {
        return capabilities.detected_avx2;
    }
    if (level == SimdLevel::Avx2Fma) {
        return capabilities.detected_avx2 && capabilities.detected_fma;
    }
    return false;
}

}  // namespace

const char* simd_level_name(SimdLevel level) {
    switch (level) {
        case SimdLevel::Scalar:
            return "scalar";
        case SimdLevel::Sse2:
            return "sse2";
        case SimdLevel::Avx2:
            return "avx2";
        case SimdLevel::Avx2Fma:
            return "avx2_fma";
        case SimdLevel::Auto:
            return "auto";
    }
    return "unknown";
}

SimdCapabilities detect_simd_capabilities() {
    SimdCapabilities capabilities;
    capabilities.detected_sse2 = detected_sse2();
    capabilities.detected_avx2 = detected_avx2();
    capabilities.detected_fma = detected_fma();

    if (available(SimdLevel::Avx2Fma, capabilities)) {
        capabilities.auto_level = SimdLevel::Avx2Fma;
    } else if (available(SimdLevel::Avx2, capabilities)) {
        capabilities.auto_level = SimdLevel::Avx2;
    } else if (available(SimdLevel::Sse2, capabilities)) {
        capabilities.auto_level = SimdLevel::Sse2;
    } else {
        capabilities.auto_level = SimdLevel::Scalar;
    }
    return capabilities;
}

SimdCapabilities simd_capabilities() {
    static const SimdCapabilities capabilities = detect_simd_capabilities();
    return capabilities;
}

SimdSelection select_simd_level(SimdLevel requested) {
    const SimdCapabilities capabilities = simd_capabilities();
    SimdLevel selected = requested == SimdLevel::Auto ? capabilities.auto_level : requested;
    if (!available(selected, capabilities)) {
        if (requested == SimdLevel::Avx2Fma && available(SimdLevel::Avx2, capabilities)) {
            selected = SimdLevel::Avx2;
        } else if (
            (requested == SimdLevel::Avx2Fma || requested == SimdLevel::Avx2) &&
            available(SimdLevel::Sse2, capabilities)
        ) {
            selected = SimdLevel::Sse2;
        } else if (requested == SimdLevel::Sse2) {
            selected = SimdLevel::Scalar;
        } else if (available(SimdLevel::Sse2, capabilities)) {
            selected = SimdLevel::Sse2;
        } else {
            selected = SimdLevel::Scalar;
        }
    }
    return {requested, selected, selected != requested && requested != SimdLevel::Auto};
}

void gradient_contiguous_scalar(const double* values, std::size_t n, double dt, double* out) {
    out[0] = (values[1] - values[0]) / dt;
    for (std::size_t idx = 1; idx + 1 < n; ++idx) {
        out[idx] = (values[idx + 1] - values[idx - 1]) / (2.0 * dt);
    }
    out[n - 1] = (values[n - 1] - values[n - 2]) / dt;
}

double dot_product_contiguous_scalar(const double* x, const double* y, std::size_t n) {
    double out = 0.0;
    for (std::size_t idx = 0; idx < n; ++idx) {
        out += x[idx] * y[idx];
    }
    return out;
}

void local_cost_contiguous_scalar(double x_value, const double* y, std::size_t n, double* out) {
    for (std::size_t idx = 0; idx < n; ++idx) {
        const double delta = x_value - y[idx];
        out[idx] = delta * delta;
    }
}

void smooth9_contiguous_scalar(const double* gradient, std::size_t n, double* out) {
    for (std::size_t idx = 4; idx + 4 < n; ++idx) {
        double sum = 0.0;
        for (std::size_t offset = idx - 4; offset <= idx + 4; ++offset) {
            sum += gradient[offset];
        }
        out[idx] = sum / 9.0;
    }
}

L1Sums l1_pair_contiguous_scalar(const double* x, const double* y, std::size_t n) {
    L1Sums out;
    for (std::size_t idx = 0; idx < n; ++idx) {
        out.numerator += std::abs(x[idx] - y[idx]);
        out.denominator += std::abs(y[idx]);
    }
    return out;
}

L1Sums l1_x_constant_contiguous_scalar(const double* x, double y_value, std::size_t n) {
    L1Sums out;
    const double abs_y = std::abs(y_value);
    for (std::size_t idx = 0; idx < n; ++idx) {
        out.numerator += std::abs(x[idx] - y_value);
        out.denominator += abs_y;
    }
    return out;
}

L1Sums l1_constant_y_contiguous_scalar(double x_value, const double* y, std::size_t n) {
    L1Sums out;
    for (std::size_t idx = 0; idx < n; ++idx) {
        out.numerator += std::abs(x_value - y[idx]);
        out.denominator += std::abs(y[idx]);
    }
    return out;
}

}  // namespace iso18571_native
