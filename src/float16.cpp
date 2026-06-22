#include "float16.h"

#include <algorithm>
#include <bit>
#include <climits>
#include <cmath>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace {

constexpr std::uint16_t kSignMask      = 0x8000U;
constexpr std::uint16_t kExponentMask  = 0x7C00U;
constexpr std::uint16_t kFractionMask  = 0x03FFU;
constexpr std::uint16_t kQuietNaN      = 0x7E00U;
constexpr std::uint16_t kPositiveOne   = 0x3C00U;
constexpr int           kExponentBias  = 15;
constexpr int           kFractionBits  = 10;
constexpr int           kMaxExponent   = 15;
constexpr int           kMinNormalExp  = -14;
constexpr int           kDivisionGuard = 42;

static inline Float16 from_bits (std::uint16_t bits) noexcept { return Float16(Float16Bits {bits}); }

static inline std::uint16_t sign_bits (Float16 value) noexcept {
    return static_cast<std::uint16_t>(value.bits() & kSignMask);
}

static inline std::uint16_t exponent_bits (Float16 value) noexcept {
    return static_cast<std::uint16_t>(value.bits() & kExponentMask);
}

static inline std::uint16_t fraction_bits (Float16 value) noexcept {
    return static_cast<std::uint16_t>(value.bits() & kFractionMask);
}

static inline bool sign_bit (Float16 value) noexcept { return sign_bits(value) != 0U; }

static inline bool is_nan_bits (Float16 value) noexcept {
    return exponent_bits(value) == kExponentMask && fraction_bits(value) != 0U;
}

static inline bool is_inf_bits (Float16 value) noexcept {
    return exponent_bits(value) == kExponentMask && fraction_bits(value) == 0U;
}

static inline bool is_zero_bits (Float16 value) noexcept {
    return (value.bits() & static_cast<std::uint16_t>(~kSignMask)) == 0U;
}

static inline std::uint64_t round_shift_right (std::uint64_t value, int shift) noexcept {
    if (shift <= 0) {
        return value << -shift;
    }
    if (shift > 64) {
        return 0;
    }
    if (shift == 64) {
        constexpr std::uint64_t half = 1ULL << 63U;
        return value > half ? 1ULL : 0ULL;
    }

    const std::uint64_t quotient = value >> shift;
    const std::uint64_t mask     = (1ULL << shift) - 1ULL;
    const std::uint64_t rem      = value & mask;
    const std::uint64_t half     = 1ULL << (shift - 1);
    if (rem > half || (rem == half && (quotient & 1ULL) != 0ULL)) {
        return quotient + 1ULL;
    }
    return quotient;
}

static inline Float16 pack_components (bool negative, std::uint64_t significand, int exponent) noexcept {
    const std::uint16_t sign = negative ? kSignMask : 0U;
    if (significand == 0ULL) {
        return from_bits(sign);
    }

    const int highest_bit   = static_cast<int>(std::bit_width(significand)) - 1;
    int       binary_exp    = exponent + highest_bit;
    const int subnorm_shift = -(exponent + 24);

    if (binary_exp < kMinNormalExp) {
        const std::uint64_t rounded = round_shift_right(significand, subnorm_shift);
        if (rounded == 0ULL) {
            return from_bits(sign);
        }
        if (rounded >= 0x0400ULL) {
            return from_bits(static_cast<std::uint16_t>(sign | 0x0400U));
        }
        return from_bits(static_cast<std::uint16_t>(sign | static_cast<std::uint16_t>(rounded)));
    }

    if (binary_exp > kMaxExponent) {
        return from_bits(static_cast<std::uint16_t>(sign | kExponentMask));
    }

    const int     shift   = binary_exp - kFractionBits - exponent;
    std::uint64_t rounded = round_shift_right(significand, shift);
    if (rounded == 0x0800ULL) {
        rounded = 0x0400ULL;
        ++binary_exp;
    }
    if (binary_exp > kMaxExponent) {
        return from_bits(static_cast<std::uint16_t>(sign | kExponentMask));
    }

    const auto encoded_exponent = static_cast<std::uint16_t>(binary_exp + kExponentBias);
    const auto encoded_fraction = static_cast<std::uint16_t>(rounded - 0x0400ULL);
    return from_bits(static_cast<std::uint16_t>(sign | static_cast<std::uint16_t>(encoded_exponent << kFractionBits) |
                                                encoded_fraction));
}

