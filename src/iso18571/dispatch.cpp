#include "engine.hpp"

#include <cstdint>
#include <string>

#if defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
#include <cpuid.h>
#endif

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_AMD64))
#include <intrin.h>
#endif

namespace iso18571 {

namespace {

#if defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
void cpuid (unsigned int leaf, unsigned int subleaf, unsigned int (&registers)[4]) {
    __cpuid_count(leaf, subleaf, registers[0], registers[1], registers[2], registers[3]);
}

unsigned int max_basic_leaf () { return __get_cpuid_max(0U, nullptr); }

unsigned int max_extended_leaf () { return __get_cpuid_max(0x80000000U, nullptr); }

std::uint64_t xgetbv (unsigned int index) {
    unsigned int eax = 0;
    unsigned int edx = 0;
    __asm__ volatile("xgetbv" : "=a"(eax), "=d"(edx) : "c"(index));
    return (static_cast<std::uint64_t>(edx) << 32U) | eax;
}

bool os_supports_avx () {
    const std::uint64_t mask = xgetbv(0);
    return (mask & 0x6U) == 0x6U;
}

bool os_supports_avx512 () {
    const std::uint64_t mask = xgetbv(0);
    return (mask & 0xE6U) == 0xE6U;
}
#endif

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_AMD64))
void cpuid (int leaf, int subleaf, int (&registers)[4]) { __cpuidex(registers, leaf, subleaf); }

unsigned int max_basic_leaf () {
    int registers[4] = {};
    cpuid(0, 0, registers);
    return static_cast<unsigned int>(registers[0]);
}

unsigned int max_extended_leaf () {
    int registers[4] = {};
    cpuid(static_cast<int>(0x80000000U), 0, registers);
    return static_cast<unsigned int>(registers[0]);
}

bool os_supports_avx () {
    const unsigned long long mask = _xgetbv(0);
    return (mask & 0x6) == 0x6;
}

bool os_supports_avx512 () {
    const unsigned long long mask = _xgetbv(0);
    return (mask & 0xE6) == 0xE6;
}
#endif

bool supports_v2 () {
#if defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
    if (max_basic_leaf() < 1U) {
        return false;
    }
    unsigned int registers[4] = {};
    cpuid(1, 0, registers);
    const bool sse3      = (registers[2] & (1U << 0U)) != 0;
    const bool ssse3     = (registers[2] & (1U << 9U)) != 0;
    const bool sse41     = (registers[2] & (1U << 19U)) != 0;
    const bool sse42     = (registers[2] & (1U << 20U)) != 0;
    const bool popcnt    = (registers[2] & (1U << 23U)) != 0;
    const bool cx16      = (registers[2] & (1U << 13U)) != 0;
    bool       lahf_sahf = false;
    if (max_extended_leaf() >= 0x80000001U) {
        cpuid(0x80000001U, 0, registers);
        lahf_sahf = (registers[2] & (1U << 0U)) != 0;
    }
    return sse3 && ssse3 && sse41 && sse42 && popcnt && cx16 && lahf_sahf;
#elif defined(_MSC_VER) && (defined(_M_X64) || defined(_M_AMD64))
    if (max_basic_leaf() < 1U) {
        return false;
    }
    int registers[4] = {};
    cpuid(1, 0, registers);
    const bool sse3      = (registers[2] & (1 << 0)) != 0;
    const bool ssse3     = (registers[2] & (1 << 9)) != 0;
    const bool sse41     = (registers[2] & (1 << 19)) != 0;
    const bool sse42     = (registers[2] & (1 << 20)) != 0;
    const bool popcnt    = (registers[2] & (1 << 23)) != 0;
    const bool cx16      = (registers[2] & (1 << 13)) != 0;
    bool       lahf_sahf = false;
    if (max_extended_leaf() >= 0x80000001U) {
        cpuid(static_cast<int>(0x80000001U), 0, registers);
        lahf_sahf = (registers[2] & (1 << 0)) != 0;
    }
    return sse3 && ssse3 && sse41 && sse42 && popcnt && cx16 && lahf_sahf;
#else
    return false;
#endif
}

