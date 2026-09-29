#pragma once
// The Universe: one basketball season, one sneaker exchange and one studio,
// running on the same clock and reacting to each other through an EventBus.
//
//   hoops::Season ──GameFinal──▶ HypeDesk ──shock()──▶ kicks::Market
//        │                          └──BigNight / HypeMove──▶ feed
//        └──PlayerOfTheWeek──▶ Booth  ──ghostwrite + analyze──▶ VerseWritten
//                          └─▶ Studio ──generate + WAV──────▶ AnthemDropped
//
// Each game day plays every scheduled game, publishes the results, then runs one
// trading day on the exchange, so a monster night shows up in that day's candle.

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "street/bars/bars.hpp"
#include "street/core/event_bus.hpp"
#include "street/culture/events.hpp"
#include "street/culture/signature.hpp"
#include "street/hoops/season.hpp"
#include "street/kicks/market.hpp"

namespace street::culture {

struct Config {
    std::uint64_t seed = 1;
    int weeks = 0;                 ///< culture weeks to play; 0 = the whole regular season
    int days_per_week = 3;         ///< game days per culture week
    std::size_t signatures = 6;    ///< players who get a signature shoe
    bars::Language language = bars::Language::English; ///< for ghostwritten verses
    bool anthems = true;           ///< synthesize an anthem for each Player of the Week
    int anthem_bars = 8;
    std::optional<std::filesystem::path> mixtape; ///< folder for anthem WAV files
    int playoff_teams = 8;
};

/// Everything that happened in one culture week.
struct WeekReport {
    int week = 0;
    int first_day = 0, last_day = 0; ///< 1-based game days
    std::vector<hoops::GameResult> games;
    std::vector<BigNight> big_nights;
    std::vector<HypeMove> hype_moves;
    std::optional<PlayerOfTheWeek> player_of_the_week;
    std::optional<VerseWritten> verse;
    std::optional<AnthemDropped> anthem;
};

/// How a signature shoe did over the season, and how tightly it tracked its athlete.
struct SignatureReport {
    SignatureShoe shoe;
    kicks::Price open = 0;   ///< first close after the drop
    kicks::Price close = 0;  ///< last close
    kicks::Price high = 0;
    double change = 0.0;     ///< close / open - 1
    double premium = 0.0;    ///< close / retail - 1
    std::vector<double> closes;
    /// Pearson r between the athlete's Game Score and the shoe's same-day return,
    /// over the days they played. 0 when there is not enough data.
    double correlation = 0.0;
    int games = 0;
};

struct Recap {
    int weeks_played = 0;
    int days_played = 0;
    bool season_complete = false;
    std::optional<hoops::PlayoffResult> playoffs;
    std::vector<SignatureReport> signatures; ///< sorted by season change, best first
    std::vector<AnthemDropped> mixtape;
};

class Universe {
public:
    explicit Universe(Config config);
    ~Universe();
    Universe(const Universe&) = delete;
    Universe& operator=(const Universe&) = delete;

    /// Plays the next culture week (up to days_per_week game days) and returns what happened.
    /// Throws std::logic_error when done().
    WeekReport play_week();
    /// True once the configured number of weeks or the regular season is exhausted.
    bool done() const noexcept;
    int week() const noexcept { return week_; }
    /// Weeks this universe will play in total (respects Config::weeks).
    int total_weeks() const noexcept;

    /// Wraps up: runs the playoffs if the regular season is complete and builds the recap.
    Recap finish();

    const Config& config() const noexcept { return config_; }
    const hoops::Season& season() const noexcept { return season_; }
    const kicks::Market& market() const noexcept { return market_; }
    const std::vector<SignatureShoe>& signatures() const noexcept { return signatures_; }
    const SignatureShoe* signature_of(std::uint32_t player_id) const noexcept;
    /// Subscribe here to observe the universe live (GameFinal, BigNight, HypeMove, ...).
    EventBus& bus() noexcept { return bus_; }

private:
    struct Bridges;
    void play_day(WeekReport& report);
    std::optional<PlayerOfTheWeek> pick_player_of_the_week(const WeekReport& report) const;

    Config config_;
    hoops::Season season_;
    kicks::Market market_;
    EventBus bus_;
    std::vector<SignatureShoe> signatures_;
    std::unique_ptr<Bridges> bridges_;
    std::vector<AnthemDropped> mixtape_;
    int week_ = 0;
    // Per signature athlete, the (Game Score, same-day return) pairs behind the correlation,
    // and tonight's Game Score while a day is being played.
    std::vector<std::vector<std::pair<double, double>>> performance_log_;
    std::vector<std::optional<double>> todays_game_score_;
    WeekReport* current_ = nullptr; // report being filled while a week is played
};

} // namespace street::culture