static inline Float16 pack_unsigned_integer (bool negative, unsigned long long magnitude) noexcept {
    return pack_components(negative, static_cast<std::uint64_t>(magnitude), 0);
}

template<typename T>
static inline Float16 pack_signed_integer (T value) noexcept {
    using Unsigned = std::make_unsigned_t<T>;
    if (value < 0) {
        const auto magnitude = static_cast<Unsigned>(-(value + 1));
        return pack_unsigned_integer(true, static_cast<unsigned long long>(magnitude) + 1ULL);
    }
    return pack_unsigned_integer(false, static_cast<unsigned long long>(value));
}

template<typename T>
static inline Float16 pack_integral (T value) noexcept {
    if constexpr (std::is_same_v<T, bool>) {
        return pack_unsigned_integer(false, value ? 1ULL : 0ULL);
    } else if constexpr (std::is_signed_v<T>) {
        return pack_signed_integer(value);
    } else {
        return pack_unsigned_integer(false, static_cast<unsigned long long>(value));
    }
}

static inline Float16 pack_float_bits (std::uint32_t bits) noexcept {
    const bool          negative = (bits & 0x80000000UL) != 0UL;
    const std::uint32_t exponent = (bits >> 23U) & 0xFFU;
    const std::uint32_t fraction = bits & 0x007FFFFFUL;

    if (exponent == 0xFFU) {
        return from_bits(
            static_cast<std::uint16_t>((negative ? kSignMask : 0U) | (fraction == 0U ? kExponentMask : kQuietNaN)));
    }
    if (exponent == 0U) {
        return pack_components(negative, static_cast<std::uint64_t>(fraction), -149);
    }
    return pack_components(negative, static_cast<std::uint64_t>((1UL << 23U) | fraction),
                           static_cast<int>(exponent) - 127 - 23);
}

static inline Float16 pack_double_bits (std::uint64_t bits) noexcept {
    const bool          negative = (bits & 0x8000000000000000ULL) != 0ULL;
    const std::uint64_t exponent = (bits >> 52U) & 0x7FFULL;
    const std::uint64_t fraction = bits & 0x000FFFFFFFFFFFFFULL;

    if (exponent == 0x7FFULL) {
        return from_bits(
            static_cast<std::uint16_t>((negative ? kSignMask : 0U) | (fraction == 0ULL ? kExponentMask : kQuietNaN)));
    }
    if (exponent == 0ULL) {
        return pack_components(negative, fraction, -1074);
    }
    return pack_components(negative, (1ULL << 52U) | fraction, static_cast<int>(exponent) - 1023 - 52);
}

static inline Float16 pack_long_double (long double value) noexcept {
    const bool          negative  = std::signbit(value);
    const std::uint16_t sign      = negative ? kSignMask : 0U;
    const long double   magnitude = std::abs(value);

    if (std::isnan(value)) {
        return from_bits(static_cast<std::uint16_t>(sign | kQuietNaN));
    }
    if (std::isinf(value)) {
        return from_bits(static_cast<std::uint16_t>(sign | kExponentMask));
    }
    if (magnitude == 0.0L) {
        return from_bits(sign);
    }

    int                      exponent = 0;
    const long double        fraction = std::frexp(magnitude, &exponent);
    constexpr int            scale    = 63;
    const unsigned long long scaled   = static_cast<unsigned long long>(std::ldexp(fraction, scale));
    return pack_components(negative, scaled, exponent - scale);
}

struct Components {
    bool          negative    = false;
    std::uint64_t significand = 0;
    int           exponent    = 0;
};

static inline Components decode_finite (Float16 value) noexcept {
    const std::uint16_t exponent = static_cast<std::uint16_t>((value.bits() & kExponentMask) >> kFractionBits);
    const std::uint16_t fraction = fraction_bits(value);
    if (exponent == 0U) {
        return {sign_bit(value), static_cast<std::uint64_t>(fraction), -24};
    }
    return {
        sign_bit(value),
        static_cast<std::uint64_t>(0x0400U | fraction),
        static_cast<int>(exponent) - kExponentBias - kFractionBits,
    };
}

