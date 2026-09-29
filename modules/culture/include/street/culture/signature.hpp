#pragma once
// Signature sneakers and the model that turns box scores into hype.

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "street/hoops/game.hpp"
#include "street/hoops/model.hpp"
#include "street/kicks/catalog.hpp"

namespace street::culture {

struct SignatureShoe {
    std::uint32_t player_id = 0;
    std::uint32_t team_id = 0;
    kicks::Sneaker sneaker;
};

/// Designs a player's first signature shoe: SKU from the nickname ("SIG-METAP"),
/// colorway from the team, retail and scarcity from how good the player is.
/// `taken` lists SKUs already in use; a digit is appended on collision.
SignatureShoe design_signature(const hoops::Player& player, const hoops::Team& team,
                               const std::vector<std::string>& taken = {});

/// Picks the league's `count` best players (stars first, then overall rating).
std::vector<const hoops::Player*> signature_athletes(const std::vector<hoops::Team>& league,
                                                     std::size_t count);

struct HypeImpulse {
    double magnitude = 0.0; ///< relative fair-value shock for kicks::Market::shock
    std::string headline;
};

/// How much a single game should move a signature shoe.
///
/// The market reacts to surprise, not to talent: a star's 28 points on an ordinary
/// night is priced in. `expected_game_score` is what the market already assumes
/// (the player's season average, or a rating-based prior early on). The shock grows
/// with the gap between tonight's Game Score and that expectation, plus jumps for
/// the moments that go viral (40- and 50-point games, triple-doubles), and is
/// clamped to [-8%, +30%]. Returns nothing for nights too ordinary to move the price.
std::optional<HypeImpulse> performance_impulse(const hoops::PlayerLine& line,
                                               double expected_game_score, bool won,
                                               const std::string& nickname,
                                               const std::string& shoe_name);

/// Rating-based guess of a player's typical Game Score, used before a season sample exists.
double prior_game_score(const hoops::Player& player) noexcept;

/// True for lines that deserve a headline: 35+ points, a triple-double, or Game Score >= 30.
bool is_big_night(const hoops::PlayerLine& line) noexcept;

} // namespace street::culture
