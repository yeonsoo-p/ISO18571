#pragma once

#include <complex>
#include <cstdint>
#include <type_traits>

class Float16;

using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using i8  = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

using f16  = Float16;
using f32  = float;
using f64  = double;
using f128 = long double;

using c64  = std::complex<f32>;
using c128 = std::complex<f64>;
using c256 = std::complex<f128>;

static_assert(std::is_same_v<u8, std::uint8_t>);
static_assert(std::is_same_v<u16, std::uint16_t>);
static_assert(std::is_same_v<u32, std::uint32_t>);
static_assert(std::is_same_v<u64, std::uint64_t>);

static_assert(std::is_same_v<i8, std::int8_t>);
static_assert(std::is_same_v<i16, std::int16_t>);
static_assert(std::is_same_v<i32, std::int32_t>);
static_assert(std::is_same_v<i64, std::int64_t>);

static_assert(std::is_same_v<f16, Float16>);
static_assert(std::is_same_v<f32, float>);
static_assert(std::is_same_v<f64, double>);
static_assert(std::is_same_v<f128, long double>);

static_assert(std::is_same_v<c64, std::complex<f32>>);
static_assert(std::is_same_v<c128, std::complex<f64>>);
static_assert(std::is_same_v<c256, std::complex<f128>>);
