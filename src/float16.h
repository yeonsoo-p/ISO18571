#pragma once

#include <cmath>
#include <cstdint>
#include <type_traits>
#include <utility>

enum class Float16Bits : std::uint16_t {};

class Float16 final {
  public:
    constexpr Float16 () noexcept = default;
    explicit constexpr Float16 (Float16Bits bits) noexcept: bits_(static_cast<std::uint16_t>(bits)) {}

    explicit Float16 (bool value) noexcept;
    explicit Float16 (char value) noexcept;
    explicit Float16 (signed char value) noexcept;
    explicit Float16 (unsigned char value) noexcept;
    explicit Float16 (wchar_t value) noexcept;
    explicit Float16 (char8_t value) noexcept;
    explicit Float16 (char16_t value) noexcept;
    explicit Float16 (char32_t value) noexcept;
    explicit Float16 (short value) noexcept;
    explicit Float16 (unsigned short value) noexcept;
    explicit Float16 (int value) noexcept;
    explicit Float16 (unsigned int value) noexcept;
    explicit Float16 (long value) noexcept;
    explicit Float16 (unsigned long value) noexcept;
    explicit Float16 (long long value) noexcept;
    explicit Float16 (unsigned long long value) noexcept;
    explicit Float16 (float value) noexcept;
    explicit Float16 (double value) noexcept;
    explicit Float16 (long double value) noexcept;

    explicit operator bool () const noexcept;
    explicit operator char () const noexcept;
    explicit operator signed char () const noexcept;
    explicit operator unsigned char () const noexcept;
    explicit operator wchar_t () const noexcept;
    explicit operator char8_t () const noexcept;
    explicit operator char16_t () const noexcept;
    explicit operator char32_t () const noexcept;
    explicit operator short () const noexcept;
    explicit operator unsigned short () const noexcept;
    explicit operator int () const noexcept;
    explicit operator unsigned int () const noexcept;
    explicit operator long () const noexcept;
    explicit operator unsigned long () const noexcept;
    explicit operator long long () const noexcept;
    explicit operator unsigned long long () const noexcept;
    explicit operator float () const noexcept;
    explicit operator double () const noexcept;
    explicit operator long double () const noexcept;

    std::uint16_t bits () const noexcept { return bits_; }

  private:
    std::uint16_t bits_ = 0;
};

Float16 operator +(Float16 value) noexcept;
Float16 operator -(Float16 value) noexcept;

Float16 operator +(Float16 left, Float16 right) noexcept;
Float16 operator -(Float16 left, Float16 right) noexcept;
Float16 operator *(Float16 left, Float16 right) noexcept;
Float16 operator /(Float16 left, Float16 right) noexcept;

Float16& operator +=(Float16& left, Float16 right) noexcept;
Float16& operator -=(Float16& left, Float16 right) noexcept;
Float16& operator *=(Float16& left, Float16 right) noexcept;
Float16& operator /=(Float16& left, Float16 right) noexcept;

Float16& operator ++(Float16& value) noexcept;
Float16  operator ++(Float16& value, int) noexcept;
Float16& operator --(Float16& value) noexcept;
Float16  operator --(Float16& value, int) noexcept;

bool operator ==(Float16 left, Float16 right) noexcept;
bool operator !=(Float16 left, Float16 right) noexcept;
bool operator <(Float16 left, Float16 right) noexcept;
bool operator <=(Float16 left, Float16 right) noexcept;
bool operator >(Float16 left, Float16 right) noexcept;
bool operator >=(Float16 left, Float16 right) noexcept;

namespace std {

Float16 abs (Float16 value) noexcept;
bool    isfinite (Float16 value) noexcept;
bool    isnan (Float16 value) noexcept;
bool    isinf (Float16 value) noexcept;
bool    signbit (Float16 value) noexcept;
int     fpclassify (Float16 value) noexcept;

} // namespace std

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
static_assert(!std::is_convertible_v<Float16, float>);
static_assert(!std::is_convertible_v<Float16, double>);
static_assert(!std::is_convertible_v<Float16, long double>);

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
static_assert(std::is_same_v<decltype(static_cast<Float16>(std::declval<float>())), Float16>);
static_assert(std::is_same_v<decltype(static_cast<Float16>(std::declval<double>())), Float16>);
static_assert(std::is_same_v<decltype(static_cast<Float16>(std::declval<long double>())), Float16>);

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
static_assert(std::is_same_v<decltype(static_cast<float>(std::declval<Float16>())), float>);
static_assert(std::is_same_v<decltype(static_cast<double>(std::declval<Float16>())), double>);
static_assert(std::is_same_v<decltype(static_cast<long double>(std::declval<Float16>())), long double>);

static_assert(std::is_same_v<decltype(+std::declval<Float16>()), Float16>);
static_assert(std::is_same_v<decltype(-std::declval<Float16>()), Float16>);

static_assert(std::is_same_v<decltype(std::declval<Float16>() + std::declval<Float16>()), Float16>);
static_assert(std::is_same_v<decltype(std::declval<Float16>() - std::declval<Float16>()), Float16>);
static_assert(std::is_same_v<decltype(std::declval<Float16>() * std::declval<Float16>()), Float16>);
static_assert(std::is_same_v<decltype(std::declval<Float16>() / std::declval<Float16>()), Float16>);

static_assert(std::is_same_v<decltype(std::declval<Float16&>() += std::declval<Float16>()), Float16&>);
static_assert(std::is_same_v<decltype(std::declval<Float16&>() -= std::declval<Float16>()), Float16&>);
static_assert(std::is_same_v<decltype(std::declval<Float16&>() *= std::declval<Float16>()), Float16&>);
static_assert(std::is_same_v<decltype(std::declval<Float16&>() /= std::declval<Float16>()), Float16&>);

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

static_assert(std::is_same_v<decltype(std::abs(std::declval<Float16>())), Float16>);
static_assert(std::is_same_v<decltype(std::isfinite(std::declval<Float16>())), bool>);
static_assert(std::is_same_v<decltype(std::isnan(std::declval<Float16>())), bool>);
static_assert(std::is_same_v<decltype(std::isinf(std::declval<Float16>())), bool>);
static_assert(std::is_same_v<decltype(std::signbit(std::declval<Float16>())), bool>);
static_assert(std::is_same_v<decltype(std::fpclassify(std::declval<Float16>())), int>);
