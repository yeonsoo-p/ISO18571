#pragma once

#include <cstddef>
#include <cstdint>

namespace iso18571_native {

enum class SimdLevel : std::int32_t {
    Scalar = 0,
    Sse2 = 1,
    Avx2 = 2,
    Avx2Fma = 3,
    Auto = 4,
};

struct SimdSelection {
    SimdLevel requested = SimdLevel::Scalar;
    SimdLevel selected = SimdLevel::Scalar;
    bool fallback = false;
};

struct SimdCapabilities {
    bool compiled_scalar = true;
    bool compiled_sse2 = false;
    bool compiled_avx2 = false;
    bool compiled_avx2_fma = false;
    bool detected_sse2 = false;
    bool detected_avx2 = false;
    bool detected_fma = false;
    SimdLevel auto_level = SimdLevel::Scalar;
};

const char* simd_level_name(SimdLevel level);
SimdCapabilities simd_capabilities();
SimdSelection select_simd_level(SimdLevel requested);

void gradient_contiguous_scalar(const double* values, std::size_t n, double dt, double* out);
void gradient_contiguous_sse2(const double* values, std::size_t n, double dt, double* out);
void gradient_contiguous_avx2(const double* values, std::size_t n, double dt, double* out);
void gradient_contiguous_avx2_fma(const double* values, std::size_t n, double dt, double* out);
void gradient_contiguous(const double* values, std::size_t n, double dt, double* out, SimdLevel level);

}  // namespace iso18571_native
