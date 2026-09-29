#include "street/hoops/season.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>

#include "street/core/rng.hpp"

namespace street::hoops {
namespace {

constexpr double kEloK = 20.0;
constexpr double kEloHomeEdge = 70.0; // Elo points of home-court advantage

/// Circle-method round robin: n-1 days (n even; a bye slot is added for odd n),
/// every team exactly once per day, home and away alternating around the circle.
std::vector<std::vector<Matchup>> round_robin(const std::vector<std::uint32_t>& ids) {
    std::vector<int> ring(ids.size());
    std::iota(ring.begin(), ring.end(), 0);
    if (ring.size() % 2 == 1) ring.push_back(-1); // bye
    const std::size_t m = ring.size();
    std::vector<std::vector<Matchup>> days;
    for (std::size_t day = 0; day + 1 < m; ++day) {
        std::vector<Matchup> games;
        for (std::size_t i = 0; i < m / 2; ++i) {
            const int a = ring[i];
            const int b = ring[m - 1 - i];
            if (a < 0 || b < 0) continue;
            // Alternate who hosts so each team's home games are spread over the cycle.
            const bool a_home = i == 0 ? day % 2 == 0 : i % 2 == 1;
            const auto ia = static_cast<std::size_t>(a), ib = static_cast<std::size_t>(b);
            games.push_back(a_home ? Matchup{ids[ia], ids[ib]} : Matchup{ids[ib], ids[ia]});
        }
        days.push_back(std::move(games));
        std::rotate(ring.begin() + 1, ring.end() - 1, ring.end()); // first slot stays fixed
    }
    return days;
}

bool same_pct(const Standing& a, const Standing& b) {
    // Cross-multiplication keeps the comparison exact (no floating point).
    return static_cast<long long>(a.wins) * b.games() == static_cast<long long>(b.wins) * a.games();
}

bool better_pct(const Standing& a, const Standing& b) {
    return static_cast<long long>(a.wins) * b.games() > static_cast<long long>(b.wins) * a.games();
}

void push_result(Standing& s, bool won) {
    const char c = won ? 'W' : 'L';
    if (s.streak.size() >= 2 && s.streak[0] == c)
        s.streak = std::string(1, c) + std::to_string(std::stoi(s.streak.substr(1)) + 1);
    else
        s.streak = std::string(1, c) + "1";
    s.last10.push_back(c);
    if (s.last10.size() > 10) s.last10.erase(0, s.last10.size() - 10);
}

} // namespace

double Standing::pct() const noexcept {
    return games() > 0 ? static_cast<double>(wins) / games() : 0.0;
}

double PlayerSeason::ts_pct() const noexcept {
    const double shots = fga + 0.44 * fta;
    return shots > 0.0 ? pts / (2.0 * shots) : 0.0;
}

Season::Season(std::vector<Team> teams, std::uint64_t seed, int rounds)
    : teams_(std::move(teams)), seed_(seed) {
    if (teams_.size() < 2) throw std::invalid_argument("Season: need at least two teams");
    if (rounds < 1) throw std::invalid_argument("Season: rounds must be at least 1");

    std::vector<std::uint32_t> ids;
    for (std::size_t i = 0; i < teams_.size(); ++i) {
        const Team& t = teams_[i];
        if (!team_by_id_.emplace(t.id, i).second)
            throw std::invalid_argument("Season: duplicate team id " + std::to_string(t.id));
        for (const auto& p : t.roster) team_by_player_.emplace(p.id, i);
        ids.push_back(t.id);
        Standing s;
        s.team_id = t.id;
        table_.push_back(s);
    }
    head_to_head_.assign(teams_.size(), std::vector<int>(teams_.size(), 0));

    // Each cycle mirrors the first one with home and away swapped; day order is
    // shuffled per cycle so the calendar feels less mechanical.
    const auto cycle = round_robin(ids);
    Rng calendar(seed_, 0xCA1E);
    for (int r = 0; r < rounds; ++r) {
        std::vector<std::vector<Matchup>> days = cycle;
        if (r % 2 == 1)
            for (auto& day : days)
                for (auto& game : day) std::swap(game.home_id, game.away_id);
        calendar.shuffle(days.begin(), days.end());
        for (auto& day : days) schedule_.push_back(std::move(day));
    }
}

int Season::games_per_team() const noexcept {
    if (schedule_.empty()) return 0;
    std::size_t games = 0;
    for (const auto& day : schedule_) games += day.size();
    return static_cast<int>(games * 2 / teams_.size());
}

const std::vector<Matchup>& Season::schedule(int day_index) const {
    if (day_index < 0 || day_index >= total_days())
        throw std::out_of_range("Season::schedule: no such day");
    return schedule_[static_cast<std::size_t>(day_index)];
}

std::size_t Season::team_index(std::uint32_t id) const {
    const auto it = team_by_id_.find(id);
    if (it == team_by_id_.end()) throw std::out_of_range("Season: unknown team id " + std::to_string(id));
    return it->second;
}

const Team& Season::team(std::uint32_t id) const { return teams_[team_index(id)]; }

const Team& Season::team_of(std::uint32_t player_id) const {
    const auto it = team_by_player_.find(player_id);
    if (it == team_by_player_.end())
        throw std::out_of_range("Season: unknown player id " + std::to_string(player_id));
    return teams_[it->second];
}

const Player& Season::player(std::uint32_t id) const {
    const Player* p = team_of(id).find_player(id);
    if (p == nullptr) throw std::out_of_range("Season: unknown player id " + std::to_string(id));
    return *p;
}

GameResult Season::play_game(std::uint32_t home_id, std::uint32_t away_id,
                             const GameOptions& options) {
    // One independent stream per game number: results never depend on options or
    // on how the caller paces the season.
    const std::uint64_t n = ++games_played_;
    Rng rng(mix64(seed_ ^ mix64(n)), n);
    GameResult result = simulate_game(team(home_id), team(away_id), rng, options);
    update_elo(result);
    return result;
}

void Season::update_elo(const GameResult& r) {
    Standing& home = table_[team_index(r.home_id)];
    Standing& away = table_[team_index(r.away_id)];
    const double gap = home.elo + kEloHomeEdge - away.elo;
    const double expected_home = 1.0 / (1.0 + std::pow(10.0, -gap / 400.0));
    const bool home_won = r.home_score > r.away_score;
    // FiveThirtyEight-style margin-of-victory multiplier: blowouts count more, but
    // less so when the favourite was expected to win big (autocorrelation guard).
    const double winner_gap = home_won ? gap : -gap;
    const double mov = std::log(r.margin() + 1.0) * 2.2 / (winner_gap * 0.001 + 2.2);
    const double delta = kEloK * mov * ((home_won ? 1.0 : 0.0) - expected_home);
    home.elo += delta;
    away.elo -= delta;
}

void Season::record_regular_season(const GameResult& r) {
    const std::size_t hi = team_index(r.home_id), ai = team_index(r.away_id);
    Standing& home = table_[hi];
    Standing& away = table_[ai];
    const bool home_won = r.home_score > r.away_score;
    home.points_for += r.home_score;
    home.points_against += r.away_score;
    away.points_for += r.away_score;
    away.points_against += r.home_score;
    (home_won ? home.wins : home.losses) += 1;
    (home_won ? away.losses : away.wins) += 1;
    (home_won ? home.home_wins : home.home_losses) += 1;
    push_result(home, home_won);
    push_result(away, !home_won);
    ++head_to_head_[home_won ? hi : ai][home_won ? ai : hi];

    for (int side = 0; side < 2; ++side) {
        const std::uint32_t team_id = r.team_id(side);
        for (const PlayerLine& l : r.box[static_cast<std::size_t>(side)]) {
            if (l.minutes <= 0.0) continue;
            PlayerSeason& s = player_stats_[l.player_id];
            s.player_id = l.player_id;
            s.team_id = team_id;
            ++s.games;
            s.minutes += l.minutes;
            s.pts += l.pts;
            s.fgm += l.fgm;
            s.fga += l.fga;
            s.tpm += l.tpm;
            s.tpa += l.tpa;
            s.ftm += l.ftm;
            s.fta += l.fta;
            s.reb += l.reb();
            s.ast += l.ast;
            s.stl += l.stl;
            s.blk += l.blk;
            s.tov += l.tov;
            s.game_score_total += l.game_score();
            s.best_game = std::max(s.best_game, l.pts);
        }
    }
}

std::vector<GameResult> Season::play_next_day(const GameOptions& options) {
    std::vector<GameResult> results;
    if (regular_season_done()) return results;
    const auto& games = schedule_[static_cast<std::size_t>(day_)];
    results.reserve(games.size());
    for (const Matchup& m : games) {
        results.push_back(play_game(m.home_id, m.away_id, options));
        record_regular_season(results.back());
    }
    ++day_;
    return results;
}

void Season::play_regular_season() {
    while (!regular_season_done()) play_next_day();
}

std::vector<Standing> Season::standings() const {
    std::vector<Standing> out = table_;
    std::stable_sort(out.begin(), out.end(), [](const Standing& a, const Standing& b) {
        if (!same_pct(a, b)) return better_pct(a, b);
        return a.team_id < b.team_id;
    });

    // Break ties within each group of equal win% by head-to-head record among the
    // group, then point differential, then points scored.
    for (std::size_t begin = 0; begin < out.size();) {
        std::size_t end = begin + 1;
        while (end < out.size() && same_pct(out[begin], out[end])) ++end;
        if (end - begin > 1) {
            std::vector<std::size_t> group;
            for (std::size_t k = begin; k < end; ++k) group.push_back(team_index(out[k].team_id));
            const auto h2h_pct = [&](std::size_t idx) {
                int w = 0, l = 0;
                for (std::size_t other : group) {
                    w += head_to_head_[idx][other];
                    l += head_to_head_[other][idx];
                }
                return w + l > 0 ? static_cast<double>(w) / (w + l) : 0.5;
            };
            std::sort(out.begin() + static_cast<std::ptrdiff_t>(begin),
                      out.begin() + static_cast<std::ptrdiff_t>(end),
                      [&](const Standing& a, const Standing& b) {
                          const double ha = h2h_pct(team_index(a.team_id));
                          const double hb = h2h_pct(team_index(b.team_id));
                          if (ha != hb) return ha > hb;
                          if (a.diff() != b.diff()) return a.diff() > b.diff();
                          if (a.points_for != b.points_for) return a.points_for > b.points_for;
                          return a.team_id < b.team_id;
                      });
        }
        begin = end;
    }
    return out;
}

const Standing& Season::standing(std::uint32_t team_id) const {
    return table_[team_index(team_id)];
}

std::vector<PlayerSeason> Season::player_stats() const {
    std::vector<PlayerSeason> out;
    out.reserve(player_stats_.size());
    for (const auto& [id, s] : player_stats_) out.push_back(s);
    std::sort(out.begin(), out.end(),
              [](const PlayerSeason& a, const PlayerSeason& b) { return a.player_id < b.player_id; });
    return out;
}

PlayerSeason Season::player_stats(std::uint32_t player_id) const {
    const auto it = player_stats_.find(player_id);
    if (it != player_stats_.end()) return it->second;
    PlayerSeason empty;
    empty.player_id = player_id;
    empty.team_id = team_of(player_id).id;
    return empty;
}

std::vector<PlayerSeason> Season::leaders(Stat stat, std::size_t count) const {
    const auto value = [stat](const PlayerSeason& s) {
        switch (stat) {
        case Stat::Points: return s.ppg();
        case Stat::Rebounds: return s.rpg();
        case Stat::Assists: return s.apg();
        case Stat::Steals: return s.spg();
        case Stat::Blocks: return s.bpg();
        case Stat::GameScore: return s.game_score();
        }
        return 0.0;
    };
    std::vector<PlayerSeason> pool;
    for (const auto& s : player_stats()) {
        const int team_games = standing(s.team_id).games();
        if (s.games * 2 >= std::max(team_games, 1)) pool.push_back(s);
    }
    std::stable_sort(pool.begin(), pool.end(), [&](const PlayerSeason& a, const PlayerSeason& b) {
        return value(a) > value(b);
    });
    if (pool.size() > count) pool.resize(count);
    return pool;
}

std::vector<MvpCandidate> Season::mvp_race(std::size_t count) const {
    // Voters reward production on winning teams: average Game Score scaled by team win%.
    std::vector<MvpCandidate> race;
    for (const auto& s : player_stats()) {
        const Standing& team_standing = standing(s.team_id);
        if (s.games * 2 < std::max(team_standing.games(), 1)) continue;
        MvpCandidate c;
        c.player_id = s.player_id;
        c.stats = s;
        c.team_pct = team_standing.pct();
        c.score = s.game_score() * (0.7 + 0.6 * c.team_pct);
        race.push_back(c);
    }
    std::stable_sort(race.begin(), race.end(),
                     [](const MvpCandidate& a, const MvpCandidate& b) { return a.score > b.score; });
    if (race.size() > count) race.resize(count);
    return race;
}

} // namespace street::hoops
