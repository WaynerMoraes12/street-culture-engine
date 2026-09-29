#pragma once
// Internal: independent random streams derived from one beat seed.
//
// Each concern draws from its own PCG32 stream, so decisions don't shift each
// other: overriding --bpm leaves the drum groove untouched, and adding a track
// to a pattern doesn't change how the other tracks are humanised.

#include <cstdint>

namespace street::beats::detail {

enum class Stream : std::uint64_t {
    Resolve = 1, ///< tempo and key picks
    Harmony,     ///< chord progression
    Groove,      ///< drum grammar and bass line
    Title,       ///< generated title
    Timing,      ///< shared per-step groove drift
    Human,       ///< per-track jitter and velocity (the track index is mixed into the seed)
    Noise,       ///< noise sources inside voices
    Vinyl,       ///< crackle
};

constexpr std::uint64_t stream(Stream s) noexcept {
    return static_cast<std::uint64_t>(s);
}

} // namespace street::beats::detail
