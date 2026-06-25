#pragma once

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <utility>

#include "types.h"

struct Float16Bits {
    u16 value = 0;
};

class Float16 final {
  public:
    constexpr Float16 () noexcept = default;
    explicit constexpr Float16 (Float16Bits bits) noexcept: bits_(bits.value) {}

    explicit constexpr Float16 (bool value) noexcept;
    explicit constexpr Float16 (char value) noexcept;
    explicit constexpr Float16 (signed char value) noexcept;
    explicit constexpr Float16 (unsigned char value) noexcept;
    explicit constexpr Float16 (wchar_t value) noexcept;
    explicit constexpr Float16 (char8_t value) noexcept;
    explicit constexpr Float16 (char16_t value) noexcept;
    explicit constexpr Float16 (char32_t value) noexcept;
    explicit constexpr Float16 (short value) noexcept;
    explicit constexpr Float16 (unsigned short value) noexcept;
    explicit constexpr Float16 (int value) noexcept;
    explicit constexpr Float16 (unsigned int value) noexcept;
    explicit constexpr Float16 (long value) noexcept;
    explicit constexpr Float16 (unsigned long value) noexcept;
    explicit constexpr Float16 (long long value) noexcept;
    explicit constexpr Float16 (unsigned long long value) noexcept;
    explicit constexpr Float16 (f32 value) noexcept;
    explicit constexpr Float16 (f64 value) noexcept;
    explicit constexpr Float16 (f128 value) noexcept;

    explicit constexpr operator bool () const noexcept;
    explicit constexpr operator char () const noexcept;
    explicit constexpr operator signed char () const noexcept;
    explicit constexpr operator unsigned char () const noexcept;
    explicit constexpr operator wchar_t () const noexcept;
    explicit constexpr operator char8_t () const noexcept;
    explicit constexpr operator char16_t () const noexcept;
    explicit constexpr operator char32_t () const noexcept;
    explicit constexpr operator short () const noexcept;
    explicit constexpr operator unsigned short () const noexcept;
    explicit constexpr operator int () const noexcept;
    explicit constexpr operator unsigned int () const noexcept;
    explicit constexpr operator long () const noexcept;
    explicit constexpr operator unsigned long () const noexcept;
    explicit constexpr operator long long () const noexcept;
    explicit constexpr operator unsigned long long () const noexcept;
    explicit constexpr operator f32 () const noexcept;
    explicit constexpr operator f64 () const noexcept;
    explicit constexpr operator f128 () const noexcept;

    constexpr u16 bits () const noexcept { return bits_; }

  private:
    u16 bits_ = 0;
};

constexpr Float16 operator +(Float16 value) noexcept;
constexpr Float16 operator -(Float16 value) noexcept;

constexpr Float16 operator +(Float16 left, Float16 right) noexcept;
constexpr Float16 operator -(Float16 left, Float16 right) noexcept;
constexpr Float16 operator *(Float16 left, Float16 right) noexcept;
constexpr Float16 operator /(Float16 left, Float16 right) noexcept;

constexpr Float16& operator +=(Float16& left, Float16 right) noexcept;
constexpr Float16& operator -=(Float16& left, Float16 right) noexcept;
constexpr Float16& operator *=(Float16& left, Float16 right) noexcept;
constexpr Float16& operator /=(Float16& left, Float16 right) noexcept;

constexpr Float16& operator ++(Float16& value) noexcept;
constexpr Float16  operator ++(Float16& value, int) noexcept;
constexpr Float16& operator --(Float16& value) noexcept;
constexpr Float16  operator --(Float16& value, int) noexcept;

constexpr bool operator ==(Float16 left, Float16 right) noexcept;
constexpr bool operator !=(Float16 left, Float16 right) noexcept;
constexpr bool operator <(Float16 left, Float16 right) noexcept;
constexpr bool operator <=(Float16 left, Float16 right) noexcept;
constexpr bool operator >(Float16 left, Float16 right) noexcept;
constexpr bool operator >=(Float16 left, Float16 right) noexcept;

namespace std {

// Intentional: Float16 participates in the same generic floating-point paths as f32 and f64.
template<>
struct is_floating_point<Float16>: true_type {};

template<>
class numeric_limits<Float16> {
  public:
    static constexpr bool is_specialized = true;

    static constexpr Float16 min () noexcept { return Float16(Float16Bits {0x0400U}); }
    static constexpr Float16 max () noexcept { return Float16(Float16Bits {0x7BFFU}); }
    static constexpr Float16 lowest () noexcept { return Float16(Float16Bits {0xFBFFU}); }

    static constexpr int digits       = 11;
    static constexpr int digits10     = 3;
    static constexpr int max_digits10 = 5;

    static constexpr bool is_signed  = true;
    static constexpr bool is_integer = false;
    static constexpr bool is_exact   = false;
    static constexpr int  radix      = 2;

    static constexpr Float16 epsilon () noexcept { return Float16(Float16Bits {0x1400U}); }
    static constexpr Float16 round_error () noexcept { return Float16(Float16Bits {0x3800U}); }

    static constexpr int min_exponent   = -13;
    static constexpr int min_exponent10 = -4;
    static constexpr int max_exponent   = 16;
    static constexpr int max_exponent10 = 4;

    static constexpr bool               has_infinity      = true;
    static constexpr bool               has_quiet_NaN     = true;
    static constexpr bool               has_signaling_NaN = true;
    static constexpr float_denorm_style has_denorm        = denorm_present;
    static constexpr bool               has_denorm_loss   = false;

    static constexpr Float16 infinity () noexcept { return Float16(Float16Bits {0x7C00U}); }
    static constexpr Float16 quiet_NaN () noexcept { return Float16(Float16Bits {0x7E00U}); }
    static constexpr Float16 signaling_NaN () noexcept { return Float16(Float16Bits {0x7D00U}); }
    static constexpr Float16 denorm_min () noexcept { return Float16(Float16Bits {0x0001U}); }

    static constexpr bool is_iec559  = true;
    static constexpr bool is_bounded = true;
    static constexpr bool is_modulo  = false;

    static constexpr bool              traps           = false;
    static constexpr bool              tinyness_before = false;
    static constexpr float_round_style round_style     = round_to_nearest;
};

constexpr Float16 abs (Float16 value) noexcept;
constexpr bool    isfinite (Float16 value) noexcept;
constexpr bool    isnan (Float16 value) noexcept;
constexpr bool    isinf (Float16 value) noexcept;
constexpr bool    signbit (Float16 value) noexcept;
constexpr int     fpclassify (Float16 value) noexcept;

} // namespace std