bool supports_v3 () {
#if defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
    if (max_basic_leaf() < 7U) {
        return false;
    }
    unsigned int registers[4] = {};
    cpuid(1, 0, registers);
    const bool avx     = (registers[2] & (1U << 28U)) != 0;
    const bool osxsave = (registers[2] & (1U << 27U)) != 0;
    const bool fma     = (registers[2] & (1U << 12U)) != 0;
    const bool f16c    = (registers[2] & (1U << 29U)) != 0;
    const bool movbe   = (registers[2] & (1U << 22U)) != 0;
    cpuid(7, 0, registers);
    const bool avx2  = (registers[1] & (1U << 5U)) != 0;
    const bool bmi   = (registers[1] & (1U << 3U)) != 0;
    const bool bmi2  = (registers[1] & (1U << 8U)) != 0;
    bool       lzcnt = false;
    if (max_extended_leaf() >= 0x80000001U) {
        cpuid(0x80000001U, 0, registers);
        lzcnt = (registers[2] & (1U << 5U)) != 0;
    }
    return supports_v2() && osxsave && os_supports_avx() && avx && avx2 && bmi && bmi2 && fma && f16c && lzcnt && movbe;
#elif defined(_MSC_VER) && (defined(_M_X64) || defined(_M_AMD64))
    if (max_basic_leaf() < 7U) {
        return false;
    }
    int registers[4] = {};
    cpuid(1, 0, registers);
    const bool avx     = (registers[2] & (1 << 28)) != 0;
    const bool osxsave = (registers[2] & (1 << 27)) != 0;
    const bool fma     = (registers[2] & (1 << 12)) != 0;
    const bool f16c    = (registers[2] & (1 << 29)) != 0;
    const bool movbe   = (registers[2] & (1 << 22)) != 0;
    cpuid(7, 0, registers);
    const bool avx2  = (registers[1] & (1 << 5)) != 0;
    const bool bmi   = (registers[1] & (1 << 3)) != 0;
    const bool bmi2  = (registers[1] & (1 << 8)) != 0;
    bool       lzcnt = false;
    if (max_extended_leaf() >= 0x80000001U) {
        cpuid(static_cast<int>(0x80000001U), 0, registers);
        lzcnt = (registers[2] & (1 << 5)) != 0;
    }
    return supports_v2() && osxsave && os_supports_avx() && avx && avx2 && bmi && bmi2 && fma && f16c && lzcnt && movbe;
#else
    return false;
#endif
}

bool supports_v4 () {
#if defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
    if (max_basic_leaf() < 7U) {
        return false;
    }
    unsigned int registers[4] = {};
    cpuid(7, 0, registers);
    const bool avx512f  = (registers[1] & (1U << 16U)) != 0;
    const bool avx512dq = (registers[1] & (1U << 17U)) != 0;
    const bool avx512cd = (registers[1] & (1U << 28U)) != 0;
    const bool avx512bw = (registers[1] & (1U << 30U)) != 0;
    const bool avx512vl = (registers[1] & (1U << 31U)) != 0;
    return supports_v3() && os_supports_avx512() && avx512f && avx512dq && avx512cd && avx512bw && avx512vl;
#elif defined(_MSC_VER) && (defined(_M_X64) || defined(_M_AMD64))
    if (max_basic_leaf() < 7U) {
        return false;
    }
    int registers[4] = {};
    cpuid(7, 0, registers);
    const bool avx512f  = (registers[1] & (1 << 16)) != 0;
    const bool avx512dq = (registers[1] & (1 << 17)) != 0;
    const bool avx512cd = (registers[1] & (1 << 28)) != 0;
    const bool avx512bw = (registers[1] & (1 << 30)) != 0;
    const bool avx512vl = (registers[1] & (1 << 31)) != 0;
    return supports_v3() && os_supports_avx512() && avx512f && avx512dq && avx512cd && avx512bw && avx512vl;
#else
    return false;
#endif
}

DispatchTable make_dispatch_table () {
#if defined(ISO18571_COMPILED_X86_64_V4)
    if (supports_v4()) {
        return {score_components_v4, "x86-64-v4"};
    }
#endif
#if defined(ISO18571_COMPILED_X86_64_V3)
    if (supports_v3()) {
        return {score_components_v3, "x86-64-v3"};
    }
#endif
#if defined(ISO18571_COMPILED_X86_64_V2)
    if (supports_v2()) {
        return {score_components_v2, "x86-64-v2"};
    }
#endif
    return {score_components_v1, "x86-64-v1"};
}

} // namespace

const DispatchTable& dispatch_table () {
    static const DispatchTable table = make_dispatch_table();
    return table;
}

const char* compiled_x86_64_levels () {
    static const std::string levels = [] {
        std::string out = "x86-64-v1";
#if defined(ISO18571_COMPILED_X86_64_V2)
        out += ",x86-64-v2";
#endif
#if defined(ISO18571_COMPILED_X86_64_V3)
        out += ",x86-64-v3";
#endif
#if defined(ISO18571_COMPILED_X86_64_V4)
        out += ",x86-64-v4";
#endif
        return out;
    }();
    return levels.c_str();
}

} // namespace iso18571
