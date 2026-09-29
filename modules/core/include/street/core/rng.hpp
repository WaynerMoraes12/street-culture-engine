#pragma once
// Deterministic, platform-independent random numbers.
//
// Standard <random> distributions are implementation-defined, so the same seed
// yields different games on MSVC, libstdc++ and libc++. Every stochastic system
// in the Street Culture Engine draws from this PCG32 generator and its hand-written
// distributions instead, so a seed is a complete, shareable description of a
// beat, a game, a season or a market run.

#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <span>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace street {

/// SplitMix64 finalizer: scrambles any 64-bit value into a well-distributed one.
constexpr std::uint64_t mix64(std::uint64_t x) noexcept {
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}

/// Stable 64-bit hash of a string (FNV-1a + mix64). Handy for deriving seeds from names.
constexpr std::uint64_t hash_seed(std::string_view text) noexcept {
    std::uint64_t h = 0xCBF29CE484222325ull;
    for (char c : text) {
        h ^= static_cast<unsigned char>(c);
        h *= 0x100000001B3ull;
    }
    return mix64(h);
}

/// Non-deterministic seed (random_device mixed with the clock) for "surprise me" runs.
std::uint64_t random_seed();

/// PCG32 (O'Neill, 2014): small state, excellent statistical quality, fast.
/// Satisfies UniformRandomBitGenerator, so it also works with std::shuffle & co.
class Rng {
public:
    using result_type = std::uint32_t;

    explicit Rng(std::uint64_t seed = 0x5EEDull, std::uint64_t stream = 0) noexcept
        : inc_((mix64(stream) << 1u) | 1u) {
        next_u32();
        state_ += mix64(seed);
        next_u32();
    }

    static constexpr result_type min() noexcept { return 0; }
    static constexpr result_type max() noexcept { return 0xFFFFFFFFu; }
    result_type operator()() noexcept { return next_u32(); }

    std::uint32_t next_u32() noexcept {
        const std::uint64_t old = state_;
        state_ = old * 6364136223846793005ull + inc_;
        const auto xorshifted = static_cast<std::uint32_t>(((old >> 18u) ^ old) >> 27u);
        const auto rot = static_cast<std::uint32_t>(old >> 59u);
        return (xorshifted >> rot) | (xorshifted << ((32u - rot) & 31u));
    }

    std::uint64_t next_u64() noexcept {
        const std::uint64_t hi = next_u32();
        return (hi << 32) | next_u32();
    }

    /// Uniform double in [0, 1) with 53 bits of precision.
    double uniform() noexcept { return static_cast<double>(next_u64() >> 11) * 0x1.0p-53; }

    /// Uniform double in [lo, hi).
    double uniform(double lo, double hi) noexcept { return lo + (hi - lo) * uniform(); }

    /// Unbiased integer in [0, bound) using Lemire's multiply-shift rejection method.
    std::uint32_t below(std::uint32_t bound) noexcept {
        if (bound == 0) return 0;
        std::uint64_t m = std::uint64_t{next_u32()} * bound;
        auto low = static_cast<std::uint32_t>(m);
        if (low < bound) {
            const std::uint32_t threshold = (0u - bound) % bound;
            while (low < threshold) {
                m = std::uint64_t{next_u32()} * bound;
                low = static_cast<std::uint32_t>(m);
            }
        }
        return static_cast<std::uint32_t>(m >> 32);
    }

    /// Uniform integer in the closed range [lo, hi]. Arguments may be given in any order.
    int range(int lo, int hi) noexcept {
        if (hi < lo) std::swap(lo, hi);
        const auto span = static_cast<std::uint32_t>(static_cast<std::int64_t>(hi) - lo + 1);
        return static_cast<int>(static_cast<std::int64_t>(lo) + below(span));
    }

    /// True with probability p (p <= 0 never, p >= 1 always).
    bool chance(double p) noexcept { return uniform() < p; }

    /// Gaussian sample via Box-Muller (the second value of each pair is cached).
    double normal(double mean = 0.0, double stddev = 1.0) noexcept {
        if (has_spare_) {
            has_spare_ = false;
            return mean + stddev * spare_;
        }
        const double u1 = 1.0 - uniform(); // (0, 1]: log() stays finite
        const double u2 = uniform();
        const double radius = std::sqrt(-2.0 * std::log(u1));
        const double theta = 6.283185307179586476925 * u2;
        spare_ = radius * std::sin(theta);
        has_spare_ = true;
        return mean + stddev * radius * std::cos(theta);
    }

    /// Index drawn proportionally to non-negative weights. All-zero weights pick uniformly.
    std::size_t weighted(std::span<const double> weights) {
        if (weights.empty()) throw std::invalid_argument("Rng::weighted: no weights");
        double total = 0.0;
        for (double w : weights) total += w > 0.0 ? w : 0.0;
        if (total <= 0.0) return below(static_cast<std::uint32_t>(weights.size()));
        double target = uniform() * total;
        for (std::size_t i = 0; i < weights.size(); ++i) {
            const double w = weights[i] > 0.0 ? weights[i] : 0.0;
            if (target < w) return i;
            target -= w;
        }
        for (std::size_t i = weights.size(); i-- > 0;)
            if (weights[i] > 0.0) return i; // floating-point slack lands on the last live bucket
        return weights.size() - 1;
    }

    std::size_t weighted(std::initializer_list<double> weights) {
        return weighted(std::span<const double>(weights.begin(), weights.size()));
    }

    template <class T>
    const T& pick(const std::vector<T>& items) {
        if (items.empty()) throw std::out_of_range("Rng::pick: empty container");
        return items[below(static_cast<std::uint32_t>(items.size()))];
    }

    template <class T>
    const T& pick(std::span<const T> items) {
        if (items.empty()) throw std::out_of_range("Rng::pick: empty container");
        return items[below(static_cast<std::uint32_t>(items.size()))];
    }

    template <class T, std::size_t N>
    const T& pick(const T (&items)[N]) {
        return items[below(static_cast<std::uint32_t>(N))];
    }

    /// Fisher-Yates shuffle driven by this generator (portable, unlike std::shuffle's algorithm).
    template <class RandomIt>
    void shuffle(RandomIt first, RandomIt last) {
        const auto n = static_cast<std::uint32_t>(last - first);
        for (std::uint32_t i = n; i > 1; --i) {
            const std::uint32_t j = below(i);
            using std::swap;
            swap(first[i - 1], first[j]);
        }
    }

    /// Derives an independent generator. Deterministic: the n-th fork of a given seed is fixed.
    Rng fork() noexcept {
        const std::uint64_t seed = next_u64();
        const std::uint64_t stream = next_u64();
        return Rng(seed, stream);
    }

private:
    std::uint64_t state_ = 0;
    std::uint64_t inc_ = 1;
    double spare_ = 0.0;
    bool has_spare_ = false;
};

} // namespace street
