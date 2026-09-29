#include <algorithm>
#include <cmath>
#include <format>
#include <stdexcept>

#include "street/hoops/game.hpp"

namespace street::hoops {

std::string_view zone_name(ShotZone zone) noexcept {
    switch (zone) {
    case ShotZone::Rim: return "RIM";
    case ShotZone::Mid: return "MID";
    case ShotZone::Three: return "3PT";
    }
    return "?";
}

double PlayerLine::game_score() const noexcept {
    return pts + 0.4 * fgm - 0.7 * fga - 0.4 * (fta - ftm) + 0.7 * oreb + 0.3 * dreb + stl +
           0.7 * ast + 0.7 * blk - 0.4 * pf - tov;
}

int GameResult::winner_side() const noexcept { return home_score >= away_score ? kHome : kAway; }

std::uint32_t GameResult::winner_id() const noexcept {
    return winner_side() == kHome ? home_id : away_id;
}

std::uint32_t GameResult::loser_id() const noexcept {
    return winner_side() == kHome ? away_id : home_id;
}

int GameResult::margin() const noexcept { return std::abs(home_score - away_score); }

const PlayerLine* GameResult::find_line(std::uint32_t player_id) const noexcept {
    for (const auto& side : box)
        for (const auto& line : side)
            if (line.player_id == player_id) return &line;
    return nullptr;
}

int GameResult::side_of(std::uint32_t player_id) const noexcept {
    for (int side = 0; side < 2; ++side)
        for (const auto& line : box[static_cast<std::size_t>(side)])
            if (line.player_id == player_id) return side;
    return -1;
}

const PlayerLine& GameResult::top_performer() const {
    const PlayerLine* best = nullptr;
    for (const auto& side : box)
        for (const auto& line : side)
            if (best == nullptr || line.game_score() > best->game_score()) best = &line;
    if (best == nullptr) throw std::logic_error("GameResult::top_performer: empty box score");
    return *best;
}

TeamTotals team_totals(const GameResult& result, int side) {
    TeamTotals t;
    for (const auto& l : result.box[side == kHome ? 0 : 1]) {
        t.pts += l.pts;
        t.fgm += l.fgm;
        t.fga += l.fga;
        t.tpm += l.tpm;
        t.tpa += l.tpa;
        t.ftm += l.ftm;
        t.fta += l.fta;
        t.oreb += l.oreb;
        t.dreb += l.dreb;
        t.ast += l.ast;
        t.stl += l.stl;
        t.blk += l.blk;
        t.tov += l.tov;
        t.pf += l.pf;
        t.minutes += l.minutes;
    }
    return t;
}

std::string format_clock(int period, double clock) {
    const std::string label = period <= 4 ? std::format("Q{}", period) : std::format("OT{}", period - 4);
    clock = std::max(clock, 0.0);
    if (clock < 10.0 && clock > 0.0) {
        // Tenths inside the last ten seconds, like a real scoreboard. Truncate, never round up.
        const int tenths = static_cast<int>(clock * 10.0);
        return std::format("{} 00:{:02}.{}", label, tenths / 10, tenths % 10);
    }
    const int whole = static_cast<int>(std::ceil(clock - 1e-9));
    return std::format("{} {:02}:{:02}", label, whole / 60, whole % 60);
}

} // namespace street::hoops