static inline Float16 quiet_nan () noexcept { return from_bits(kQuietNaN); }

static inline Float16 infinity (bool negative) noexcept {
    return from_bits(static_cast<std::uint16_t>((negative ? kSignMask : 0U) | kExponentMask));
}

static inline Float16 zero (bool negative) noexcept { return from_bits(negative ? kSignMask : 0U); }

static inline Float16 add_finite (Float16 left, Float16 right) noexcept {
    Components left_components  = decode_finite(left);
    Components right_components = decode_finite(right);
    if (left_components.significand == 0ULL) {
        return right;
    }
    if (right_components.significand == 0ULL) {
        return left;
    }

    const int common_exponent = std::min(left_components.exponent, right_components.exponent);
    auto      left_magnitude =
        static_cast<long long>(left_components.significand << (left_components.exponent - common_exponent));
    auto right_magnitude =
        static_cast<long long>(right_components.significand << (right_components.exponent - common_exponent));
    if (left_components.negative) {
        left_magnitude = -left_magnitude;
    }
    if (right_components.negative) {
        right_magnitude = -right_magnitude;
    }

    const long long sum = left_magnitude + right_magnitude;
    if (sum < 0) {
        return pack_components(true, static_cast<std::uint64_t>(-sum), common_exponent);
    }
    return pack_components(false, static_cast<std::uint64_t>(sum), common_exponent);
}

static inline Float16 multiply_finite (Float16 left, Float16 right) noexcept {
    const Components left_components  = decode_finite(left);
    const Components right_components = decode_finite(right);
    return pack_components(left_components.negative != right_components.negative,
                           left_components.significand * right_components.significand,
                           left_components.exponent + right_components.exponent);
}

static inline Float16 divide_finite (Float16 left, Float16 right) noexcept {
    const Components left_components  = decode_finite(left);
    const Components right_components = decode_finite(right);
    const auto       numerator        = static_cast<std::uint64_t>(left_components.significand << kDivisionGuard);
    auto             quotient         = static_cast<std::uint64_t>(numerator / right_components.significand);
    const auto       remainder        = static_cast<std::uint64_t>(numerator % right_components.significand);
    if (remainder != 0ULL) {
        quotient |= 1ULL;
    }
    return pack_components(left_components.negative != right_components.negative, quotient,
                           left_components.exponent - right_components.exponent - kDivisionGuard);
}

static inline std::uint64_t integer_magnitude (Float16 value) noexcept {
    if (is_nan_bits(value) || is_inf_bits(value)) {
        return std::numeric_limits<std::uint64_t>::max();
    }

    const Components components = decode_finite(value);
    if (components.exponent >= 0) {
        if (components.exponent >= 64) {
            return std::numeric_limits<std::uint64_t>::max();
        }
        return components.significand << components.exponent;
    }
    if (components.exponent <= -64) {
        return 0ULL;
    }
    return components.significand >> -components.exponent;
}

template<typename T>
static inline T cast_to_unsigned_integer (Float16 value) noexcept {
    if (sign_bit(value)) {
        return 0;
    }
    const std::uint64_t magnitude = integer_magnitude(value);
    const auto          maximum   = static_cast<std::uint64_t>(std::numeric_limits<T>::max());
    return magnitude > maximum ? std::numeric_limits<T>::max() : static_cast<T>(magnitude);
}

template<typename T>
static inline T cast_to_signed_integer (Float16 value) noexcept {
    const std::uint64_t magnitude = integer_magnitude(value);
    if (sign_bit(value)) {
        const auto limit = static_cast<std::uint64_t>(std::numeric_limits<T>::max()) + 1ULL;
        if (magnitude >= limit) {
            return std::numeric_limits<T>::min();
        }
        return static_cast<T>(-static_cast<long long>(magnitude));
    }
    const auto maximum = static_cast<std::uint64_t>(std::numeric_limits<T>::max());
    return magnitude > maximum ? std::numeric_limits<T>::max() : static_cast<T>(magnitude);
}

