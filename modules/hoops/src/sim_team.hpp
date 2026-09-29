#pragma once
// Internal: per-game simulation state for a team and its players.
//
// Ratings are converted once per game into the quantities the engine needs
// every possession (usage weights, logit shifts, turnover multipliers), so
// the hot loop does arithmetic instead of re-deriving them from 1–99 numbers.

#include <array>
#include <utility>
#include <vector>

#include "street/core/rng.hpp"
#include "street/hoops/model.hpp"

namespace street::hoops::detail {

struct SimPlayer {
    const Player* player = nullptr;

    // ── derived from ratings (constant during the game) ──
    double usage = 1.0;                                 ///< relative share of shots
    double rim_tend = 1.0, mid_tend = 1.0, three_tend = 1.0; ///< shot-zone appetite
    double rim_skill = 0.0, mid_skill = 0.0, three_skill = 0.0; ///< make-probability logit shifts
    double ft_prob = 0.75;
    double ball_security = 1.0; ///< turnover multiplier (< 1 protects the ball)
    double steal_weight = 1.0, block_weight = 1.0, rebound_weight = 1.0, pass_weight = 1.0;
    double foul_weight = 1.0;
    double perimeter_d = 0.0, interior_d = 0.0; ///< logit shifts taken off opponents' shots
    double clutch = 0.0;                        ///< logit shift in clutch time
    // Pre-exponentiated factors for the per-shot hot path.
    double block_power = 1.0;    ///< how much more often than average he blocks shots
    double finish_factor = 1.0;  ///< good finishers get blocked less
    double foul_draw = 1.0;      ///< athletic slashers draw more shooting fouls
    double pressure_factor = 1.0; ///< contribution to forced turnovers (multiplicative)
    double drain = 0.0;                         ///< energy lost per second on the floor
    double closer = 0.0; ///< 0..1: how badly the coach wants him on the floor late

    // ── game state ──
    double energy = 1.0;
    double played = 0.0; ///< seconds on the floor
    double floor_share = 0.0; ///< planned fraction of game time on the floor (team sum = 5)
    int fouls = 0;
    int streak = 0; ///< consecutive made field goals
    bool on_fire = false;
    bool on_court = false;
};

struct SimTeam {
    const Team* team = nullptr;
    const PlayStyle* style = nullptr;
    std::vector<SimPlayer> players; ///< roster order, parallel to the box score
    std::array<int, 5> lineup{};    ///< indices into players, sorted PG..C for matchups
    int score = 0;
    int period_fouls = 0;
    int rim_attempts = 0;   ///< tracked so defenses can adjust to paint-heavy offenses
    int shot_attempts = 0;
};

inline constexpr int kFoulOut = 6;

/// Builds the per-game state. Draws small per-game minute-plan jitter from rng.
SimTeam make_sim_team(const Team& team, Rng& rng);

/// Game situation the coach looks at when deciding substitutions.
struct RotationContext {
    int period = 1;
    double clock = 0.0;      ///< seconds left in the period
    double elapsed = 0.0;    ///< seconds played in the game
    int margin = 0;          ///< own score minus opponent's
};

using Substitution = std::pair<int, int>; ///< (player out, player in)

/// Fixed-capacity list: the rotation runs inside the game loop, so no allocation.
struct Substitutions {
    std::array<Substitution, 5> list{};
    int count = 0;
};

/// Swaps players whose playing time, energy, fouls or the game situation call for
/// it and reports the swaps. Deterministic: the coach never rolls dice.
Substitutions manage_rotation(SimTeam& team, const RotationContext& context);

} // namespace street::hoops::detail
