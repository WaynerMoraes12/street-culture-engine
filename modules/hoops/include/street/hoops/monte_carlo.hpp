#pragma once
// Monte Carlo title odds: simulate thousands of full seasons (regular season +
// playoffs) across threads and count how often each team wins it all.
//
// Reproducibility contract: work is cut into fixed chunks of seasons. Chunk c
// draws every season seed from its own Rng derived from (seed, c), and chunk
// tallies are merged in chunk order, so the odds are bit-identical whether the
// run used 1 thread or 64.

#include <cstdint>
#include <functional>
#include <vector>

#include "street/hoops/model.hpp"

namespace street::hoops {

struct TitleOdds {
    std::uint32_t team_id = 0;
    double title_probability = 0.0;
    double finals_probability = 0.0;
    double playoff_probability = 0.0;
    double average_wins = 0.0;
};

struct TitleOddsOptions {
    int playoff_teams = 8;
    int rounds = 2; ///< regular-season round robins
    /// Called from the calling thread roughly every 50 ms with (seasons done, total).
    std::function<void(int, int)> progress;
};

struct TitleOddsReport {
    std::vector<TitleOdds> odds; ///< best title odds first
    int seasons = 0;
    std::uint64_t games = 0; ///< regular-season and playoff games simulated
    unsigned threads = 1;
    double seconds = 0.0;
};

/// Seasons simulated per chunk; part of the reproducibility contract above.
inline constexpr int kSeasonsPerChunk = 8;

/// threads = 0 uses std::thread::hardware_concurrency(). Throws std::invalid_argument
/// for seasons < 1 or an invalid league / playoff size.
std::vector<TitleOdds> title_odds(const std::vector<Team>& league, int seasons,
                                  std::uint64_t seed, unsigned threads = 0);

/// Same simulation, plus throughput figures and an optional progress callback.
TitleOddsReport simulate_title_odds(const std::vector<Team>& league, int seasons,
                                    std::uint64_t seed, unsigned threads = 0,
                                    const TitleOddsOptions& options = {});

} // namespace street::hoops
