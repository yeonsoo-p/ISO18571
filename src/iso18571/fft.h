#pragma once

#include <complex>
#include <cstddef>

namespace fft {

inline constexpr bool FORWARD  = true;
inline constexpr bool BACKWARD = false;

using C2CPowerOfTwoFn = void (*)(std::complex<double>* data, std::size_t length, bool forward, double fct);

struct DispatchTable {
    C2CPowerOfTwoFn c2c_power_of_two = nullptr;
    const char*     level            = "x86-64-v1";
};

void c2c_power_of_two (std::complex<double>* data, std::size_t length, bool forward, double fct);

void c2c_power_of_two_v1 (std::complex<double>* data, std::size_t length, bool forward, double fct);

#if defined(ISO18571_FFT_COMPILED_X86_64_V2)
void c2c_power_of_two_v2 (std::complex<double>* data, std::size_t length, bool forward, double fct);
#endif

#if defined(ISO18571_FFT_COMPILED_X86_64_V3)
void c2c_power_of_two_v3 (std::complex<double>* data, std::size_t length, bool forward, double fct);
#endif

#if defined(ISO18571_FFT_COMPILED_X86_64_V4)
void c2c_power_of_two_v4 (std::complex<double>* data, std::size_t length, bool forward, double fct);
#endif

const DispatchTable& dispatch_table ();

} // namespace fft