template<typename T>
static inline T cast_to_integral (Float16 value) noexcept {
    if constexpr (std::is_same_v<T, bool>) {
        return !is_zero_bits(value) && !is_nan_bits(value);
    } else if constexpr (std::is_signed_v<T>) {
        return cast_to_signed_integer<T>(value);
    } else {
        return cast_to_unsigned_integer<T>(value);
    }
}

static inline std::uint32_t float_bits (Float16 value) noexcept {
    const std::uint32_t sign     = static_cast<std::uint32_t>(sign_bits(value)) << 16U;
    const std::uint16_t exponent = static_cast<std::uint16_t>((value.bits() & kExponentMask) >> kFractionBits);
    const std::uint16_t fraction = fraction_bits(value);
    if (exponent == 0x1FU) {
        return sign | 0x7F800000UL | static_cast<std::uint32_t>(fraction == 0U ? 0UL : 0x00400000UL);
    }
    if (exponent == 0U) {
        if (fraction == 0U) {
            return sign;
        }
        const int shift =
            std::countl_zero(static_cast<unsigned int>(fraction)) - (std::numeric_limits<unsigned int>::digits - 10);
        const auto normalized_fraction = static_cast<std::uint32_t>(fraction << (shift + 1));
        const auto encoded_exponent    = static_cast<std::uint32_t>(127 - 14 - shift);
        return sign | (encoded_exponent << 23U) | ((normalized_fraction & kFractionMask) << 13U);
    }
    const auto encoded_exponent = static_cast<std::uint32_t>(static_cast<int>(exponent) - kExponentBias + 127);
    return sign | (encoded_exponent << 23U) | (static_cast<std::uint32_t>(fraction) << 13U);
}

static inline std::uint64_t double_bits (Float16 value) noexcept {
    const std::uint64_t sign     = static_cast<std::uint64_t>(sign_bits(value)) << 48U;
    const std::uint16_t exponent = static_cast<std::uint16_t>((value.bits() & kExponentMask) >> kFractionBits);
    const std::uint16_t fraction = fraction_bits(value);
    if (exponent == 0x1FU) {
        return sign | 0x7FF0000000000000ULL | (fraction == 0U ? 0ULL : 0x0008000000000000ULL);
    }
    if (exponent == 0U) {
        if (fraction == 0U) {
            return sign;
        }
        const int shift =
            std::countl_zero(static_cast<unsigned int>(fraction)) - (std::numeric_limits<unsigned int>::digits - 10);
        const auto normalized_fraction = static_cast<std::uint64_t>(fraction << (shift + 1));
        const auto encoded_exponent    = static_cast<std::uint64_t>(1023 - 14 - shift);
        return sign | (encoded_exponent << 52U) | ((normalized_fraction & kFractionMask) << 42U);
    }
    const auto encoded_exponent = static_cast<std::uint64_t>(static_cast<int>(exponent) - kExponentBias + 1023);
    return sign | (encoded_exponent << 52U) | (static_cast<std::uint64_t>(fraction) << 42U);
}

static inline long double long_double_value (Float16 value) noexcept {
    if (is_nan_bits(value)) {
        return sign_bit(value) ? -std::numeric_limits<long double>::quiet_NaN()
                               : std::numeric_limits<long double>::quiet_NaN();
    }
    if (is_inf_bits(value)) {
        return sign_bit(value) ? -std::numeric_limits<long double>::infinity()
                               : std::numeric_limits<long double>::infinity();
    }
    const Components  components = decode_finite(value);
    const long double magnitude  = std::ldexp(static_cast<long double>(components.significand), components.exponent);
    return components.negative ? -magnitude : magnitude;
}

} // namespace

