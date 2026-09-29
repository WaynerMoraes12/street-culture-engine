#pragma once
// Internal: the play-by-play announcer.
//
// Pure text generation from templates. Every function draws only from the
// "flavor" generator it is given, never from the game's main stream, so the
// commentary can be switched off without changing a single possession.

#include <string>

#include "street/core/rng.hpp"
#include "street/hoops/game.hpp"
#include "street/hoops/model.hpp"

namespace street::hoops::detail {

/// Everything the announcer needs to call a field-goal attempt.
struct ShotCall {
    const Player* shooter = nullptr;
    const Player* passer = nullptr;   ///< set when the make is assisted
    const Player* defender = nullptr; ///< matched defender
    const Player* blocker = nullptr;  ///< set when the shot was blocked
    const Team* team = nullptr;       ///< the offense
    ShotZone zone = ShotZone::Rim;
    double distance = 0.0; ///< feet from the rim
    bool made = false;
    bool fouled = false;   ///< shooting foul on the attempt (and-one when made)
    bool putback = false;
    bool fast_break = false;
    bool heave = false;
    bool dunk = false; ///< athletic finish right at the rim
    bool running_on_fumes = false; ///< a star scoring on nearly empty legs
};

std::string call_shot(const ShotCall& call, Rng& flavor);
std::string call_block(const ShotCall& call, Rng& flavor);

enum class FreeThrowTrip { Shooting, AndOne, Bonus, Intentional };
std::string call_free_throws(const Player& shooter, int made, int attempts, FreeThrowTrip trip,
                             Rng& flavor);

enum class TurnoverKind { BadPass, Travel, OutOfBounds, OffensiveFoul, ShotClock, ThreeSeconds };
std::string call_steal(const Player& thief, const Player& victim, Rng& flavor);
std::string call_turnover(TurnoverKind kind, const Player& player, const Player& defender,
                          const Team& team, Rng& flavor);

std::string call_rebound(const Player& player, bool offensive, Rng& flavor);
std::string call_heating_up(const Player& player, Rng& flavor);
std::string call_on_fire(const Player& player, Rng& flavor);
std::string call_fire_out(const Player& player, Rng& flavor);
std::string call_foul(const Player& fouler, const Player& victim, bool intentional, Rng& flavor);
std::string call_foul_out(const Player& player);
std::string call_substitution(const Player& in, const Player& out);
std::string call_tipoff(const Player& winner, const Team& team, Rng& flavor);

/// Situational tag appended to a made basket late in games ("Tie game!",
/// "Bridge Poets take the lead!", "Dagger!"). Empty when nothing is at stake.
struct Situation {
    int period = 1;
    double clock = 0.0;
    int lead_before = 0; ///< offense lead before the basket (negative = trailing)
    int lead_after = 0;
    bool game_winner = false; ///< basket at the final buzzer that won it
    bool period_buzzer = false;
    ShotZone zone = ShotZone::Rim;
};
std::string call_situation(const Situation& situation, const Team& team, Rng& flavor);

// ── the legends in the booth ────────────────────────────────────────────────
// Broadcaster-style nods to Jordan, Kobe, LeBron and Iverson. They are always
// comparisons ("shades of", "-esque"), attached to the move just described,
// and deliberately rare so they land when they appear.

/// Scoring milestone line for a player who just reached 40 or 50 points.
std::string call_milestone(const Player& player, int points, Rng& flavor);
/// The sixth made three of a night: the 1992 Finals shrug.
std::string call_shrug(const Player& player, Rng& flavor);

std::string call_period_end(int period, const Team& home, const Team& away, int home_score,
                            int away_score, bool game_over);

} // namespace street::hoops::detail
