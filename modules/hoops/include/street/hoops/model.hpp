#pragma once
// The basketball data model: ratings, players, teams, and the playing styles
// ("identities") that turn a roster into a way of playing.
//
// Everything here is plain data. The game engine reads ratings through
// logistic models, so a rating is best read as "how far from league average":
// 70 is a solid rotation player, 80 a starter-quality specialist, 90+ elite.

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "street/core/term.hpp"

namespace street::hoops {

enum class Position { PG, SG, SF, PF, C };

std::string_view position_name(Position position) noexcept; ///< "PG", "SG", ...
bool is_guard(Position position) noexcept;                   ///< PG or SG
bool is_big(Position position) noexcept;                     ///< PF or C

/// Twelve 1–99 ratings. Shooting (inside/mid/three/free_throw), creation
/// (passing/handling), defense (perimeter_d/interior_d), rebounding, and
/// physical/mental traits (athleticism, stamina, clutch).
struct Ratings {
    int inside = 50, mid = 50, three = 50, free_throw = 50;
    int passing = 50, handling = 50;
    int perimeter_d = 50, interior_d = 50, rebounding = 50;
    int athleticism = 50, stamina = 50, clutch = 50;

    /// Single 40–99 number in the spirit of video-game overalls. Each skill group
    /// counts its best components most, so specialists are not punished for
    /// the skills their role never uses (a rim protector's three-point rating).
    int overall() const noexcept;
};

struct Player {
    std::uint32_t id = 0; ///< unique across the league (team id * 100 + slot)
    std::string name, nickname;
    Position position = Position::SF;
    int jersey = 0, age = 25;
    std::string archetype; ///< "Sharpshooter", "Rim Protector", ...
    Ratings ratings;
    bool star = false; ///< franchise player (each league team has one or two)

    std::string last_name() const;     ///< "Greene"
    std::string short_name() const;    ///< "D. Greene"
    std::string display_name() const;  ///< Darius "Metaphor" Greene
};

struct Team {
    std::uint32_t id = 0;
    std::string city, name, abbrev;
    term::Rgb color;
    std::string identity; ///< playing style name, see play_style()
    std::vector<Player> roster; ///< depth-chart order: five starters first
    std::string arena;          ///< home court, for flavor text
    std::string sound; ///< closest beats style id for the city: boombap, trap, lofi, drill, funk

    std::string full_name() const; ///< "Queens Bridge Poets"
    /// Minutes-weighted rating of the rotation (starters count most).
    int overall() const;
    std::vector<const Player*> stars() const;
    const Player* find_player(std::uint32_t player_id) const noexcept;
};

/// What a team identity means on the floor. The engine multiplies these into
/// possession length, shot selection, defense and rebounding, so two teams with
/// identical talent still produce recognisably different box scores.
struct PlayStyle {
    std::string_view name;
    std::string_view description;
    double pace = 99.0;         ///< possessions per 48 minutes it pushes toward (league ~100)
    double rim = 1.0;           ///< shot-mix multipliers
    double mid = 1.0;
    double three = 1.0;
    double assist = 1.0;        ///< share of shots created by a pass
    double usage_focus = 1.0;   ///< exponent on usage: > 1 funnels shots to the stars
    double defense = 0.0;       ///< logit shift on opponents' make probability (< 0 = tougher)
    double pressure = 1.0;      ///< multiplier on turnovers forced
    double crash = 1.0;         ///< offensive-rebounding aggressiveness
    double transition = 1.0;    ///< how often it runs after stops
    double draw_foul = 1.0;     ///< multiplier on shooting fouls drawn
};

/// Every named identity, starting with the neutral "Balanced" profile.
std::span<const PlayStyle> play_styles() noexcept;
/// Case-insensitive lookup; unknown identities fall back to "Balanced".
const PlayStyle& play_style(std::string_view identity) noexcept;

} // namespace street::hoops