Float16::Float16 (bool value) noexcept: bits_(pack_integral(value).bits()) {}
Float16::Float16 (char value) noexcept: bits_(pack_integral(value).bits()) {}
Float16::Float16 (signed char value) noexcept: bits_(pack_integral(value).bits()) {}
Float16::Float16 (unsigned char value) noexcept: bits_(pack_integral(value).bits()) {}
Float16::Float16 (wchar_t value) noexcept: bits_(pack_integral(value).bits()) {}
Float16::Float16 (char8_t value) noexcept: bits_(pack_integral(value).bits()) {}
Float16::Float16 (char16_t value) noexcept: bits_(pack_integral(value).bits()) {}
Float16::Float16 (char32_t value) noexcept: bits_(pack_integral(value).bits()) {}
Float16::Float16 (short value) noexcept: bits_(pack_integral(value).bits()) {}
Float16::Float16 (unsigned short value) noexcept: bits_(pack_integral(value).bits()) {}
Float16::Float16 (int value) noexcept: bits_(pack_integral(value).bits()) {}
Float16::Float16 (unsigned int value) noexcept: bits_(pack_integral(value).bits()) {}
Float16::Float16 (long value) noexcept: bits_(pack_integral(value).bits()) {}
Float16::Float16 (unsigned long value) noexcept: bits_(pack_integral(value).bits()) {}
Float16::Float16 (long long value) noexcept: bits_(pack_integral(value).bits()) {}
Float16::Float16 (unsigned long long value) noexcept: bits_(pack_integral(value).bits()) {}
Float16::Float16 (float value) noexcept: bits_(pack_float_bits(std::bit_cast<std::uint32_t>(value)).bits()) {}
Float16::Float16 (double value) noexcept: bits_(pack_double_bits(std::bit_cast<std::uint64_t>(value)).bits()) {}
Float16::Float16 (long double value) noexcept: bits_(pack_long_double(value).bits()) {}

Float16::operator bool () const noexcept { return cast_to_integral<bool>(*this); }
Float16::operator char () const noexcept { return cast_to_integral<char>(*this); }
Float16::operator signed char () const noexcept { return cast_to_integral<signed char>(*this); }
Float16::operator unsigned char () const noexcept { return cast_to_integral<unsigned char>(*this); }
Float16::operator wchar_t () const noexcept { return cast_to_integral<wchar_t>(*this); }
Float16::operator char8_t () const noexcept { return cast_to_integral<char8_t>(*this); }
Float16::operator char16_t () const noexcept { return cast_to_integral<char16_t>(*this); }
Float16::operator char32_t () const noexcept { return cast_to_integral<char32_t>(*this); }
Float16::operator short () const noexcept { return cast_to_integral<short>(*this); }
Float16::operator unsigned short () const noexcept { return cast_to_integral<unsigned short>(*this); }
Float16::operator int () const noexcept { return cast_to_integral<int>(*this); }
Float16::operator unsigned int () const noexcept { return cast_to_integral<unsigned int>(*this); }
Float16::operator long () const noexcept { return cast_to_integral<long>(*this); }
Float16::operator unsigned long () const noexcept { return cast_to_integral<unsigned long>(*this); }
Float16::operator long long () const noexcept { return cast_to_integral<long long>(*this); }
Float16::operator unsigned long long () const noexcept { return cast_to_integral<unsigned long long>(*this); }
Float16::operator float () const noexcept { return std::bit_cast<float>(float_bits(*this)); }
Float16::operator double () const noexcept { return std::bit_cast<double>(double_bits(*this)); }
Float16::operator long double () const noexcept { return long_double_value(*this); }

Float16 operator +(Float16 value) noexcept { return value; }

Float16 operator -(Float16 value) noexcept { return from_bits(static_cast<std::uint16_t>(value.bits() ^ kSignMask)); }

Float16 operator +(Float16 left, Float16 right) noexcept {
    if (is_nan_bits(left) || is_nan_bits(right)) {
        return quiet_nan();
    }
    if (is_inf_bits(left) || is_inf_bits(right)) {
        if (is_inf_bits(left) && is_inf_bits(right) && sign_bit(left) != sign_bit(right)) {
            return quiet_nan();
        }
        return is_inf_bits(left) ? left : right;
    }
    return add_finite(left, right);
}

Float16 operator -(Float16 left, Float16 right) noexcept { return left + -right; }

