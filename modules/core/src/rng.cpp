#include "street/core/rng.hpp"

#include <chrono>
#include <random>

namespace street {

std::uint64_t random_seed() {
    const auto ticks = static_cast<std::uint64_t>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count());
    std::uint64_t entropy = 0;
    try {
        std::random_device device;
        entropy = (std::uint64_t{device()} << 32) ^ device();
    } catch (...) {
        // Some platforms lack a hardware entropy source; the clock alone still varies per run.
    }
    // Keep seeds short enough to type back in: 0 .. 999'999'999.
    return mix64(entropy ^ ticks) % 1'000'000'000ull;
}

} // namespace street
