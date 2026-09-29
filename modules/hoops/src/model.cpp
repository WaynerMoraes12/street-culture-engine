#include "street/hoops/model.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>

#include "depth_chart.hpp"
#include "street/core/text.hpp"

namespace street::hoops {
namespace {

/// Weighted blend of three values, best first: rewards the skills a player leans on.
double best_of(std::array<int, 3> values, double w0, double w1, double w2) {
    std::sort(values.begin(), values.end(), std::greater<>());
    return w0 * values[0] + w1 * values[1] + w2 * values[2];
}

double best_of(int a, int b, double w_best) {
    const int hi = std::max(a, b);
    const int lo = std::min(a, b);
    return w_best * hi + (1.0 - w_best) * lo;
}

// Identity table. Multipliers are relative to a neutral team; the engine turns
// them into pace, shot mix, defensive logit shifts and rebounding weights.
constexpr std::array<PlayStyle, 9> kStyles = {{
    {"Balanced", "No gimmicks: take what the defense gives.", 99.0},
    {"Pace & Space", "Five-out spacing and early offense: threes and layups, nothing in between.",
     102.0, 1.05, 0.75, 1.12, 1.08, 1.00, 0.00, 1.00, 0.90, 1.15, 1.00},
    {"Run & Gun", "Push after every stop. First good look goes up; a track meet every night.",
     106.0, 1.15, 0.85, 1.10, 1.00, 1.00, 0.04, 1.10, 0.85, 1.60, 1.00},
    {"Three-Point Barrage", "The math says threes, so threes it is. Let it fly from anywhere.",
     101.0, 0.90, 0.65, 1.15, 1.10, 1.00, 0.00, 1.00, 0.90, 1.10, 0.90},
    {"Grit & Grind", "Slow it down, bump every cutter, own the glass and win ugly.", 94.0, 1.05,
     1.20, 0.85, 0.95, 1.05, -0.04, 1.10, 1.15, 0.70, 1.10},
    {"Paint Beasts", "Bully ball: pound it inside, crash the offensive glass, live at the line.",
     97.0, 1.30, 0.90, 0.75, 0.95, 1.00, -0.02, 1.00, 1.10, 0.90, 1.10},
    {"Lockdown D", "Switch everything, jump passing lanes, contest every single look.", 95.0,
     1.00, 1.05, 0.95, 1.00, 1.00, -0.07, 1.25, 1.00, 1.00, 1.00},
    {"Iso Heavy", "Clear out one side and let the stars cook one-on-one.", 96.0, 1.00, 1.35,
     0.95, 0.75, 1.35, 0.00, 1.00, 0.95, 0.90, 1.15},
    {"Jogo Bonito", "The beautiful game: the ball moves, the defense chases, everybody eats.",
     100.0, 1.10, 0.90, 1.10, 1.30, 0.80, 0.00, 1.05, 1.00, 1.20, 1.00},
}};

} // namespace

std::string_view position_name(Position position) noexcept {
    switch (position) {
    case Position::PG: return "PG";
    case Position::SG: return "SG";
    case Position::SF: return "SF";
    case Position::PF: return "PF";
    case Position::C: return "C";
    }
    return "?";
}

bool is_guard(Position position) noexcept {
    return position == Position::PG || position == Position::SG;
}

bool is_big(Position position) noexcept {
    return position == Position::PF || position == Position::C;
}

int Ratings::overall() const noexcept {
    const double scoring = 0.9 * best_of({inside, mid, three}, 0.55, 0.30, 0.15) + 0.1 * free_throw;
    const double creation = best_of(passing, handling, 0.65);
    const double defense = best_of(perimeter_d, interior_d, 0.70);
    const double raw = 0.36 * scoring + 0.14 * creation + 0.20 * defense + 0.10 * rebounding +
                       0.12 * athleticism + 0.04 * stamina + 0.04 * clutch;
    return static_cast<int>(std::clamp(std::lround(raw * 1.15), 40L, 99L));
}

std::string Player::last_name() const {
    const auto space = name.find_last_of(' ');
    return space == std::string::npos ? name : name.substr(space + 1);
}

std::string Player::short_name() const {
    const auto space = name.find_last_of(' ');
    if (space == std::string::npos || name.empty()) return name;
    // First code point of the first name (UTF-8 lead byte + continuation bytes).
    std::size_t end = 1;
    while (end < name.size() && (static_cast<unsigned char>(name[end]) & 0xC0) == 0x80) ++end;
    return name.substr(0, end) + ". " + name.substr(space + 1);
}

std::string Player::display_name() const {
    if (nickname.empty()) return name;
    const auto space = name.find(' ');
    if (space == std::string::npos) return name + " \"" + nickname + "\"";
    return name.substr(0, space) + " \"" + nickname + "\"" + name.substr(space);
}

std::string Team::full_name() const { return city + " " + name; }

int Team::overall() const {
    if (roster.empty()) return 0;
    const std::size_t n = std::max<std::size_t>(roster.size(), 5);
    const std::vector<double> minutes = detail::depth_minutes(n);
    double total = 0.0, weight = 0.0;
    for (std::size_t i = 0; i < roster.size(); ++i) {
        total += minutes[i] * roster[i].ratings.overall();
        weight += minutes[i];
    }
    return static_cast<int>(std::lround(total / weight));
}

std::vector<const Player*> Team::stars() const {
    std::vector<const Player*> out;
    for (const auto& p : roster)
        if (p.star) out.push_back(&p);
    return out;
}

const Player* Team::find_player(std::uint32_t player_id) const noexcept {
    for (const auto& p : roster)
        if (p.id == player_id) return &p;
    return nullptr;
}

std::span<const PlayStyle> play_styles() noexcept { return kStyles; }

const PlayStyle& play_style(std::string_view identity) noexcept {
    for (const auto& style : kStyles)
        if (text::iequals(style.name, identity)) return style;
    return kStyles.front();
}

namespace detail {

std::vector<double> depth_minutes(std::size_t size) {
    // Relative share of the floor for each depth slot: five starters, a sixth man,
    // then a falling bench. Slots past the twelfth barely see garbage time.
    constexpr std::array<double, 12> kShape = {1.00, 0.97, 0.94, 0.91, 0.88, 0.70,
                                               0.58, 0.50, 0.35, 0.20, 0.08, 0.04};
    constexpr double kTotal = 240.0, kCap = 40.0;
    if (size <= 5) return std::vector<double>(5, kTotal / 5.0); // an iron five plays it all

    std::vector<double> weights(size);
    for (std::size_t i = 0; i < weights.size(); ++i)
        weights[i] = i < kShape.size() ? kShape[i] : 0.02;

    // Scale to 240 minutes, capping anyone at 40 and redistributing the excess.
    std::vector<double> minutes(weights.size(), 0.0);
    std::vector<bool> capped(weights.size(), false);
    for (int pass = 0; pass < 8; ++pass) {
        double free_weight = 0.0, fixed = 0.0;
        for (std::size_t i = 0; i < weights.size(); ++i) {
            if (capped[i])
                fixed += kCap;
            else
                free_weight += weights[i];
        }
        if (free_weight <= 0.0) break;
        bool changed = false;
        for (std::size_t i = 0; i < weights.size(); ++i) {
            if (capped[i]) {
                minutes[i] = kCap;
                continue;
            }
            minutes[i] = (kTotal - fixed) * weights[i] / free_weight;
            if (minutes[i] > kCap) {
                capped[i] = true;
                changed = true;
            }
        }
        if (!changed) break;
    }
    return minutes;
}

} // namespace detail
} // namespace street::hoops
