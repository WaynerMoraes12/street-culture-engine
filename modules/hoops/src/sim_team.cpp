#include "sim_team.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "depth_chart.hpp"

namespace street::hoops::detail {
namespace {

double logistic(double x) { return 1.0 / (1.0 + std::exp(-x)); }

double ratio_pow(int rating, double exponent) {
    return std::pow(static_cast<double>(std::max(rating, 1)) / 70.0, exponent);
}

SimPlayer derive(const Player& player, const PlayStyle& style) {
    const Ratings& r = player.ratings;
    SimPlayer p;
    p.player = &player;

    // Usage: how often the offense runs through him. Driven by his best scoring
    // skill, handle and burst; stars get the benefit of the doubt.
    std::array<int, 3> scoring = {r.inside, r.mid, r.three};
    std::sort(scoring.begin(), scoring.end(), [](int a, int b) { return a > b; });
    const double creation = 0.55 * scoring[0] + 0.20 * scoring[1] + 0.10 * r.handling +
                            0.15 * r.athleticism;
    const double usage = std::pow(creation / 70.0, 2.5) * (player.star ? 1.10 : 1.0);
    p.usage = std::pow(usage, style.usage_focus);

    p.rim_tend = std::exp(0.050 * (r.inside - 70) + 0.020 * (r.athleticism - 70));
    p.mid_tend = std::exp(0.040 * (r.mid - 70));
    p.three_tend = std::exp(0.055 * (r.three - 70));

    p.rim_skill = 0.017 * (0.7 * r.inside + 0.3 * r.athleticism - 70.0);
    p.mid_skill = 0.018 * (r.mid - 70);
    p.three_skill = 0.017 * (r.three - 70);
    p.ft_prob = logistic(1.25 + 0.045 * (r.free_throw - 75));

    p.ball_security = std::exp(-0.018 * (r.handling - 70) - 0.010 * (r.passing - 70));
    p.steal_weight = ratio_pow(r.perimeter_d, 3.0) * ratio_pow(r.athleticism, 1.0);
    p.block_weight = ratio_pow(r.interior_d, 4.0) * ratio_pow(r.athleticism, 2.0);
    p.rebound_weight = ratio_pow(r.rebounding, 3.0);
    p.pass_weight = ratio_pow(r.passing, 4.0);
    p.foul_weight = (is_big(player.position) ? 1.4 : 1.0) * std::exp(-0.01 * (r.perimeter_d - 70));
    p.perimeter_d = 0.010 * (r.perimeter_d - 70);
    p.interior_d = 0.012 * (r.interior_d - 70);
    p.clutch = 0.010 * (r.clutch - 70);
    p.block_power = std::exp(3.3 * p.interior_d);
    p.finish_factor = std::exp(-0.5 * p.rim_skill);
    p.foul_draw = std::exp(0.012 * (r.athleticism - 70) + 0.008 * (r.inside - 70));
    p.pressure_factor = std::exp(0.3 * p.perimeter_d);
    p.drain = (0.30 + 0.5 * (100 - r.stamina) / 100.0) / 600.0;
    p.closer = std::clamp((r.overall() - 66) / 22.0, 0.0, 1.0);
    return p;
}

int foul_limit(int period, double clock) {
    // Classic coaching rule of thumb: two in the first, three in the second...
    if (period >= 4) return clock <= 360.0 ? kFoulOut : 5;
    return period + 1;
}

bool valid_lineup(const SimTeam& team, const std::array<int, 5>& lineup) {
    int guards = 0, bigs = 0;
    for (int idx : lineup) {
        const Position pos = team.players[static_cast<std::size_t>(idx)].player->position;
        guards += is_guard(pos) ? 1 : 0;
        bigs += is_big(pos) ? 1 : 0;
    }
    return guards >= 1 && bigs >= 1 && guards <= 3 && bigs <= 3;
}

void sort_lineup(SimTeam& team) {
    auto key = [&](int idx) {
        return static_cast<int>(team.players[static_cast<std::size_t>(idx)].player->position) * 100 +
               idx;
    };
    std::sort(team.lineup.begin(), team.lineup.end(),
              [&](int a, int b) { return key(a) < key(b); });
}

} // namespace

SimTeam make_sim_team(const Team& team, Rng& rng) {
    SimTeam t;
    t.team = &team;
    t.style = &play_style(team.identity);
    const std::vector<double> plan = depth_minutes(team.roster.size());

    double total = 0.0;
    t.players.reserve(team.roster.size());
    for (std::size_t i = 0; i < team.roster.size(); ++i) {
        SimPlayer p = derive(team.roster[i], *t.style);
        // Coaches ride the hot hand and stars a little longer on some nights.
        const double jitter = std::clamp(1.0 + rng.normal(0.0, 0.05), 0.85, 1.15);
        p.floor_share = plan[i] / 48.0 * jitter * (team.roster[i].star ? 1.06 : 1.0);
        total += p.floor_share;
        t.players.push_back(p);
    }
    for (auto& p : t.players) p.floor_share *= 5.0 / total; // five players always on the floor

    for (int i = 0; i < 5; ++i) {
        t.lineup[static_cast<std::size_t>(i)] = i;
        t.players[static_cast<std::size_t>(i)].on_court = true;
    }
    sort_lineup(t);
    return t;
}

Substitutions manage_rotation(SimTeam& team, const RotationContext& c) {
    // Every player gets a "need to play" score in seconds: how far he is behind
    // his planned share of the minutes so far, nudged by energy and fouls, and
    // overridden late in close games (closers in) or blowouts (starters out).
    const int spread = std::abs(c.margin);
    const bool crunch = c.period > 4 || (c.period == 4 && c.clock <= 300.0 && spread <= 8);
    const bool garbage = c.period >= 4 && ((spread >= 20 && c.clock <= 480.0) || spread >= 30);
    const int limit = foul_limit(c.period, c.clock);

    constexpr double kEnergySeconds = 300.0; // one full tank is worth five minutes of need
    constexpr double kHysteresis = 360.0;    // don't churn the lineup over small differences
    constexpr double kOverride = 1500.0;

    const std::size_t n = team.players.size();
    std::array<double, 32> need{};
    const std::size_t tracked = std::min<std::size_t>(n, need.size());
    for (std::size_t i = 0; i < tracked; ++i) {
        const SimPlayer& p = team.players[i];
        double v = p.floor_share * c.elapsed - p.played + (p.energy - 0.8) * kEnergySeconds;
        if (p.fouls >= limit && !crunch) v -= 900.0;
        if (crunch) v += kOverride * p.closer;
        if (garbage) v -= kOverride * p.closer;
        need[i] = v;
    }

    Substitutions subs;
    for (int pass = 0; pass < 5; ++pass) {
        int best_slot = -1, best_in = -1;
        double best_gain = kHysteresis;
        for (int slot = 0; slot < 5; ++slot) {
            const auto out = static_cast<std::size_t>(team.lineup[static_cast<std::size_t>(slot)]);
            const bool forced = team.players[out].fouls >= kFoulOut;
            for (std::size_t in = 0; in < tracked; ++in) {
                const SimPlayer& candidate = team.players[in];
                if (candidate.on_court || candidate.fouls >= kFoulOut) continue;
                const double gain = need[in] - need[out] + (forced ? 1e9 : 0.0);
                if (gain <= best_gain) continue;
                std::array<int, 5> trial = team.lineup;
                trial[static_cast<std::size_t>(slot)] = static_cast<int>(in);
                // A fouled-out player must leave even if the replacement breaks positions.
                if (!forced && !valid_lineup(team, trial)) continue;
                best_gain = gain;
                best_slot = slot;
                best_in = static_cast<int>(in);
            }
        }
        if (best_slot < 0) break;
        const int out = team.lineup[static_cast<std::size_t>(best_slot)];
        team.players[static_cast<std::size_t>(out)].on_court = false;
        team.players[static_cast<std::size_t>(best_in)].on_court = true;
        team.lineup[static_cast<std::size_t>(best_slot)] = best_in;
        subs.list[static_cast<std::size_t>(subs.count++)] = {out, best_in};
    }
    if (subs.count > 0) sort_lineup(team);
    return subs;
}

} // namespace street::hoops::detail