namespace float16_detail {

inline constexpr u16 kSignMask      = 0x8000U;
inline constexpr u16 kExponentMask  = 0x7C00U;
inline constexpr u16 kFractionMask  = 0x03FFU;
inline constexpr u16 kQuietNaN      = 0x7E00U;
inline constexpr u16 kPositiveOne   = 0x3C00U;
inline constexpr int kExponentBias  = 15;
inline constexpr int kFractionBits  = 10;
inline constexpr int kMaxExponent   = 15;
inline constexpr int kMinNormalExp  = -14;
inline constexpr int kDivisionGuard = 42;

struct Components {
    bool negative    = false;
    u64  significand = 0;
    int  exponent    = 0;
};

template<typename T>
using NativeArithmeticType = std::remove_cv_t<std::remove_reference_t<T>>;

template<typename T>
inline constexpr bool kNativeArithmetic =
    std::is_arithmetic_v<NativeArithmeticType<T>> && !std::is_same_v<NativeArithmeticType<T>, Float16>;

template<typename T>
inline constexpr bool kNativeIntegral = kNativeArithmetic<T> && std::is_integral_v<NativeArithmeticType<T>>;

template<typename T>
inline constexpr bool kNativeFloating = kNativeArithmetic<T> && std::is_floating_point_v<NativeArithmeticType<T>>;

constexpr inline Float16 from_bits (u16 bits) noexcept { return Float16(Float16Bits {bits}); }

constexpr inline u16 sign_bits (Float16 value) noexcept { return static_cast<u16>(value.bits() & kSignMask); }

constexpr inline u16 exponent_bits (Float16 value) noexcept { return static_cast<u16>(value.bits() & kExponentMask); }

constexpr inline u16 fraction_bits (Float16 value) noexcept { return static_cast<u16>(value.bits() & kFractionMask); }

constexpr inline bool sign_bit (Float16 value) noexcept { return sign_bits(value) != 0U; }

constexpr inline bool is_nan_bits (Float16 value) noexcept {
    return exponent_bits(value) == kExponentMask && fraction_bits(value) != 0U;
}

constexpr inline bool is_inf_bits (Float16 value) noexcept {
    return exponent_bits(value) == kExponentMask && fraction_bits(value) == 0U;
}

constexpr inline bool is_zero_bits (Float16 value) noexcept {
    return (value.bits() & static_cast<u16>(~kSignMask)) == 0U;
}

constexpr inline u64 round_shift_right (u64 value, long long shift) noexcept {
    if (shift <= 0) {
        const auto left_shift = static_cast<unsigned long long>(-shift);
        if (left_shift >= std::numeric_limits<u64>::digits) {
            return std::numeric_limits<u64>::max();
        }
        if (value > (std::numeric_limits<u64>::max() >> left_shift)) {
            return std::numeric_limits<u64>::max();
        }
        return value << left_shift;
    }
    if (shift > std::numeric_limits<u64>::digits) {
        return 0;
    }
    if (shift == std::numeric_limits<u64>::digits) {
        constexpr u64 half = 1ULL << 63U;
        return value > half ? 1ULL : 0ULL;
    }

    const auto right_shift = static_cast<unsigned int>(shift);
    const u64  quotient    = value >> right_shift;
    const u64  mask        = (1ULL << right_shift) - 1ULL;
    const u64  rem         = value & mask;
    const u64  half        = 1ULL << (right_shift - 1U);
    if (rem > half || (rem == half && (quotient & 1ULL) != 0ULL)) {
        return quotient + 1ULL;
    }
    return quotient;
}

constexpr inline u64 shifted_significand (const Components& components, int common_exponent) noexcept {
    const long long shift = static_cast<long long>(components.exponent) - static_cast<long long>(common_exponent);
    if (shift <= 0) {
        return components.significand;
    }
    if (shift >= std::numeric_limits<u64>::digits) {
        return std::numeric_limits<u64>::max();
    }
    const auto left_shift = static_cast<unsigned int>(shift);
    if (components.significand > (std::numeric_limits<u64>::max() >> left_shift)) {
        return std::numeric_limits<u64>::max();
    }
    return components.significand << left_shift;
}

constexpr inline Float16 pack_components (bool negative, u64 significand, int exponent) noexcept {
    const u16 sign = negative ? kSignMask : 0U;
    if (significand == 0ULL) {
        return from_bits(sign);
    }

    const int highest_bit = static_cast<int>(std::bit_width(significand)) - 1;
    int       binary_exp  = exponent + highest_bit;

    if (binary_exp < kMinNormalExp) {
        const long long subnorm_shift = -(static_cast<long long>(exponent) + 24LL);
        const u64       rounded       = round_shift_right(significand, subnorm_shift);
        if (rounded == 0ULL) {
            return from_bits(sign);
        }
        if (rounded >= 0x0400ULL) {
            return from_bits(static_cast<u16>(sign | 0x0400U));
        }
        return from_bits(static_cast<u16>(sign | static_cast<u16>(rounded)));
    }

    if (binary_exp > kMaxExponent) {
        return from_bits(static_cast<u16>(sign | kExponentMask));
    }

    const long long shift   = static_cast<long long>(binary_exp) - kFractionBits - exponent;
    u64             rounded = round_shift_right(significand, shift);
    if (rounded == 0x0800ULL) {
        rounded = 0x0400ULL;
        ++binary_exp;
    }
    if (binary_exp > kMaxExponent) {
        return from_bits(static_cast<u16>(sign | kExponentMask));
    }

    const auto encoded_exponent = static_cast<u16>(binary_exp + kExponentBias);
    const auto encoded_fraction = static_cast<u16>(rounded - 0x0400ULL);
    return from_bits(static_cast<u16>(sign | static_cast<u16>(encoded_exponent << kFractionBits) | encoded_fraction));
}

constexpr inline Float16 pack_unsigned_integer (bool negative, unsigned long long magnitude) noexcept {
    return pack_components(negative, static_cast<u64>(magnitude), 0);
}

template<typename T>
constexpr inline Float16 pack_signed_integer (T value) noexcept {
    using Unsigned = std::make_unsigned_t<T>;
    if (value < 0) {
        const auto magnitude = static_cast<Unsigned>(-(value + 1));
        return pack_unsigned_integer(true, static_cast<unsigned long long>(magnitude) + 1ULL);
    }
    return pack_unsigned_integer(false, static_cast<unsigned long long>(value));
}

template<typename T>
constexpr inline Float16 pack_integral (T value) noexcept {
    if constexpr (std::is_same_v<T, bool>) {
        return pack_unsigned_integer(false, value ? 1ULL : 0ULL);
    } else if constexpr (std::is_signed_v<T>) {
        return pack_signed_integer(value);
    } else {
        return pack_unsigned_integer(false, static_cast<unsigned long long>(value));
    }
}

constexpr inline Float16 pack_float_bits (u32 bits) noexcept {
    const bool negative = (bits & 0x80000000UL) != 0UL;
    const u32  exponent = (bits >> 23U) & 0xFFU;
    const u32  fraction = bits & 0x007FFFFFUL;

    if (exponent == 0xFFU) {
        return from_bits(static_cast<u16>((negative ? kSignMask : 0U) | (fraction == 0U ? kExponentMask : kQuietNaN)));
    }
    if (exponent == 0U) {
        return pack_components(negative, static_cast<u64>(fraction), -149);
    }
    return pack_components(negative, static_cast<u64>((1UL << 23U) | fraction), static_cast<int>(exponent) - 127 - 23);
}

constexpr inline Float16 pack_double_bits (u64 bits) noexcept {
    const bool negative = (bits & 0x8000000000000000ULL) != 0ULL;
    const u64  exponent = (bits >> 52U) & 0x7FFULL;
    const u64  fraction = bits & 0x000FFFFFFFFFFFFFULL;

    if (exponent == 0x7FFULL) {
        return from_bits(
            static_cast<u16>((negative ? kSignMask : 0U) | (fraction == 0ULL ? kExponentMask : kQuietNaN)));
    }
    if (exponent == 0ULL) {
        return pack_components(negative, fraction, -1074);
    }
    return pack_components(negative, (1ULL << 52U) | fraction, static_cast<int>(exponent) - 1023 - 52);
}

constexpr inline bool long_double_sign_bit (f128 value) noexcept { return std::signbit(value); }

constexpr inline Float16 pack_long_double_constexpr (f128 value) noexcept {
    const bool negative = long_double_sign_bit(value);
    const u16  sign     = negative ? kSignMask : 0U;

    if (value != value) {
        return from_bits(static_cast<u16>(sign | kQuietNaN));
    }

    constexpr f128 infinity = std::numeric_limits<f128>::infinity();
    if (value == infinity || value == -infinity) {
        return from_bits(static_cast<u16>(sign | kExponentMask));
    }

    f128 magnitude = negative ? -value : value;
    if (magnitude == 0.0L) {
        return from_bits(sign);
    }

    int exponent = 0;
    while (magnitude >= 1.0L) {
        magnitude *= 0.5L;
        ++exponent;
    }
    while (magnitude < 0.5L) {
        magnitude *= 2.0L;
        --exponent;
    }

    f128 scaled_value = magnitude;
    for (int idx = 0; idx < 63; ++idx) {
        scaled_value *= 2.0L;
    }
    return pack_components(negative, static_cast<unsigned long long>(scaled_value), exponent - 63);
}

inline Float16 pack_long_double_runtime (f128 value) noexcept {
    const bool negative = std::signbit(value);
    const u16  sign     = negative ? kSignMask : 0U;

    if (std::isnan(value)) {
        return from_bits(static_cast<u16>(sign | kQuietNaN));
    }
    if (std::isinf(value)) {
        return from_bits(static_cast<u16>(sign | kExponentMask));
    }

    const f128 magnitude = std::fabs(value);
    if (magnitude == 0.0L) {
        return from_bits(sign);
    }

    int        exponent     = 0;
    const f128 normalized   = std::frexp(magnitude, &exponent);
    const f128 scaled_value = std::ldexp(normalized, 63);
    return pack_components(negative, static_cast<unsigned long long>(scaled_value), exponent - 63);
}

constexpr inline Float16 pack_long_double (f128 value) noexcept {
    if consteval {
        return pack_long_double_constexpr(value);
    } else {
        return pack_long_double_runtime(value);
    }
}

constexpr inline Components decode_finite (Float16 value) noexcept {
    const u16 exponent = static_cast<u16>((value.bits() & kExponentMask) >> kFractionBits);
    const u16 fraction = fraction_bits(value);
    if (exponent == 0U) {
        return {sign_bit(value), static_cast<u64>(fraction), -24};
    }
    return {
        sign_bit(value),
        static_cast<u64>(0x0400U | fraction),
        static_cast<int>(exponent) - kExponentBias - kFractionBits,
    };
}

constexpr inline Float16 quiet_nan () noexcept { return from_bits(kQuietNaN); }

constexpr inline Float16 infinity (bool negative) noexcept {
    return from_bits(static_cast<u16>((negative ? kSignMask : 0U) | kExponentMask));
}

constexpr inline Float16 zero (bool negative) noexcept { return from_bits(negative ? kSignMask : 0U); }

constexpr inline Float16 add_finite (Float16 left, Float16 right) noexcept {
    Components left_components  = decode_finite(left);
    Components right_components = decode_finite(right);
    if (left_components.significand == 0ULL) {
        return right;
    }
    if (right_components.significand == 0ULL) {
        return left;
    }

    const int common_exponent = std::min(left_components.exponent, right_components.exponent);
    const u64 left_magnitude  = shifted_significand(left_components, common_exponent);
    const u64 right_magnitude = shifted_significand(right_components, common_exponent);
    if (left_components.negative == right_components.negative) {
        if (std::numeric_limits<u64>::max() - left_magnitude < right_magnitude) {
            return infinity(left_components.negative);
        }
        return pack_components(left_components.negative, left_magnitude + right_magnitude, common_exponent);
    }

    if (left_magnitude >= right_magnitude) {
        return pack_components(left_components.negative, left_magnitude - right_magnitude, common_exponent);
    }
    return pack_components(right_components.negative, right_magnitude - left_magnitude, common_exponent);
}

constexpr inline Float16 multiply_finite (Float16 left, Float16 right) noexcept {
    const Components left_components  = decode_finite(left);
    const Components right_components = decode_finite(right);
    return pack_components(left_components.negative != right_components.negative,
                           left_components.significand * right_components.significand,
                           left_components.exponent + right_components.exponent);
}

constexpr inline Float16 divide_finite (Float16 left, Float16 right) noexcept {
    const Components left_components  = decode_finite(left);
    const Components right_components = decode_finite(right);
    const auto       numerator        = static_cast<u64>(left_components.significand << kDivisionGuard);
    auto             quotient         = static_cast<u64>(numerator / right_components.significand);
    const auto       remainder        = static_cast<u64>(numerator % right_components.significand);
    if (remainder != 0ULL) {
        quotient |= 1ULL;
    }
    return pack_components(left_components.negative != right_components.negative, quotient,
                           left_components.exponent - right_components.exponent - kDivisionGuard);
}

constexpr inline u64 integer_magnitude (Float16 value) noexcept {
    if (is_nan_bits(value) || is_inf_bits(value)) {
        return std::numeric_limits<u64>::max();
    }

    const Components components = decode_finite(value);
    if (components.exponent >= 0) {
        if (components.exponent >= 64) {
            return std::numeric_limits<u64>::max();
        }
        return components.significand << components.exponent;
    }
    if (components.exponent <= -64) {
        return 0ULL;
    }
    return components.significand >> -components.exponent;
}

template<typename T>
constexpr inline T cast_to_unsigned_integer (Float16 value) noexcept {
    if (sign_bit(value)) {
        return 0;
    }
    const u64  magnitude = integer_magnitude(value);
    const auto maximum   = static_cast<u64>(std::numeric_limits<T>::max());
    return magnitude > maximum ? std::numeric_limits<T>::max() : static_cast<T>(magnitude);
}

template<typename T>
constexpr inline T cast_to_signed_integer (Float16 value) noexcept {
    const u64 magnitude = integer_magnitude(value);
    if (sign_bit(value)) {
        const auto limit = static_cast<u64>(std::numeric_limits<T>::max()) + 1ULL;
        if (magnitude >= limit) {
            return std::numeric_limits<T>::min();
        }
        return static_cast<T>(-static_cast<long long>(magnitude));
    }
    const auto maximum = static_cast<u64>(std::numeric_limits<T>::max());
    return magnitude > maximum ? std::numeric_limits<T>::max() : static_cast<T>(magnitude);
}

template<typename T>
constexpr inline T cast_to_integral (Float16 value) noexcept {
    if constexpr (std::is_same_v<T, bool>) {
        return !is_zero_bits(value) && !is_nan_bits(value);
    } else if constexpr (std::is_signed_v<T>) {
        return cast_to_signed_integer<T>(value);
    } else {
        return cast_to_unsigned_integer<T>(value);
    }
}

constexpr inline u32 float_bits (Float16 value) noexcept {
    const u32 sign     = static_cast<u32>(sign_bits(value)) << 16U;
    const u16 exponent = static_cast<u16>((value.bits() & kExponentMask) >> kFractionBits);
    const u16 fraction = fraction_bits(value);
    if (exponent == 0x1FU) {
        return sign | 0x7F800000UL | static_cast<u32>(fraction == 0U ? 0UL : 0x00400000UL);
    }
    if (exponent == 0U) {
        if (fraction == 0U) {
            return sign;
        }
        const int shift =
            std::countl_zero(static_cast<unsigned int>(fraction)) - (std::numeric_limits<unsigned int>::digits - 10);
        const auto normalized_fraction = static_cast<u32>(fraction << (shift + 1));
        const auto encoded_exponent    = static_cast<u32>(127 - 14 - shift);
        return sign | (encoded_exponent << 23U) | ((normalized_fraction & kFractionMask) << 13U);
    }
    const auto encoded_exponent = static_cast<u32>(static_cast<int>(exponent) - kExponentBias + 127);
    return sign | (encoded_exponent << 23U) | (static_cast<u32>(fraction) << 13U);
}

constexpr inline u64 double_bits (Float16 value) noexcept {
    const u64 sign     = static_cast<u64>(sign_bits(value)) << 48U;
    const u16 exponent = static_cast<u16>((value.bits() & kExponentMask) >> kFractionBits);
    const u16 fraction = fraction_bits(value);
    if (exponent == 0x1FU) {
        return sign | 0x7FF0000000000000ULL | (fraction == 0U ? 0ULL : 0x0008000000000000ULL);
    }
    if (exponent == 0U) {
        if (fraction == 0U) {
            return sign;
        }
        const int shift =
            std::countl_zero(static_cast<unsigned int>(fraction)) - (std::numeric_limits<unsigned int>::digits - 10);
        const auto normalized_fraction = static_cast<u64>(fraction << (shift + 1));
        const auto encoded_exponent    = static_cast<u64>(1023 - 14 - shift);
        return sign | (encoded_exponent << 52U) | ((normalized_fraction & kFractionMask) << 42U);
    }
    const auto encoded_exponent = static_cast<u64>(static_cast<int>(exponent) - kExponentBias + 1023);
    return sign | (encoded_exponent << 52U) | (static_cast<u64>(fraction) << 42U);
}

constexpr inline f128 long_double_value (Float16 value) noexcept {
    if (is_nan_bits(value)) {
        return sign_bit(value) ? -std::numeric_limits<f128>::quiet_NaN() : std::numeric_limits<f128>::quiet_NaN();
    }
    if (is_inf_bits(value)) {
        return sign_bit(value) ? -std::numeric_limits<f128>::infinity() : std::numeric_limits<f128>::infinity();
    }
    const Components components = decode_finite(value);
    f128             magnitude  = static_cast<f128>(components.significand);
    if (components.exponent >= 0) {
        for (int idx = 0; idx < components.exponent; ++idx) {
            magnitude *= 2.0L;
        }
    } else {
        for (int idx = 0; idx < -components.exponent; ++idx) {
            magnitude *= 0.5L;
        }
    }
    return components.negative ? -magnitude : magnitude;
}

} // namespace float16_detail

