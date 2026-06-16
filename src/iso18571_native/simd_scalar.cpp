#include "simd.hpp"

#include <algorithm>

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

bool compiled_sse2() {
#if defined(ISO18571_COMPILED_SSE2)
    return true;
#else
    return false;
#endif
}

bool compiled_avx2() {
#if defined(ISO18571_COMPILED_AVX2)
    return true;
#else
    return false;
#endif
}

bool compiled_avx2_fma() {
#if defined(ISO18571_COMPILED_AVX2_FMA)
    return true;
#else
    return false;
#endif
}

bool available(SimdLevel level, const SimdCapabilities& capabilities) {
    if (level == SimdLevel::Scalar) {
        return true;
    }
    if (level == SimdLevel::Sse2) {
        return capabilities.compiled_sse2 && capabilities.detected_sse2;
    }
    if (level == SimdLevel::Avx2) {
        return capabilities.compiled_avx2 && capabilities.detected_avx2;
    }
    if (level == SimdLevel::Avx2Fma) {
        return capabilities.compiled_avx2_fma && capabilities.detected_avx2 && capabilities.detected_fma;
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

SimdCapabilities simd_capabilities() {
    SimdCapabilities capabilities;
    capabilities.compiled_sse2 = compiled_sse2();
    capabilities.compiled_avx2 = compiled_avx2();
    capabilities.compiled_avx2_fma = compiled_avx2_fma();
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

SimdSelection select_simd_level(SimdLevel requested) {
    const SimdCapabilities capabilities = simd_capabilities();
    SimdLevel selected = requested == SimdLevel::Auto ? capabilities.auto_level : requested;
    if (!available(selected, capabilities)) {
        if (available(SimdLevel::Sse2, capabilities)) {
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

#if !defined(ISO18571_COMPILED_SSE2)
void gradient_contiguous_sse2(const double* values, std::size_t n, double dt, double* out) {
    gradient_contiguous_scalar(values, n, dt, out);
}
#endif

#if !defined(ISO18571_COMPILED_AVX2)
void gradient_contiguous_avx2(const double* values, std::size_t n, double dt, double* out) {
    gradient_contiguous_scalar(values, n, dt, out);
}
#endif

#if !defined(ISO18571_COMPILED_AVX2_FMA)
void gradient_contiguous_avx2_fma(const double* values, std::size_t n, double dt, double* out) {
    gradient_contiguous_scalar(values, n, dt, out);
}
#endif

void gradient_contiguous(const double* values, std::size_t n, double dt, double* out, SimdLevel level) {
    const SimdSelection selection = select_simd_level(level);
    switch (selection.selected) {
        case SimdLevel::Sse2:
            gradient_contiguous_sse2(values, n, dt, out);
            return;
        case SimdLevel::Avx2:
            gradient_contiguous_avx2(values, n, dt, out);
            return;
        case SimdLevel::Avx2Fma:
            gradient_contiguous_avx2_fma(values, n, dt, out);
            return;
        case SimdLevel::Auto:
        case SimdLevel::Scalar:
            gradient_contiguous_scalar(values, n, dt, out);
            return;
    }
}

}  // namespace iso18571_native
