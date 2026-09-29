#pragma once
// The league: twelve fictional franchises rooted in hip-hop geography.
//
// Rosters are handcrafted (names, nicknames, roles, depth chart) and the
// ratings are generated from each player's archetype and tier with a fixed
// internal seed, so the league is identical on every run and every platform.

#include <span>
#include <string_view>
#include <vector>

#include "street/hoops/model.hpp"

namespace street::hoops {

/// Rating template for a player role, before tier and per-player variation.
struct Archetype {
    std::string_view name;
    std::string_view description;
    Ratings base; ///< a typical starter in this role
};

std::span<const Archetype> archetypes() noexcept;

/// The twelve league teams, ids 1..12, ten players each (player id = team id * 100 + slot).
std::vector<Team> default_league();

/// Finds a team by abbreviation ("QNS"), team name ("Bridge Poets"), city ("Queens")
/// or full name, ignoring case. Returns nullptr when nothing matches.
const Team* find_team(const std::vector<Team>& league, std::string_view abbrev_or_name);

} // namespace street::hoops