constexpr inline Float16::Float16 (bool value) noexcept: bits_(float16_detail::pack_integral(value).bits()) {}
constexpr inline Float16::Float16 (char value) noexcept: bits_(float16_detail::pack_integral(value).bits()) {}
constexpr inline Float16::Float16 (signed char value) noexcept: bits_(float16_detail::pack_integral(value).bits()) {}
constexpr inline Float16::Float16 (unsigned char value) noexcept: bits_(float16_detail::pack_integral(value).bits()) {}
constexpr inline Float16::Float16 (wchar_t value) noexcept: bits_(float16_detail::pack_integral(value).bits()) {}
constexpr inline Float16::Float16 (char8_t value) noexcept: bits_(float16_detail::pack_integral(value).bits()) {}
constexpr inline Float16::Float16 (char16_t value) noexcept: bits_(float16_detail::pack_integral(value).bits()) {}
constexpr inline Float16::Float16 (char32_t value) noexcept: bits_(float16_detail::pack_integral(value).bits()) {}
constexpr inline Float16::Float16 (short value) noexcept: bits_(float16_detail::pack_integral(value).bits()) {}
constexpr inline Float16::Float16 (unsigned short value) noexcept: bits_(float16_detail::pack_integral(value).bits()) {}
constexpr inline Float16::Float16 (int value) noexcept: bits_(float16_detail::pack_integral(value).bits()) {}
constexpr inline Float16::Float16 (unsigned int value) noexcept: bits_(float16_detail::pack_integral(value).bits()) {}
constexpr inline Float16::Float16 (long value) noexcept: bits_(float16_detail::pack_integral(value).bits()) {}
constexpr inline Float16::Float16 (unsigned long value) noexcept: bits_(float16_detail::pack_integral(value).bits()) {}
constexpr inline Float16::Float16 (long long value) noexcept: bits_(float16_detail::pack_integral(value).bits()) {}
constexpr inline Float16::Float16 (unsigned long long value) noexcept
    : bits_(float16_detail::pack_integral(value).bits()) {}
