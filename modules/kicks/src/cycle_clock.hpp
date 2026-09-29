#pragma once
// Internal: cheap, high-resolution timestamps for per-operation latency samples.
//
// std::chrono::steady_clock is too coarse for sub-microsecond samples on some platforms: on
// Windows it is backed by QueryPerformanceCounter, which typically ticks every 100 ns. On x86
// we read the time-stamp counter instead (constant-rate on every CPU of the last decade) and
// calibrate it against steady_clock over the whole measurement. Everywhere else we fall back
// to steady_clock, which has nanosecond resolution on Linux and macOS.

#include <chrono>
#include <cstdint>

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
#include <intrin.h>
#define STREET_KICKS_HAVE_TSC 1
#elif (defined(__GNUC__) || defined(__clang__)) && (defined(__x86_64__) || defined(__i386__))
#include <x86intrin.h>
#define STREET_KICKS_HAVE_TSC 1
#endif

namespace street::kicks::detail {

inline std::uint64_t cycle_stamp() noexcept {
#ifdef STREET_KICKS_HAVE_TSC
    return __rdtsc();
#else
    return static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
#endif
}

} // namespace street::kicks::detail
