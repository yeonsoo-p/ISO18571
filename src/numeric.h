#pragma once

#include "float16.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <type_traits>
#include <utility>

namespace numeric {

namespace detail {

template<typename T>
struct IsStdComplex: std::false_type {};

template<typename T>
struct IsStdComplex<std::complex<T>>: std::true_type {};

template<typename T>
inline constexpr bool is_std_complex_v = IsStdComplex<std::remove_cvref_t<T>>::value;

template<typename T>
struct ScalarType {
    using type = std::remove_cvref_t<T>;
};

template<typename T>
struct ScalarType<std::complex<T>> {
    using type = T;
};

template<typename T>
using ScalarTypeT = typename ScalarType<std::remove_cvref_t<T>>::type;

template<typename>
inline constexpr bool always_false_v = false;

template<typename T>
constexpr f128 default_relative_tolerance () noexcept {
    using Scalar = ScalarTypeT<T>;
    if constexpr (std::is_integral_v<Scalar>) {
        return 0.0L;
    } else if constexpr (std::is_same_v<Scalar, f16>) {
        return 1.0e-3L;
    } else if constexpr (std::is_same_v<Scalar, f32>) {
        return 1.0e-5L;
    } else if constexpr (std::is_same_v<Scalar, f64> || std::is_same_v<Scalar, f128>) {
        return 1.0e-9L;
    } else {
        static_assert(always_false_v<Scalar>, "Unsupported numeric::almost_equal scalar type");
    }
}

template<typename T, typename U>
bool floating_almost_equal (T left, U right) noexcept {
    const f128 left_value  = static_cast<f128>(left);
    const f128 right_value = static_cast<f128>(right);
    if (!std::isfinite(left_value) || !std::isfinite(right_value)) {
        return false;
    }

    const f128 rtol  = std::max(default_relative_tolerance<T>(), default_relative_tolerance<U>());
    const f128 diff  = std::fabs(left_value - right_value);
    const f128 scale = std::max(std::fabs(left_value), std::fabs(right_value));
    return diff <= rtol * scale;
}

} // namespace detail

template<typename T, typename U>
bool almost_equal (const T& left, const U& right) noexcept {
    using Left  = std::remove_cvref_t<T>;
    using Right = std::remove_cvref_t<U>;

    if constexpr (detail::is_std_complex_v<Left> && detail::is_std_complex_v<Right>) {
        return almost_equal(left.real(), right.real()) && almost_equal(left.imag(), right.imag());
    } else if constexpr (detail::is_std_complex_v<Left>) {
        return almost_equal(left.real(), right) && almost_equal(left.imag(), 0);
    } else if constexpr (detail::is_std_complex_v<Right>) {
        return almost_equal(left, right.real()) && almost_equal(0, right.imag());
    } else if constexpr (std::is_integral_v<Left> && std::is_integral_v<Right>) {
        return std::cmp_equal(left, right);
    } else if constexpr (std::is_floating_point_v<Left> || std::is_floating_point_v<Right>) {
        return detail::floating_almost_equal(left, right);
    } else {
        static_assert(detail::always_false_v<Left>, "Unsupported numeric::almost_equal type");
    }
}

} // namespace numeric