Float16 operator *(Float16 left, Float16 right) noexcept {
    if (is_nan_bits(left) || is_nan_bits(right)) {
        return quiet_nan();
    }
    const bool negative = sign_bit(left) != sign_bit(right);
    if ((is_inf_bits(left) && is_zero_bits(right)) || (is_zero_bits(left) && is_inf_bits(right))) {
        return quiet_nan();
    }
    if (is_inf_bits(left) || is_inf_bits(right)) {
        return infinity(negative);
    }
    if (is_zero_bits(left) || is_zero_bits(right)) {
        return zero(negative);
    }
    return multiply_finite(left, right);
}

Float16 operator /(Float16 left, Float16 right) noexcept {
    if (is_nan_bits(left) || is_nan_bits(right)) {
        return quiet_nan();
    }
    const bool negative = sign_bit(left) != sign_bit(right);
    if ((is_zero_bits(left) && is_zero_bits(right)) || (is_inf_bits(left) && is_inf_bits(right))) {
        return quiet_nan();
    }
    if (is_inf_bits(left) || is_zero_bits(right)) {
        return infinity(negative);
    }
    if (is_zero_bits(left) || is_inf_bits(right)) {
        return zero(negative);
    }
    return divide_finite(left, right);
}

Float16& operator +=(Float16& left, Float16 right) noexcept {
    left = left + right;
    return left;
}

Float16& operator -=(Float16& left, Float16 right) noexcept {
    left = left - right;
    return left;
}

Float16& operator *=(Float16& left, Float16 right) noexcept {
    left = left * right;
    return left;
}

Float16& operator /=(Float16& left, Float16 right) noexcept {
    left = left / right;
    return left;
}

Float16& operator ++(Float16& value) noexcept {
    value += from_bits(kPositiveOne);
    return value;
}

Float16 operator ++(Float16& value, int) noexcept {
    const Float16 previous = value;
    ++value;
    return previous;
}

Float16& operator --(Float16& value) noexcept {
    value -= from_bits(kPositiveOne);
    return value;
}

Float16 operator --(Float16& value, int) noexcept {
    const Float16 previous = value;
    --value;
    return previous;
}

bool operator ==(Float16 left, Float16 right) noexcept {
    if (is_nan_bits(left) || is_nan_bits(right)) {
        return false;
    }
    if (is_zero_bits(left) && is_zero_bits(right)) {
        return true;
    }
    return left.bits() == right.bits();
}

bool operator !=(Float16 left, Float16 right) noexcept { return !(left == right); }

bool operator <(Float16 left, Float16 right) noexcept {
    if (is_nan_bits(left) || is_nan_bits(right) || left == right) {
        return false;
    }
    if (sign_bit(left) != sign_bit(right)) {
        return sign_bit(left);
    }
    const std::uint16_t left_magnitude  = static_cast<std::uint16_t>(left.bits() & ~kSignMask);
    const std::uint16_t right_magnitude = static_cast<std::uint16_t>(right.bits() & ~kSignMask);
    return sign_bit(left) ? left_magnitude > right_magnitude : left_magnitude < right_magnitude;
}

bool operator <=(Float16 left, Float16 right) noexcept { return left < right || left == right; }

bool operator >(Float16 left, Float16 right) noexcept { return right < left; }

bool operator >=(Float16 left, Float16 right) noexcept { return right < left || left == right; }

namespace std {

Float16 abs (Float16 value) noexcept { return from_bits(static_cast<std::uint16_t>(value.bits() & ~kSignMask)); }

bool isfinite (Float16 value) noexcept { return exponent_bits(value) != kExponentMask; }

bool isnan (Float16 value) noexcept { return is_nan_bits(value); }

bool isinf (Float16 value) noexcept { return is_inf_bits(value); }

bool signbit (Float16 value) noexcept { return ::sign_bit(value); }

int fpclassify (Float16 value) noexcept {
    if (isnan(value)) {
        return FP_NAN;
    }
    if (isinf(value)) {
        return FP_INFINITE;
    }
    if (exponent_bits(value) == 0U) {
        return fraction_bits(value) == 0U ? FP_ZERO : FP_SUBNORMAL;
    }
    return FP_NORMAL;
}

} // namespace std