constexpr inline Float16::Float16 (f32 value) noexcept
    : bits_(float16_detail::pack_float_bits(std::bit_cast<u32>(value)).bits()) {}
constexpr inline Float16::Float16 (f64 value) noexcept
    : bits_(float16_detail::pack_double_bits(std::bit_cast<u64>(value)).bits()) {}
constexpr inline Float16::Float16 (f128 value) noexcept: bits_(float16_detail::pack_long_double(value).bits()) {}

constexpr inline Float16::operator bool () const noexcept { return float16_detail::cast_to_integral<bool>(*this); }
constexpr inline Float16::operator char () const noexcept { return float16_detail::cast_to_integral<char>(*this); }
constexpr inline Float16::operator signed char () const noexcept {
    return float16_detail::cast_to_integral<signed char>(*this);
}
constexpr inline Float16::operator unsigned char () const noexcept {
    return float16_detail::cast_to_integral<unsigned char>(*this);
}
constexpr inline Float16::operator wchar_t () const noexcept {
    return float16_detail::cast_to_integral<wchar_t>(*this);
}
constexpr inline Float16::operator char8_t () const noexcept {
    return float16_detail::cast_to_integral<char8_t>(*this);
}
constexpr inline Float16::operator char16_t () const noexcept {
    return float16_detail::cast_to_integral<char16_t>(*this);
}
constexpr inline Float16::operator char32_t () const noexcept {
    return float16_detail::cast_to_integral<char32_t>(*this);
}
constexpr inline Float16::operator short () const noexcept { return float16_detail::cast_to_integral<short>(*this); }
constexpr inline Float16::operator unsigned short () const noexcept {
    return float16_detail::cast_to_integral<unsigned short>(*this);
}
constexpr inline Float16::operator int () const noexcept { return float16_detail::cast_to_integral<int>(*this); }
constexpr inline Float16::operator unsigned int () const noexcept {
    return float16_detail::cast_to_integral<unsigned int>(*this);
}
constexpr inline Float16::operator long () const noexcept { return float16_detail::cast_to_integral<long>(*this); }
constexpr inline Float16::operator unsigned long () const noexcept {
    return float16_detail::cast_to_integral<unsigned long>(*this);
}
constexpr inline Float16::operator long long () const noexcept {
    return float16_detail::cast_to_integral<long long>(*this);
}
constexpr inline Float16::operator unsigned long long () const noexcept {
    return float16_detail::cast_to_integral<unsigned long long>(*this);
}
constexpr inline Float16::operator f32 () const noexcept {
    return std::bit_cast<f32>(float16_detail::float_bits(*this));
}
constexpr inline Float16::operator f64 () const noexcept {
    return std::bit_cast<f64>(float16_detail::double_bits(*this));
}
constexpr inline Float16::operator f128 () const noexcept { return float16_detail::long_double_value(*this); }

constexpr inline Float16 operator +(Float16 value) noexcept { return value; }

constexpr inline Float16 operator -(Float16 value) noexcept {
    return float16_detail::from_bits(static_cast<u16>(value.bits() ^ float16_detail::kSignMask));
}

constexpr inline Float16 operator +(Float16 left, Float16 right) noexcept {
    if (float16_detail::is_nan_bits(left) || float16_detail::is_nan_bits(right)) {
        return float16_detail::quiet_nan();
    }
    if (float16_detail::is_inf_bits(left) || float16_detail::is_inf_bits(right)) {
        if (float16_detail::is_inf_bits(left) && float16_detail::is_inf_bits(right) &&
            float16_detail::sign_bit(left) != float16_detail::sign_bit(right)) {
            return float16_detail::quiet_nan();
        }
        return float16_detail::is_inf_bits(left) ? left : right;
    }
    return float16_detail::add_finite(left, right);
}

constexpr inline Float16 operator -(Float16 left, Float16 right) noexcept { return left + -right; }

