#pragma once

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
#include <intrin.h>
#endif

namespace secret::platform {

struct CpuFeatures final {
    bool avx = false;
    bool avx2 = false;
};

[[nodiscard]] inline CpuFeatures detect_cpu_features() noexcept {
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
    int registers[4]{};
    __cpuid(registers, 0);
    const int maximum_leaf = registers[0];
    if (maximum_leaf < 1) {
        return {};
    }

    __cpuid(registers, 1);
    constexpr int osxsave_bit = 1 << 27;
    constexpr int avx_bit = 1 << 28;
    const bool osxsave = (registers[2] & osxsave_bit) != 0;
    const bool cpu_avx = (registers[2] & avx_bit) != 0;
    if (!osxsave || !cpu_avx) {
        return {};
    }

    // AVX state and YMM state must both be enabled by Windows.
    const unsigned __int64 xcr0 = _xgetbv(0);
    const bool os_supports_avx = (xcr0 & 0x6ULL) == 0x6ULL;
    if (!os_supports_avx) {
        return {};
    }

    if (maximum_leaf < 7) {
        return {true, false};
    }

    __cpuidex(registers, 7, 0);
    constexpr int avx2_bit = 1 << 5;
    const bool cpu_avx2 = (registers[1] & avx2_bit) != 0;
    return {true, cpu_avx2};
#elif (defined(__GNUC__) || defined(__clang__)) && (defined(__x86_64__) || defined(__i386__))
    __builtin_cpu_init();
    const bool cpu_avx2 = __builtin_cpu_supports("avx2");
    const bool cpu_avx = __builtin_cpu_supports("avx");
    return {cpu_avx, cpu_avx2};
#else
    return {};
#endif
}

} // namespace secret::platform
