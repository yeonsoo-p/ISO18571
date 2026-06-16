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

struct L1Sums {
    double numerator = 0.0;
    double denominator = 0.0;
};

const char* simd_level_name(SimdLevel level);
SimdCapabilities simd_capabilities();
SimdSelection select_simd_level(SimdLevel requested);

void gradient_contiguous_scalar(const double* values, std::size_t n, double dt, double* out);
void gradient_contiguous_sse2(const double* values, std::size_t n, double dt, double* out);
void gradient_contiguous_avx2(const double* values, std::size_t n, double dt, double* out);
void gradient_contiguous_avx2_fma(const double* values, std::size_t n, double dt, double* out);
void gradient_contiguous(const double* values, std::size_t n, double dt, double* out, SimdLevel level);

double dot_product_contiguous_scalar(const double* x, const double* y, std::size_t n);
double dot_product_contiguous_sse2(const double* x, const double* y, std::size_t n);
double dot_product_contiguous_avx2(const double* x, const double* y, std::size_t n);
double dot_product_contiguous_avx2_fma(const double* x, const double* y, std::size_t n);
double dot_product_contiguous(const double* x, const double* y, std::size_t n, SimdLevel level);

void local_cost_contiguous_scalar(double x_value, const double* y, std::size_t n, double* out);
void local_cost_contiguous_sse2(double x_value, const double* y, std::size_t n, double* out);
void local_cost_contiguous_avx2(double x_value, const double* y, std::size_t n, double* out);
void local_cost_contiguous_avx2_fma(double x_value, const double* y, std::size_t n, double* out);
void local_cost_contiguous(double x_value, const double* y, std::size_t n, double* out, SimdLevel level);

void smooth9_contiguous_scalar(const double* gradient, std::size_t n, double* out);
void smooth9_contiguous_sse2(const double* gradient, std::size_t n, double* out);
void smooth9_contiguous_avx2(const double* gradient, std::size_t n, double* out);
void smooth9_contiguous_avx2_fma(const double* gradient, std::size_t n, double* out);
void smooth9_contiguous(const double* gradient, std::size_t n, double* out, SimdLevel level);

L1Sums l1_pair_contiguous_scalar(const double* x, const double* y, std::size_t n);
L1Sums l1_pair_contiguous_sse2(const double* x, const double* y, std::size_t n);
L1Sums l1_pair_contiguous_avx2(const double* x, const double* y, std::size_t n);
L1Sums l1_pair_contiguous_avx2_fma(const double* x, const double* y, std::size_t n);
L1Sums l1_pair_contiguous(const double* x, const double* y, std::size_t n, SimdLevel level);

L1Sums l1_x_constant_contiguous_scalar(const double* x, double y_value, std::size_t n);
L1Sums l1_x_constant_contiguous_sse2(const double* x, double y_value, std::size_t n);
L1Sums l1_x_constant_contiguous_avx2(const double* x, double y_value, std::size_t n);
L1Sums l1_x_constant_contiguous_avx2_fma(const double* x, double y_value, std::size_t n);
L1Sums l1_x_constant_contiguous(const double* x, double y_value, std::size_t n, SimdLevel level);

L1Sums l1_constant_y_contiguous_scalar(double x_value, const double* y, std::size_t n);
L1Sums l1_constant_y_contiguous_sse2(double x_value, const double* y, std::size_t n);
L1Sums l1_constant_y_contiguous_avx2(double x_value, const double* y, std::size_t n);
L1Sums l1_constant_y_contiguous_avx2_fma(double x_value, const double* y, std::size_t n);
L1Sums l1_constant_y_contiguous(double x_value, const double* y, std::size_t n, SimdLevel level);

}  // namespace iso18571_native