constexpr inline Float16 operator *(Float16 left, Float16 right) noexcept {
    if (float16_detail::is_nan_bits(left) || float16_detail::is_nan_bits(right)) {
        return float16_detail::quiet_nan();
    }
    const bool negative = float16_detail::sign_bit(left) != float16_detail::sign_bit(right);
    if ((float16_detail::is_inf_bits(left) && float16_detail::is_zero_bits(right)) ||
        (float16_detail::is_zero_bits(left) && float16_detail::is_inf_bits(right))) {
        return float16_detail::quiet_nan();
    }
    if (float16_detail::is_inf_bits(left) || float16_detail::is_inf_bits(right)) {
        return float16_detail::infinity(negative);
    }
    if (float16_detail::is_zero_bits(left) || float16_detail::is_zero_bits(right)) {
        return float16_detail::zero(negative);
    }
    return float16_detail::multiply_finite(left, right);
}

constexpr inline Float16 operator /(Float16 left, Float16 right) noexcept {
    if (float16_detail::is_nan_bits(left) || float16_detail::is_nan_bits(right)) {
        return float16_detail::quiet_nan();
    }
    const bool negative = float16_detail::sign_bit(left) != float16_detail::sign_bit(right);
    if ((float16_detail::is_zero_bits(left) && float16_detail::is_zero_bits(right)) ||
        (float16_detail::is_inf_bits(left) && float16_detail::is_inf_bits(right))) {
        return float16_detail::quiet_nan();
    }
    if (float16_detail::is_inf_bits(left) || float16_detail::is_zero_bits(right)) {
        return float16_detail::infinity(negative);
    }
    if (float16_detail::is_zero_bits(left) || float16_detail::is_inf_bits(right)) {
        return float16_detail::zero(negative);
    }
    return float16_detail::divide_finite(left, right);
}

template<typename T, std::enable_if_t<float16_detail::kNativeIntegral<T>, int> = 0>
constexpr inline Float16 operator +(Float16 left, T right) noexcept {
    return left + static_cast<Float16>(right);
}

template<typename T, std::enable_if_t<float16_detail::kNativeIntegral<T>, int> = 0>
constexpr inline Float16 operator +(T left, Float16 right) noexcept {
    return static_cast<Float16>(left) + right;
}

template<typename T, std::enable_if_t<float16_detail::kNativeIntegral<T>, int> = 0>
constexpr inline Float16 operator -(Float16 left, T right) noexcept {
    return left - static_cast<Float16>(right);
}

template<typename T, std::enable_if_t<float16_detail::kNativeIntegral<T>, int> = 0>
constexpr inline Float16 operator -(T left, Float16 right) noexcept {
    return static_cast<Float16>(left) - right;
}

template<typename T, std::enable_if_t<float16_detail::kNativeIntegral<T>, int> = 0>
constexpr inline Float16 operator *(Float16 left, T right) noexcept {
    return left * static_cast<Float16>(right);
}

template<typename T, std::enable_if_t<float16_detail::kNativeIntegral<T>, int> = 0>
constexpr inline Float16 operator *(T left, Float16 right) noexcept {
    return static_cast<Float16>(left) * right;
}

template<typename T, std::enable_if_t<float16_detail::kNativeIntegral<T>, int> = 0>
constexpr inline Float16 operator /(Float16 left, T right) noexcept {
    return left / static_cast<Float16>(right);
}

template<typename T, std::enable_if_t<float16_detail::kNativeIntegral<T>, int> = 0>
constexpr inline Float16 operator /(T left, Float16 right) noexcept {
    return static_cast<Float16>(left) / right;
}

template<typename T, std::enable_if_t<float16_detail::kNativeFloating<T>, int> = 0>
constexpr inline float16_detail::NativeArithmeticType<T> operator +(Float16 left, T right) noexcept {
    using Result = float16_detail::NativeArithmeticType<T>;
    return static_cast<Result>(left) + static_cast<Result>(right);
}

template<typename T, std::enable_if_t<float16_detail::kNativeFloating<T>, int> = 0>
constexpr inline float16_detail::NativeArithmeticType<T> operator +(T left, Float16 right) noexcept {
    using Result = float16_detail::NativeArithmeticType<T>;
    return static_cast<Result>(left) + static_cast<Result>(right);
}

template<typename T, std::enable_if_t<float16_detail::kNativeFloating<T>, int> = 0>
constexpr inline float16_detail::NativeArithmeticType<T> operator -(Float16 left, T right) noexcept {
    using Result = float16_detail::NativeArithmeticType<T>;
    return static_cast<Result>(left) - static_cast<Result>(right);
}

template<typename T, std::enable_if_t<float16_detail::kNativeFloating<T>, int> = 0>
constexpr inline float16_detail::NativeArithmeticType<T> operator -(T left, Float16 right) noexcept {
    using Result = float16_detail::NativeArithmeticType<T>;
    return static_cast<Result>(left) - static_cast<Result>(right);
}

template<typename T, std::enable_if_t<float16_detail::kNativeFloating<T>, int> = 0>
constexpr inline float16_detail::NativeArithmeticType<T> operator *(Float16 left, T right) noexcept {
    using Result = float16_detail::NativeArithmeticType<T>;
    return static_cast<Result>(left) * static_cast<Result>(right);
}

template<typename T, std::enable_if_t<float16_detail::kNativeFloating<T>, int> = 0>
constexpr inline float16_detail::NativeArithmeticType<T> operator *(T left, Float16 right) noexcept {
    using Result = float16_detail::NativeArithmeticType<T>;
    return static_cast<Result>(left) * static_cast<Result>(right);
}

template<typename T, std::enable_if_t<float16_detail::kNativeFloating<T>, int> = 0>
constexpr inline float16_detail::NativeArithmeticType<T> operator /(Float16 left, T right) noexcept {
    using Result = float16_detail::NativeArithmeticType<T>;
    return static_cast<Result>(left) / static_cast<Result>(right);
}

template<typename T, std::enable_if_t<float16_detail::kNativeFloating<T>, int> = 0>
constexpr inline float16_detail::NativeArithmeticType<T> operator /(T left, Float16 right) noexcept {
    using Result = float16_detail::NativeArithmeticType<T>;
    return static_cast<Result>(left) / static_cast<Result>(right);
}

constexpr inline Float16& operator +=(Float16& left, Float16 right) noexcept {
    left = left + right;
    return left;
}

constexpr inline Float16& operator -=(Float16& left, Float16 right) noexcept {
    left = left - right;
    return left;
}

constexpr inline Float16& operator *=(Float16& left, Float16 right) noexcept {
    left = left * right;
    return left;
}

constexpr inline Float16& operator /=(Float16& left, Float16 right) noexcept {
    left = left / right;
    return left;
}

template<typename T, std::enable_if_t<float16_detail::kNativeArithmetic<T>, int> = 0>
constexpr inline Float16& operator +=(Float16& left, T right) noexcept {
    left = static_cast<Float16>(left + right);
    return left;
}

template<typename T, std::enable_if_t<float16_detail::kNativeArithmetic<T>, int> = 0>
constexpr inline Float16& operator -=(Float16& left, T right) noexcept {
    left = static_cast<Float16>(left - right);
    return left;
}

template<typename T, std::enable_if_t<float16_detail::kNativeArithmetic<T>, int> = 0>
constexpr inline Float16& operator *=(Float16& left, T right) noexcept {
    left = static_cast<Float16>(left * right);
    return left;
}

template<typename T, std::enable_if_t<float16_detail::kNativeArithmetic<T>, int> = 0>
constexpr inline Float16& operator /=(Float16& left, T right) noexcept {
    left = static_cast<Float16>(left / right);
    return left;
}

constexpr inline Float16& operator ++(Float16& value) noexcept {
    value += float16_detail::from_bits(float16_detail::kPositiveOne);
    return value;
}

constexpr inline Float16 operator ++(Float16& value, int) noexcept {
    const Float16 previous = value;
    ++value;
    return previous;
}

constexpr inline Float16& operator --(Float16& value) noexcept {
    value -= float16_detail::from_bits(float16_detail::kPositiveOne);
    return value;
}

