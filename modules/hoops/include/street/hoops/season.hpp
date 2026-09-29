#pragma once
// A league season: round-robin schedule, standings with tiebreakers, Elo
// ratings, player aggregates, an MVP race and a best-of-seven playoff bracket.
//
// Every game draws from its own generator derived from (season seed, game
// number), so a season replays identically whatever GameOptions are used and
// whichever way the caller drives it (day by day or all at once).

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "street/hoops/game.hpp"
#include "street/hoops/model.hpp"

namespace street::hoops {

struct Standing {
    std::uint32_t team_id = 0;
    int wins = 0, losses = 0;
    int points_for = 0, points_against = 0;
    int home_wins = 0, home_losses = 0;
    double elo = 1500.0;
    std::string streak = "-"; ///< "W3", "L1", or "-" before the first game
    std::string last10;       ///< most recent results, oldest first: "WWLWL..."

    int games() const noexcept { return wins + losses; }
    double pct() const noexcept;
    int diff() const noexcept { return points_for - points_against; }
};

struct Matchup {
    std::uint32_t home_id = 0, away_id = 0;
};

/// A best-of-seven series. The higher seed hosts games 1, 2, 5 and 7 (2-2-1-1-1).
struct Series {
    int round = 0; ///< 0 = first round; the last round is the finals
    std::uint32_t high_id = 0, low_id = 0;
    int high_seed = 0, low_seed = 0;
    int high_wins = 0, low_wins = 0;
    std::vector<GameResult> games;

    std::uint32_t winner_id() const noexcept { return high_wins > low_wins ? high_id : low_id; }
    std::uint32_t loser_id() const noexcept { return high_wins > low_wins ? low_id : high_id; }
    int winner_seed() const noexcept { return high_wins > low_wins ? high_seed : low_seed; }
};

struct PlayoffResult {
    int teams = 0;
    std::vector<Series> series; ///< round by round, bracket order within a round
    std::uint32_t champion_id = 0;
    std::uint32_t finals_mvp_id = 0; ///< best total Game Score on the champion in the finals

    int rounds() const noexcept;
    std::vector<const Series*> round(int index) const;
    const Series& finals() const; ///< throws std::logic_error when empty
};

/// Regular-season totals for one player.
struct PlayerSeason {
    std::uint32_t player_id = 0, team_id = 0;
    int games = 0;
    double minutes = 0.0;
    int pts = 0, fgm = 0, fga = 0, tpm = 0, tpa = 0, ftm = 0, fta = 0;
    int reb = 0, ast = 0, stl = 0, blk = 0, tov = 0;
    double game_score_total = 0.0;
    int best_game = 0; ///< season-high points

    double ppg() const noexcept { return per_game(pts); }
    double rpg() const noexcept { return per_game(reb); }
    double apg() const noexcept { return per_game(ast); }
    double spg() const noexcept { return per_game(stl); }
    double bpg() const noexcept { return per_game(blk); }
    double mpg() const noexcept { return games > 0 ? minutes / games : 0.0; }
    double game_score() const noexcept { return games > 0 ? game_score_total / games : 0.0; }
    double fg_pct() const noexcept { return fga > 0 ? static_cast<double>(fgm) / fga : 0.0; }
    double tp_pct() const noexcept { return tpa > 0 ? static_cast<double>(tpm) / tpa : 0.0; }
    double ft_pct() const noexcept { return fta > 0 ? static_cast<double>(ftm) / fta : 0.0; }
    /// True shooting: points per shooting possession, 0.44 FTA per possession.
    double ts_pct() const noexcept;

private:
    double per_game(int total) const noexcept {
        return games > 0 ? static_cast<double>(total) / games : 0.0;
    }
};

enum class Stat { Points, Rebounds, Assists, Steals, Blocks, GameScore };

struct MvpCandidate {
    std::uint32_t player_id = 0;
    double score = 0.0; ///< Game Score weighted by team success
    PlayerSeason stats;
    double team_pct = 0.0;
};

class Season {
public:
    /// rounds = how many times each pair of teams meets (home and away alternate).
    /// Throws std::invalid_argument for fewer than two teams, duplicate ids or rounds < 1.
    Season(std::vector<Team> teams, std::uint64_t seed, int rounds = 2);

    /// Plays one game day (every team plays at most once) and returns its results;
    /// empty once the regular season is over.
    std::vector<GameResult> play_next_day(const GameOptions& options = GameOptions::fast());
    void play_regular_season();
    bool regular_season_done() const noexcept { return day_ >= total_days(); }
    int day() const noexcept { return day_; } ///< game days played so far
    int total_days() const noexcept { return static_cast<int>(schedule_.size()); }
    int games_per_team() const noexcept;
    /// Games on a (0-based) day of the schedule. Throws std::out_of_range.
    const std::vector<Matchup>& schedule(int day_index) const;

    /// Sorted by win%, then head-to-head among tied teams, point differential, points scored.
    std::vector<Standing> standings() const;
    const Standing& standing(std::uint32_t team_id) const;

    /// Regular-season aggregates of every player who has played, by player id.
    std::vector<PlayerSeason> player_stats() const;
    PlayerSeason player_stats(std::uint32_t player_id) const; ///< zeros if not played yet
    /// Per-game leaders among players with at least half of their team's games.
    std::vector<PlayerSeason> leaders(Stat stat, std::size_t count = 5) const;
    std::vector<MvpCandidate> mvp_race(std::size_t count = 5) const;

    /// Seeds the top `teams` of the standings (power of two, 2..league size) into a
    /// best-of-seven bracket and plays it out. Finishes the regular season first if
    /// needed. Playing the playoffs twice returns the stored result.
    PlayoffResult play_playoffs(int teams = 8, const GameOptions& options = GameOptions::fast());
    const std::optional<PlayoffResult>& playoffs() const noexcept { return playoffs_; }

    const std::vector<Team>& teams() const noexcept { return teams_; }
    const Team& team(std::uint32_t id) const;             ///< throws std::out_of_range
    const Player& player(std::uint32_t id) const;         ///< throws std::out_of_range
    const Team& team_of(std::uint32_t player_id) const;   ///< throws std::out_of_range
    std::uint64_t seed() const noexcept { return seed_; }
    std::uint64_t games_played() const noexcept { return games_played_; } ///< incl. playoffs

private:
    std::size_t team_index(std::uint32_t id) const;
    GameResult play_game(std::uint32_t home_id, std::uint32_t away_id, const GameOptions& options);
    void record_regular_season(const GameResult& result);
    void update_elo(const GameResult& result);
    Series play_series(int round, std::uint32_t high_id, int high_seed, std::uint32_t low_id,
                       int low_seed, const GameOptions& options);

    std::vector<Team> teams_;
    std::uint64_t seed_ = 0;
    std::vector<std::vector<Matchup>> schedule_;
    int day_ = 0;
    std::uint64_t games_played_ = 0;
    std::vector<Standing> table_;                   // parallel to teams_
    std::vector<std::vector<int>> head_to_head_;    // [winner index][loser index] = wins
    std::unordered_map<std::uint32_t, std::size_t> team_by_id_;
    std::unordered_map<std::uint32_t, std::size_t> team_by_player_;
    std::unordered_map<std::uint32_t, PlayerSeason> player_stats_;
    std::optional<PlayoffResult> playoffs_;
};

} // namespace street::hoops
