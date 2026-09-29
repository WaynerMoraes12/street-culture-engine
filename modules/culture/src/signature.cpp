#include "street/culture/signature.hpp"

#include <algorithm>
#include <cmath>
#include <format>

#include "street/core/stats.hpp"
#include "street/core/text.hpp"

namespace street::culture {

SignatureShoe design_signature(const hoops::Player& player, const hoops::Team& team,
                               const std::vector<std::string>& taken) {
    // SKU: "SIG-" + up to five letters of the nickname, e.g. Darius "Metaphor" Greene → SIG-METAP.
    std::string code;
    const std::string source = player.nickname.empty() ? player.last_name() : player.nickname;
    for (char c : text::to_upper(text::fold_accents(source)))
        if (c >= 'A' && c <= 'Z' && code.size() < 5) code += c;
    if (code.empty()) code = std::format("P{}", player.id);

    std::string sku = "SIG-" + code;
    for (int n = 2; std::find(taken.begin(), taken.end(), sku) != taken.end(); ++n)
        sku = std::format("SIG-{}{}", code, n);

    // Better players get pricier, scarcer, hotter shoes: overall 80 → $140 and ~60k pairs,
    // overall 97 → $200 and 22k pairs. Retail is rounded to $5 like real MSRPs.
    const double talent = stats::clamp((player.ratings.overall() - 75.0) / 22.0, 0.0, 1.0);
    const auto retail_dollars =
        static_cast<std::int64_t>(std::lround((120.0 + 80.0 * talent) / 5.0) * 5);

    kicks::Sneaker shoe;
    shoe.sku = sku;
    shoe.name = (player.nickname.empty() ? player.last_name() : player.nickname) + " 1";
    shoe.colorway = team.city + " Home";
    shoe.year = 2026;
    shoe.retail = kicks::dollars(retail_dollars);
    shoe.pairs = static_cast<int>(std::lround(70'000.0 - 48'000.0 * talent));
    // A debut signature starts as a mid-tier release (resale ~1.1–2.4× retail, see
    // kicks::resale_multiplier); the athlete's season has to earn the rest.
    shoe.base_hype = 0.25 + 0.30 * talent + (player.star ? 0.05 : 0.0);
    shoe.story = std::format("{}'s first signature, raffled on opening night", player.short_name());
    return {player.id, team.id, std::move(shoe)};
}

std::vector<const hoops::Player*> signature_athletes(const std::vector<hoops::Team>& league,
                                                     std::size_t count) {
    std::vector<const hoops::Player*> pool;
    for (const auto& team : league)
        for (const auto& player : team.roster) pool.push_back(&player);
    std::stable_sort(pool.begin(), pool.end(), [](const hoops::Player* a, const hoops::Player* b) {
        if (a->star != b->star) return a->star;
        if (a->ratings.overall() != b->ratings.overall())
            return a->ratings.overall() > b->ratings.overall();
        return a->id < b->id;
    });
    pool.resize(std::min(count, pool.size()));
    return pool;
}

double prior_game_score(const hoops::Player& player) noexcept {
    // Roughly: a 70-overall rotation player averages ~8, an 90-overall star ~20.
    return std::max(4.0, 8.0 + (player.ratings.overall() - 70) * 0.6);
}

bool is_big_night(const hoops::PlayerLine& line) noexcept {
    const int doubles = (line.pts >= 10) + (line.reb() >= 10) + (line.ast >= 10);
    return line.pts >= 35 || doubles >= 3 || line.game_score() >= 30.0;
}

std::optional<HypeImpulse> performance_impulse(const hoops::PlayerLine& line,
                                               double expected_game_score, bool won,
                                               const std::string& nickname,
                                               const std::string& shoe_name) {
    if (line.minutes <= 0.0) return std::nullopt;
    const double surprise = line.game_score() - expected_game_score;
    const int doubles = (line.pts >= 10) + (line.reb() >= 10) + (line.ast >= 10);

    double magnitude = surprise / 60.0 + (won ? 0.01 : -0.005);
    if (line.pts >= 40) magnitude += 0.10;
    if (line.pts >= 50) magnitude += 0.12;
    if (doubles >= 3) magnitude += 0.06;
    magnitude = stats::clamp(magnitude, -0.08, 0.30);
    if (std::abs(magnitude) < 0.03) return std::nullopt; // priced in

    std::string headline;
    if (line.pts >= 50) {
        headline = std::format("{} drops {} — the {} sells out everywhere", nickname, line.pts, shoe_name);
    } else if (line.pts >= 40) {
        headline = std::format("{} goes for {} in the {}", nickname, line.pts, shoe_name);
    } else if (doubles >= 3) {
        headline = std::format("{} posts a triple-double ({}/{}/{})", nickname, line.pts, line.reb(), line.ast);
    } else if (magnitude < 0) {
        headline = std::format("{} held to {} on {}-{} shooting", nickname, line.pts, line.fgm, line.fga);
    } else {
        headline = std::format("{} puts up {} and {} — the {} heats up", nickname, line.pts,
                               line.reb() >= line.ast ? std::format("{} boards", line.reb())
                                                      : std::format("{} dimes", line.ast),
                               shoe_name);
    }
    return HypeImpulse{magnitude, std::move(headline)};
}

} // namespace street::culture