constexpr inline Float16 operator --(Float16& value, int) noexcept {
    const Float16 previous = value;
    --value;
    return previous;
}

constexpr inline bool operator ==(Float16 left, Float16 right) noexcept {
    if (float16_detail::is_nan_bits(left) || float16_detail::is_nan_bits(right)) {
        return false;
    }
    if (float16_detail::is_zero_bits(left) && float16_detail::is_zero_bits(right)) {
        return true;
    }
    return left.bits() == right.bits();
}

constexpr inline bool operator !=(Float16 left, Float16 right) noexcept { return !(left == right); }

constexpr inline bool operator <(Float16 left, Float16 right) noexcept {
    if (float16_detail::is_nan_bits(left) || float16_detail::is_nan_bits(right)) {
        return false;
    }
    if (float16_detail::is_zero_bits(left) && float16_detail::is_zero_bits(right)) {
        return false;
    }
    if (float16_detail::sign_bit(left) != float16_detail::sign_bit(right)) {
        return float16_detail::sign_bit(left);
    }
    const u16 left_magnitude  = static_cast<u16>(left.bits() & static_cast<u16>(~float16_detail::kSignMask));
    const u16 right_magnitude = static_cast<u16>(right.bits() & static_cast<u16>(~float16_detail::kSignMask));
    return float16_detail::sign_bit(left) ? left_magnitude > right_magnitude : left_magnitude < right_magnitude;
}

constexpr inline bool operator <=(Float16 left, Float16 right) noexcept { return left < right || left == right; }

constexpr inline bool operator >(Float16 left, Float16 right) noexcept { return right < left; }

constexpr inline bool operator >=(Float16 left, Float16 right) noexcept { return right < left || left == right; }

template<typename T, std::enable_if_t<float16_detail::kNativeFloating<T>, int> = 0>
constexpr inline bool operator ==(Float16 left, T right) noexcept {
    using Result = float16_detail::NativeArithmeticType<T>;
    return static_cast<Result>(left) == static_cast<Result>(right);
}

template<typename T, std::enable_if_t<float16_detail::kNativeFloating<T>, int> = 0>
constexpr inline bool operator ==(T left, Float16 right) noexcept {
    using Result = float16_detail::NativeArithmeticType<T>;
    return static_cast<Result>(left) == static_cast<Result>(right);
}

template<typename T, std::enable_if_t<float16_detail::kNativeIntegral<T>, int> = 0>
constexpr inline bool operator ==(Float16 left, T right) noexcept {
    return static_cast<f128>(left) == static_cast<f128>(right);
}

template<typename T, std::enable_if_t<float16_detail::kNativeIntegral<T>, int> = 0>
constexpr inline bool operator ==(T left, Float16 right) noexcept {
    return static_cast<f128>(left) == static_cast<f128>(right);
}

template<typename T, std::enable_if_t<float16_detail::kNativeArithmetic<T>, int> = 0>
constexpr inline bool operator !=(Float16 left, T right) noexcept {
    return !(left == right);
}

template<typename T, std::enable_if_t<float16_detail::kNativeArithmetic<T>, int> = 0>
constexpr inline bool operator !=(T left, Float16 right) noexcept {
    return !(left == right);
}

template<typename T, std::enable_if_t<float16_detail::kNativeFloating<T>, int> = 0>
constexpr inline bool operator <(Float16 left, T right) noexcept {
    using Result = float16_detail::NativeArithmeticType<T>;
    return static_cast<Result>(left) < static_cast<Result>(right);
}

template<typename T, std::enable_if_t<float16_detail::kNativeFloating<T>, int> = 0>
constexpr inline bool operator <(T left, Float16 right) noexcept {
    using Result = float16_detail::NativeArithmeticType<T>;
    return static_cast<Result>(left) < static_cast<Result>(right);
}

template<typename T, std::enable_if_t<float16_detail::kNativeIntegral<T>, int> = 0>
constexpr inline bool operator <(Float16 left, T right) noexcept {
    return static_cast<f128>(left) < static_cast<f128>(right);
}

template<typename T, std::enable_if_t<float16_detail::kNativeIntegral<T>, int> = 0>
constexpr inline bool operator <(T left, Float16 right) noexcept {
    return static_cast<f128>(left) < static_cast<f128>(right);
}

template<typename T, std::enable_if_t<float16_detail::kNativeArithmetic<T>, int> = 0>
constexpr inline bool operator <=(Float16 left, T right) noexcept {
    return left < right || left == right;
}

template<typename T, std::enable_if_t<float16_detail::kNativeArithmetic<T>, int> = 0>
constexpr inline bool operator <=(T left, Float16 right) noexcept {
    return left < right || left == right;
}

template<typename T, std::enable_if_t<float16_detail::kNativeArithmetic<T>, int> = 0>
constexpr inline bool operator >(Float16 left, T right) noexcept {
    return right < left;
}

template<typename T, std::enable_if_t<float16_detail::kNativeArithmetic<T>, int> = 0>
constexpr inline bool operator >(T left, Float16 right) noexcept {
    return right < left;
}

template<typename T, std::enable_if_t<float16_detail::kNativeArithmetic<T>, int> = 0>
constexpr inline bool operator >=(Float16 left, T right) noexcept {
    return right < left || left == right;
}

template<typename T, std::enable_if_t<float16_detail::kNativeArithmetic<T>, int> = 0>
constexpr inline bool operator >=(T left, Float16 right) noexcept {
    return right < left || left == right;
}

namespace std {

constexpr inline Float16 abs (Float16 value) noexcept {
    return float16_detail::from_bits(static_cast<u16>(value.bits() & static_cast<u16>(~float16_detail::kSignMask)));
}

constexpr inline bool isfinite (Float16 value) noexcept {
    return float16_detail::exponent_bits(value) != float16_detail::kExponentMask;
}

constexpr inline bool isnan (Float16 value) noexcept { return float16_detail::is_nan_bits(value); }

constexpr inline bool isinf (Float16 value) noexcept { return float16_detail::is_inf_bits(value); }

constexpr inline bool signbit (Float16 value) noexcept { return float16_detail::sign_bit(value); }

constexpr inline int fpclassify (Float16 value) noexcept {
    if (isnan(value)) {
        return FP_NAN;
    }
    if (isinf(value)) {
        return FP_INFINITE;
    }
    if (float16_detail::exponent_bits(value) == 0U) {
        return float16_detail::fraction_bits(value) == 0U ? FP_ZERO : FP_SUBNORMAL;
    }
    return FP_NORMAL;
}

} // namespace std

static_assert(sizeof(Float16) == sizeof(u16));
static_assert(alignof(Float16) == alignof(u16));
static_assert(std::is_trivially_copyable_v<Float16>);
static_assert(std::is_standard_layout_v<Float16>);
static_assert(std::has_unique_object_representations_v<Float16>);
static_assert(Float16Bits {0x3C00U}.value == 0x3C00U);
static_assert(std::is_floating_point_v<Float16>);
static_assert(std::is_floating_point_v<f16>);
static_assert(std::is_floating_point_v<f32>);
static_assert(std::is_floating_point_v<f64>);
static_assert(std::is_arithmetic_v<Float16>);
static_assert(std::is_arithmetic_v<f16>);
static_assert(std::is_arithmetic_v<f32>);
static_assert(std::is_arithmetic_v<f64>);

