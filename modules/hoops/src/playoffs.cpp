#include <algorithm>
#include <map>
#include <stdexcept>

#include "street/hoops/season.hpp"

namespace street::hoops {
namespace {

bool is_power_of_two(int n) { return n > 0 && (n & (n - 1)) == 0; }

/// Bracket order of seeds so that 1 and 2 can only meet in the final:
/// 2 -> [1 2], 4 -> [1 4 2 3], 8 -> [1 8 4 5 2 7 3 6].
std::vector<int> bracket_order(int teams) {
    std::vector<int> order{1};
    for (int size = 2; size <= teams; size *= 2) {
        std::vector<int> next;
        for (int seed : order) {
            next.push_back(seed);
            next.push_back(size + 1 - seed);
        }
        order = std::move(next);
    }
    return order;
}

} // namespace

int PlayoffResult::rounds() const noexcept {
    int r = 0;
    for (const auto& s : series) r = std::max(r, s.round + 1);
    return r;
}

std::vector<const Series*> PlayoffResult::round(int index) const {
    std::vector<const Series*> out;
    for (const auto& s : series)
        if (s.round == index) out.push_back(&s);
    return out;
}

const Series& PlayoffResult::finals() const {
    if (series.empty()) throw std::logic_error("PlayoffResult::finals: no series played");
    return series.back();
}

Series Season::play_series(int round, std::uint32_t high_id, int high_seed, std::uint32_t low_id,
                           int low_seed, const GameOptions& options) {
    Series s;
    s.round = round;
    s.high_id = high_id;
    s.low_id = low_id;
    s.high_seed = high_seed;
    s.low_seed = low_seed;
    // 2-2-1-1-1: the higher seed hosts games 1, 2, 5 and 7.
    constexpr bool kHighHosts[7] = {true, true, false, false, true, false, true};
    for (int game = 0; s.high_wins < 4 && s.low_wins < 4; ++game) {
        const bool high_home = kHighHosts[game];
        GameResult r = high_home ? play_game(high_id, low_id, options)
                                 : play_game(low_id, high_id, options);
        (r.winner_id() == high_id ? s.high_wins : s.low_wins) += 1;
        s.games.push_back(std::move(r));
    }
    return s;
}

PlayoffResult Season::play_playoffs(int teams, const GameOptions& options) {
    if (playoffs_) return *playoffs_;
    if (!is_power_of_two(teams) || teams < 2 || teams > static_cast<int>(teams_.size()))
        throw std::invalid_argument("play_playoffs: the field must be a power of two between 2 and " +
                                    std::to_string(teams_.size()) + " teams");
    if (!regular_season_done()) play_regular_season();

    const std::vector<Standing> table = standings();
    struct Entry {
        std::uint32_t id;
        int seed;
    };
    std::vector<Entry> alive;
    for (int seed : bracket_order(teams))
        alive.push_back({table[static_cast<std::size_t>(seed - 1)].team_id, seed});

    PlayoffResult result;
    result.teams = teams;
    for (int round = 0; alive.size() > 1; ++round) {
        std::vector<Entry> winners;
        for (std::size_t i = 0; i + 1 < alive.size(); i += 2) {
            const Entry& a = alive[i];
            const Entry& b = alive[i + 1];
            const Entry& high = a.seed < b.seed ? a : b;
            const Entry& low = a.seed < b.seed ? b : a;
            Series s = play_series(round, high.id, high.seed, low.id, low.seed, options);
            winners.push_back({s.winner_id(), s.winner_seed()});
            result.series.push_back(std::move(s));
        }
        alive = std::move(winners);
    }
    result.champion_id = alive.front().id;

    // Finals MVP: the champion's best total Game Score across the finals.
    std::map<std::uint32_t, double> finals_score;
    for (const GameResult& g : result.finals().games) {
        const int side = g.home_id == result.champion_id ? kHome : kAway;
        for (const PlayerLine& l : g.box[static_cast<std::size_t>(side)])
            finals_score[l.player_id] += l.game_score();
    }
    const auto best = std::max_element(finals_score.begin(), finals_score.end(),
                                       [](const auto& a, const auto& b) { return a.second < b.second; });
    result.finals_mvp_id = best != finals_score.end() ? best->first : 0;

    playoffs_ = result;
    return result;
}

} // namespace street::hoops
