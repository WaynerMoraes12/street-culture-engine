#pragma once
// Possession-by-possession game simulation.
//
// simulate_game() plays four 12-minute quarters (plus 5-minute overtimes)
// with a 24-second shot clock. Every possession chooses a ball handler and a
// shooter by usage, a shot zone from ratings and team identity, and resolves
// turnovers, steals, blocks, fouls, free throws, rebounds and assists through
// logistic models of the players involved. A rotation system manages fatigue,
// foul trouble, crunch time and garbage time so minutes look like a real box.
//
// Determinism: the result is a pure function of (home, away, rng state,
// options). Commentary and shot coordinates draw from a separate "flavor"
// stream, so turning play-by-play or shot recording on or off never changes
// the score, the box score or the rng state left behind.

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "street/core/rng.hpp"
#include "street/hoops/model.hpp"

namespace street::hoops {

inline constexpr int kHome = 0; ///< side index of the home team in GameResult arrays
inline constexpr int kAway = 1;

enum class ShotZone { Rim, Mid, Three };
std::string_view zone_name(ShotZone zone) noexcept; ///< "RIM", "MID", "3PT"

/// One player's box-score line.
struct PlayerLine {
    std::uint32_t player_id = 0;
    double minutes = 0.0;
    int pts = 0, fgm = 0, fga = 0, tpm = 0, tpa = 0, ftm = 0, fta = 0;
    int oreb = 0, dreb = 0, ast = 0, stl = 0, blk = 0, tov = 0, pf = 0, plus_minus = 0;
    bool starter = false;

    int reb() const noexcept { return oreb + dreb; }
    /// John Hollinger's Game Score: a one-number summary of a stat line
    /// (10 is an average night, 40 is a monster game).
    double game_score() const noexcept;
};

/// A field-goal attempt located on a half court, in feet.
/// x runs sideline to sideline (-25 .. 25, 0 = through the rim); y is the
/// distance from the baseline (0 .. 47). The rim sits at (0, kRimY).
struct Shot {
    std::uint32_t player_id = 0;
    int side = kHome;
    double x = 0.0, y = 0.0;
    int value = 2; ///< 2 or 3
    bool made = false;
    ShotZone zone = ShotZone::Rim;
    int period = 1;
};

inline constexpr double kRimY = 5.25;         ///< rim center, feet from the baseline
inline constexpr double kThreeRadius = 23.75; ///< arc distance from the rim center
inline constexpr double kCornerThree = 22.0;  ///< straight-line corner distance

enum class PlayKind {
    Info,         ///< tip-off, period breaks, final
    Score,        ///< made field goal
    Miss,         ///< missed field goal
    FreeThrows,   ///< a trip to the line (one play per trip)
    Turnover,
    Steal,
    Block,
    Rebound,
    Foul,
    Substitution,
    Streak,       ///< heating up / on fire / fire's out
};

/// One line of play-by-play. clock is the time left in the period, in seconds.
struct Play {
    int period = 1;
    double clock = 0.0;
    int side = -1; ///< kHome, kAway, or -1 for neutral events
    std::string text;
    int home_score = 0, away_score = 0; ///< score after the play
    bool highlight = false;             ///< dunks, blocks, clutch buckets, buzzer beaters...
    PlayKind kind = PlayKind::Info;
    int points = 0; ///< points scored on this play
};

struct GameResult {
    std::uint32_t home_id = 0, away_id = 0;
    int home_score = 0, away_score = 0, overtimes = 0;
    std::array<std::vector<int>, 2> period_scores; ///< [side][period]
    std::array<std::vector<PlayerLine>, 2> box;    ///< [0] home, [1] away; roster order
    std::vector<Shot> shots;
    std::vector<Play> plays;

    std::uint32_t winner_id() const noexcept;
    std::uint32_t loser_id() const noexcept;
    int winner_side() const noexcept;
    int score(int side) const noexcept { return side == kHome ? home_score : away_score; }
    std::uint32_t team_id(int side) const noexcept { return side == kHome ? home_id : away_id; }
    int margin() const noexcept; ///< absolute point difference
    int periods() const noexcept { return static_cast<int>(period_scores[0].size()); }
    /// Line for a player in either box, or nullptr.
    const PlayerLine* find_line(std::uint32_t player_id) const noexcept;
    /// Side (kHome/kAway) a player's line is in, or -1.
    int side_of(std::uint32_t player_id) const noexcept;
    /// Highest Game Score of the night across both teams (throws on an empty box).
    const PlayerLine& top_performer() const;
};

/// Team totals derived from a box score.
struct TeamTotals {
    int pts = 0, fgm = 0, fga = 0, tpm = 0, tpa = 0, ftm = 0, fta = 0;
    int oreb = 0, dreb = 0, ast = 0, stl = 0, blk = 0, tov = 0, pf = 0;
    double minutes = 0.0;

    int reb() const noexcept { return oreb + dreb; }
    double fg_pct() const noexcept { return fga > 0 ? static_cast<double>(fgm) / fga : 0.0; }
    double tp_pct() const noexcept { return tpa > 0 ? static_cast<double>(tpm) / tpa : 0.0; }
    double ft_pct() const noexcept { return fta > 0 ? static_cast<double>(ftm) / fta : 0.0; }
};

TeamTotals team_totals(const GameResult& result, int side);

struct GameOptions {
    bool record_plays = true;  ///< build play-by-play commentary
    bool record_shots = true;  ///< keep shot locations for charts
    int period_minutes = 12;   ///< quarter length; overtime periods are always 5 minutes

    /// Scores and box scores only: what seasons and Monte Carlo runs use.
    static GameOptions fast() noexcept { return GameOptions{false, false, 12}; }
};

/// Plays a full game. Throws std::invalid_argument if a roster has fewer than
/// five players or period_minutes is not positive.
GameResult simulate_game(const Team& home, const Team& away, Rng& rng,
                         const GameOptions& options = {});

/// "Q4 01:12", "OT1 00:03.4" — tenths are shown inside the final ten seconds.
std::string format_clock(int period, double clock);

} // namespace street::hoops