static_assert(std::numeric_limits<Float16>::is_specialized);
static_assert(std::numeric_limits<Float16>::min().bits() == 0x0400U);
static_assert(std::numeric_limits<Float16>::denorm_min().bits() == 0x0001U);
static_assert(std::numeric_limits<Float16>::max().bits() == 0x7BFFU);
static_assert(std::numeric_limits<Float16>::lowest().bits() == 0xFBFFU);
static_assert(std::numeric_limits<Float16>::epsilon().bits() == 0x1400U);
static_assert(std::numeric_limits<Float16>::infinity().bits() == 0x7C00U);
static_assert(std::numeric_limits<Float16>::quiet_NaN().bits() == 0x7E00U);
static_assert(std::numeric_limits<Float16>::digits == 11);
static_assert(std::numeric_limits<Float16>::radix == 2);
static_assert(std::numeric_limits<Float16>::round_style == std::round_to_nearest);

static_assert(!std::is_convertible_v<Float16, bool>);
static_assert(!std::is_convertible_v<Float16, char>);
static_assert(!std::is_convertible_v<Float16, signed char>);
static_assert(!std::is_convertible_v<Float16, unsigned char>);
static_assert(!std::is_convertible_v<Float16, wchar_t>);
static_assert(!std::is_convertible_v<Float16, char8_t>);
static_assert(!std::is_convertible_v<Float16, char16_t>);
static_assert(!std::is_convertible_v<Float16, char32_t>);
static_assert(!std::is_convertible_v<Float16, short>);
static_assert(!std::is_convertible_v<Float16, unsigned short>);
static_assert(!std::is_convertible_v<Float16, int>);
static_assert(!std::is_convertible_v<Float16, unsigned int>);
static_assert(!std::is_convertible_v<Float16, long>);
static_assert(!std::is_convertible_v<Float16, unsigned long>);
static_assert(!std::is_convertible_v<Float16, long long>);
static_assert(!std::is_convertible_v<Float16, unsigned long long>);
static_assert(!std::is_convertible_v<Float16, f32>);
static_assert(!std::is_convertible_v<Float16, f64>);
static_assert(!std::is_convertible_v<Float16, f128>);

static_assert(std::is_same_v<decltype(static_cast<Float16>(std::declval<bool>())), Float16>);
static_assert(std::is_same_v<decltype(static_cast<Float16>(std::declval<char>())), Float16>);
static_assert(std::is_same_v<decltype(static_cast<Float16>(std::declval<signed char>())), Float16>);
static_assert(std::is_same_v<decltype(static_cast<Float16>(std::declval<unsigned char>())), Float16>);
static_assert(std::is_same_v<decltype(static_cast<Float16>(std::declval<wchar_t>())), Float16>);
static_assert(std::is_same_v<decltype(static_cast<Float16>(std::declval<char8_t>())), Float16>);
static_assert(std::is_same_v<decltype(static_cast<Float16>(std::declval<char16_t>())), Float16>);
static_assert(std::is_same_v<decltype(static_cast<Float16>(std::declval<char32_t>())), Float16>);
static_assert(std::is_same_v<decltype(static_cast<Float16>(std::declval<short>())), Float16>);
static_assert(std::is_same_v<decltype(static_cast<Float16>(std::declval<unsigned short>())), Float16>);
static_assert(std::is_same_v<decltype(static_cast<Float16>(std::declval<int>())), Float16>);
static_assert(std::is_same_v<decltype(static_cast<Float16>(std::declval<unsigned int>())), Float16>);
static_assert(std::is_same_v<decltype(static_cast<Float16>(std::declval<long>())), Float16>);
static_assert(std::is_same_v<decltype(static_cast<Float16>(std::declval<unsigned long>())), Float16>);
static_assert(std::is_same_v<decltype(static_cast<Float16>(std::declval<long long>())), Float16>);
static_assert(std::is_same_v<decltype(static_cast<Float16>(std::declval<unsigned long long>())), Float16>);
static_assert(std::is_same_v<decltype(static_cast<Float16>(std::declval<f32>())), Float16>);
static_assert(std::is_same_v<decltype(static_cast<Float16>(std::declval<f64>())), Float16>);
static_assert(std::is_same_v<decltype(static_cast<Float16>(std::declval<f128>())), Float16>);

static_assert(std::is_same_v<decltype(static_cast<bool>(std::declval<Float16>())), bool>);
static_assert(std::is_same_v<decltype(static_cast<char>(std::declval<Float16>())), char>);
static_assert(std::is_same_v<decltype(static_cast<signed char>(std::declval<Float16>())), signed char>);
static_assert(std::is_same_v<decltype(static_cast<unsigned char>(std::declval<Float16>())), unsigned char>);
static_assert(std::is_same_v<decltype(static_cast<wchar_t>(std::declval<Float16>())), wchar_t>);
static_assert(std::is_same_v<decltype(static_cast<char8_t>(std::declval<Float16>())), char8_t>);
static_assert(std::is_same_v<decltype(static_cast<char16_t>(std::declval<Float16>())), char16_t>);
static_assert(std::is_same_v<decltype(static_cast<char32_t>(std::declval<Float16>())), char32_t>);
static_assert(std::is_same_v<decltype(static_cast<short>(std::declval<Float16>())), short>);
static_assert(std::is_same_v<decltype(static_cast<unsigned short>(std::declval<Float16>())), unsigned short>);
static_assert(std::is_same_v<decltype(static_cast<int>(std::declval<Float16>())), int>);
static_assert(std::is_same_v<decltype(static_cast<unsigned int>(std::declval<Float16>())), unsigned int>);
static_assert(std::is_same_v<decltype(static_cast<long>(std::declval<Float16>())), long>);
static_assert(std::is_same_v<decltype(static_cast<unsigned long>(std::declval<Float16>())), unsigned long>);
static_assert(std::is_same_v<decltype(static_cast<long long>(std::declval<Float16>())), long long>);
static_assert(std::is_same_v<decltype(static_cast<unsigned long long>(std::declval<Float16>())), unsigned long long>);
static_assert(std::is_same_v<decltype(static_cast<f32>(std::declval<Float16>())), f32>);
static_assert(std::is_same_v<decltype(static_cast<f64>(std::declval<Float16>())), f64>);
static_assert(std::is_same_v<decltype(static_cast<f128>(std::declval<Float16>())), f128>);

static_assert(std::is_same_v<decltype(+std::declval<Float16>()), Float16>);
static_assert(std::is_same_v<decltype(-std::declval<Float16>()), Float16>);

static_assert(std::is_same_v<decltype(std::declval<Float16>() + std::declval<Float16>()), Float16>);
static_assert(std::is_same_v<decltype(std::declval<Float16>() - std::declval<Float16>()), Float16>);
static_assert(std::is_same_v<decltype(std::declval<Float16>() * std::declval<Float16>()), Float16>);
static_assert(std::is_same_v<decltype(std::declval<Float16>() / std::declval<Float16>()), Float16>);
static_assert(std::is_same_v<decltype(std::declval<Float16>() + 1), Float16>);
static_assert(std::is_same_v<decltype(1 + std::declval<Float16>()), Float16>);
static_assert(std::is_same_v<decltype(std::declval<Float16>() - 1), Float16>);
static_assert(std::is_same_v<decltype(1 - std::declval<Float16>()), Float16>);
static_assert(std::is_same_v<decltype(std::declval<Float16>() * 2U), Float16>);
static_assert(std::is_same_v<decltype(2U * std::declval<Float16>()), Float16>);
static_assert(std::is_same_v<decltype(std::declval<Float16>() / 2LL), Float16>);
static_assert(std::is_same_v<decltype(2LL / std::declval<Float16>()), Float16>);
static_assert(std::is_same_v<decltype(std::declval<Float16>() + 1.0F), f32>);
static_assert(std::is_same_v<decltype(1.0F + std::declval<Float16>()), f32>);
static_assert(std::is_same_v<decltype(std::declval<Float16>() - 1.0), f64>);
static_assert(std::is_same_v<decltype(1.0 - std::declval<Float16>()), f64>);
static_assert(std::is_same_v<decltype(std::declval<Float16>() * 1.0L), f128>);
static_assert(std::is_same_v<decltype(1.0L * std::declval<Float16>()), f128>);
static_assert(std::is_same_v<decltype(std::declval<Float16>() / 1.0), f64>);
static_assert(std::is_same_v<decltype(1.0 / std::declval<Float16>()), f64>);

static_assert(std::is_same_v<decltype(std::declval<Float16&>() += std::declval<Float16>()), Float16&>);
static_assert(std::is_same_v<decltype(std::declval<Float16&>() -= std::declval<Float16>()), Float16&>);
static_assert(std::is_same_v<decltype(std::declval<Float16&>() *= std::declval<Float16>()), Float16&>);
static_assert(std::is_same_v<decltype(std::declval<Float16&>() /= std::declval<Float16>()), Float16&>);
static_assert(std::is_same_v<decltype(std::declval<Float16&>() += 1), Float16&>);
static_assert(std::is_same_v<decltype(std::declval<Float16&>() -= 1.0F), Float16&>);
static_assert(std::is_same_v<decltype(std::declval<Float16&>() *= 1.0), Float16&>);
static_assert(std::is_same_v<decltype(std::declval<Float16&>() /= 1.0L), Float16&>);

static_assert(std::is_same_v<decltype(++std::declval<Float16&>()), Float16&>);
static_assert(std::is_same_v<decltype(std::declval<Float16&>()++), Float16>);
static_assert(std::is_same_v<decltype(--std::declval<Float16&>()), Float16&>);
static_assert(std::is_same_v<decltype(std::declval<Float16&>()--), Float16>);

static_assert(std::is_same_v<decltype(std::declval<Float16>() == std::declval<Float16>()), bool>);
static_assert(std::is_same_v<decltype(std::declval<Float16>() != std::declval<Float16>()), bool>);
static_assert(std::is_same_v<decltype(std::declval<Float16>() < std::declval<Float16>()), bool>);
static_assert(std::is_same_v<decltype(std::declval<Float16>() <= std::declval<Float16>()), bool>);
static_assert(std::is_same_v<decltype(std::declval<Float16>() > std::declval<Float16>()), bool>);
static_assert(std::is_same_v<decltype(std::declval<Float16>() >= std::declval<Float16>()), bool>);
static_assert(std::is_same_v<decltype(std::declval<Float16>() == 1), bool>);
static_assert(std::is_same_v<decltype(1 == std::declval<Float16>()), bool>);
static_assert(std::is_same_v<decltype(std::declval<Float16>() < 1.0), bool>);
static_assert(std::is_same_v<decltype(1.0 < std::declval<Float16>()), bool>);

static_assert(std::is_same_v<decltype(std::abs(std::declval<Float16>())), Float16>);
static_assert(std::is_same_v<decltype(std::isfinite(std::declval<Float16>())), bool>);
static_assert(std::is_same_v<decltype(std::isnan(std::declval<Float16>())), bool>);
static_assert(std::is_same_v<decltype(std::isinf(std::declval<Float16>())), bool>);
static_assert(std::is_same_v<decltype(std::signbit(std::declval<Float16>())), bool>);
static_assert(std::is_same_v<decltype(std::fpclassify(std::declval<Float16>())), int>);

static_assert(Float16().bits() == 0x0000U);
static_assert(static_cast<Float16>(1).bits() == 0x3C00U);
static_assert(static_cast<Float16>(-1).bits() == 0xBC00U);
static_assert(static_cast<Float16>(1.0F).bits() == 0x3C00U);
static_assert(static_cast<Float16>(1.0).bits() == 0x3C00U);
static_assert(static_cast<Float16>(1.0L).bits() == 0x3C00U);
static_assert(static_cast<Float16>(0x1.0p-25).bits() == 0x0000U);
static_assert(static_cast<Float16>(0x1.0p-24).bits() == 0x0001U);
static_assert(static_cast<Float16>(0x1.8p-24).bits() == 0x0002U);
static_assert(static_cast<Float16>(0x1.0p-14).bits() == 0x0400U);
static_assert(static_cast<Float16>(0x1.0p-10).bits() == 0x1400U);
static_assert(static_cast<Float16>(1.0e-3).bits() == 0x1419U);
static_assert(static_cast<Float16>(1.0e-3L).bits() == 0x1419U);
static_assert(static_cast<Float16>(0x1.ffcp15).bits() == 0x7BFFU);
static_assert(static_cast<Float16>(0x1.ffep15).bits() == 0x7C00U);
static_assert(static_cast<Float16>(-0.0L).bits() == 0x8000U);
static_assert(static_cast<Float16>(std::numeric_limits<f128>::infinity()).bits() == 0x7C00U);
static_assert(static_cast<Float16>(-std::numeric_limits<f128>::infinity()).bits() == 0xFC00U);
static_assert(std::isnan(static_cast<Float16>(std::numeric_limits<f128>::quiet_NaN())));
static_assert(static_cast<f64>(Float16(Float16Bits {0x3C00U})) == 1.0);
static_assert(static_cast<f32>(Float16(Float16Bits {0x4000U})) == 2.0F);
static_assert(static_cast<f128>(Float16(Float16Bits {0x4200U})) == 3.0L);
static_assert(std::abs(Float16(Float16Bits {0xBC00U})).bits() == 0x3C00U);
static_assert(std::isfinite(Float16(Float16Bits {0x3C00U})));
static_assert(std::isinf(Float16(Float16Bits {0x7C00U})));
static_assert(std::isnan(Float16(Float16Bits {0x7E00U})));
static_assert(std::signbit(Float16(Float16Bits {0x8000U})));
static_assert(std::fpclassify(Float16(Float16Bits {0x0000U})) == FP_ZERO);
static_assert(std::fpclassify(Float16(Float16Bits {0x0001U})) == FP_SUBNORMAL);
static_assert(std::fpclassify(Float16(Float16Bits {0x3C00U})) == FP_NORMAL);
static_assert(std::fpclassify(Float16(Float16Bits {0x7C00U})) == FP_INFINITE);
static_assert(std::fpclassify(Float16(Float16Bits {0x7E00U})) == FP_NAN);
static_assert((Float16(Float16Bits {0x3C00U}) + Float16(Float16Bits {0x3C00U})).bits() == 0x4000U);
static_assert((Float16(Float16Bits {0x4000U}) - Float16(Float16Bits {0x3C00U})).bits() == 0x3C00U);
static_assert((Float16(Float16Bits {0x4000U}) * Float16(Float16Bits {0x4000U})).bits() == 0x4400U);
static_assert((Float16(Float16Bits {0x4400U}) / Float16(Float16Bits {0x4000U})).bits() == 0x4000U);
static_assert((Float16(Float16Bits {0x3C00U}) + 1).bits() == 0x4000U);
static_assert((1 + Float16(Float16Bits {0x3C00U})).bits() == 0x4000U);
static_assert((Float16(Float16Bits {0x4000U}) - 1).bits() == 0x3C00U);
static_assert((3 - Float16(Float16Bits {0x4000U})).bits() == 0x3C00U);
static_assert((Float16(Float16Bits {0x4000U}) * 2).bits() == 0x4400U);
static_assert((4 / Float16(Float16Bits {0x4000U})).bits() == 0x4000U);
static_assert(Float16(Float16Bits {0x3C00U}) + 0.5 == 1.5);
static_assert(0.5 + Float16(Float16Bits {0x3C00U}) == 1.5);
static_assert(Float16(Float16Bits {0x4000U}) == 2);
static_assert(2 == Float16(Float16Bits {0x4000U}));
static_assert(Float16(Float16Bits {0x4000U}) > 1);
static_assert(1 < Float16(Float16Bits {0x4000U}));
static_assert(Float16(Float16Bits {0x7C00U}) != 100000);
