#include <doctest/doctest.h>

#include <algorithm>
#include <ostream> // lets doctest print std::string_view operands
#include <map>
#include <numeric>
#include <set>
#include <stdexcept>

#include "street/hoops/league.hpp"
#include "street/hoops/monte_carlo.hpp"
#include "street/hoops/season.hpp"

using namespace street::hoops;

namespace {

/// A four-team league keeps Monte Carlo tests fast even under sanitizers.
std::vector<Team> mini_league() {
    auto league = default_league();
    league.resize(4);
    return league;
}

} // namespace

TEST_CASE("the schedule is a balanced double round robin") {
    const Season season(default_league(), 1);
    REQUIRE(season.total_days() == 22);
    CHECK(season.games_per_team() == 22);
    std::map<std::pair<std::uint32_t, std::uint32_t>, int> meetings; // (home, away) -> count
    std::map<std::uint32_t, int> home_games;
    for (int d = 0; d < season.total_days(); ++d) {
        std::set<std::uint32_t> busy;
        CHECK(season.schedule(d).size() == 6);
        for (const Matchup& m : season.schedule(d)) {
            CHECK(busy.insert(m.home_id).second); // nobody plays twice in a day
            CHECK(busy.insert(m.away_id).second);
            ++meetings[{m.home_id, m.away_id}];
            ++home_games[m.home_id];
        }
    }
    CHECK(meetings.size() == 132); // every ordered pair exactly once: home and away
    for (const auto& [pair, count] : meetings) CHECK(count == 1);
    for (const auto& [team, count] : home_games) CHECK(count == 11);
    CHECK_THROWS_AS((void)season.schedule(22), std::out_of_range);
}

TEST_CASE("odd leagues get byes and single round robins work") {
    auto league = default_league();
    league.resize(5);
    Season season(league, 3, 1);
    CHECK(season.total_days() == 5);
    CHECK(season.games_per_team() == 4);
    season.play_regular_season();
    for (const Standing& s : season.standings()) CHECK(s.games() == 4);
}

TEST_CASE("playing day by day fills the standings consistently") {
    Season season(default_league(), 42);
    int days = 0;
    while (!season.regular_season_done()) {
        const auto results = season.play_next_day();
        CHECK(results.size() == 6);
        ++days;
        CHECK(season.day() == days);
    }
    CHECK(season.play_next_day().empty());
    CHECK(days == 22);

    const auto table = season.standings();
    int wins = 0, losses = 0, pf = 0, pa = 0;
    double elo = 0.0;
    for (std::size_t i = 0; i < table.size(); ++i) {
        const Standing& s = table[i];
        CHECK(s.games() == 22);
        CHECK(s.home_wins + s.home_losses == 11);
        CHECK(s.last10.size() == 10);
        CHECK((s.streak[0] == 'W' || s.streak[0] == 'L'));
        wins += s.wins;
        losses += s.losses;
        pf += s.points_for;
        pa += s.points_against;
        elo += s.elo;
        if (i > 0) CHECK(table[i - 1].pct() >= s.pct());
    }
    CHECK(wins == losses);
    CHECK(pf == pa);
    CHECK(elo == doctest::Approx(1500.0 * 12)); // Elo is zero-sum
    CHECK(table.front().elo > table.back().elo);
    CHECK(season.games_played() == 132);
}

TEST_CASE("a season replays identically, however it is driven") {
    Season a(default_league(), 777);
    Season b(default_league(), 777);
    a.play_regular_season();
    GameOptions full; // with play-by-play and shots: must not change a single result
    while (!b.regular_season_done()) b.play_next_day(full);
    const auto ta = a.standings(), tb = b.standings();
    for (std::size_t i = 0; i < ta.size(); ++i) {
        CHECK(ta[i].team_id == tb[i].team_id);
        CHECK(ta[i].wins == tb[i].wins);
        CHECK(ta[i].points_for == tb[i].points_for);
        CHECK(ta[i].elo == tb[i].elo);
    }
    Season c(default_league(), 778);
    c.play_regular_season();
    bool differs = false;
    for (std::size_t i = 0; i < ta.size(); ++i)
        differs |= ta[i].points_for != c.standing(ta[i].team_id).points_for;
    CHECK(differs);
}

TEST_CASE("playoffs: seeding, best-of-seven, home court and a finals MVP") {
    Season season(default_league(), 2025);
    const PlayoffResult po = season.play_playoffs(8); // finishes the regular season itself
    CHECK(season.regular_season_done());
    REQUIRE(po.series.size() == 7);
    CHECK(po.rounds() == 3);
    CHECK(po.round(0).size() == 4);
    CHECK(po.round(2).size() == 1);

    const auto table = season.standings();
    std::set<std::uint32_t> seeded;
    for (int i = 0; i < 8; ++i) seeded.insert(table[static_cast<std::size_t>(i)].team_id);
    for (const Series* s : po.round(0)) {
        CHECK(seeded.count(s->high_id) == 1);
        CHECK(seeded.count(s->low_id) == 1);
        CHECK(s->high_seed + s->low_seed == 9); // 1v8, 4v5, 2v7, 3v6
    }
    for (const Series& s : po.series) {
        CHECK(std::max(s.high_wins, s.low_wins) == 4);
        CHECK(s.games.size() >= 4);
        CHECK(s.games.size() <= 7);
        CHECK(s.high_seed < s.low_seed);
        constexpr bool kHighHosts[7] = {true, true, false, false, true, false, true};
        for (std::size_t g = 0; g < s.games.size(); ++g)
            CHECK((s.games[g].home_id == s.high_id) == kHighHosts[g]);
    }
    CHECK(po.champion_id == po.finals().winner_id());
    CHECK(season.team_of(po.finals_mvp_id).id == po.champion_id);

    const PlayoffResult again = season.play_playoffs(8);
    CHECK(again.champion_id == po.champion_id);
    REQUIRE(season.playoffs().has_value());

    Season bad(default_league(), 1);
    CHECK_THROWS_AS(bad.play_playoffs(6), std::invalid_argument);
    CHECK_THROWS_AS(bad.play_playoffs(16), std::invalid_argument);
    CHECK_THROWS_AS(bad.play_playoffs(1), std::invalid_argument);
}

TEST_CASE("player aggregates, leaders and the MVP race") {
    Season season(default_league(), 9);
    season.play_regular_season();
    const auto stats = season.player_stats();
    CHECK(stats.size() >= 100);
    for (const PlayerSeason& s : stats) {
        CHECK(s.games <= 22);
        CHECK(s.fgm <= s.fga);
        CHECK(season.team_of(s.player_id).id == s.team_id);
    }
    const auto scorers = season.leaders(Stat::Points, 5);
    REQUIRE(scorers.size() == 5);
    for (std::size_t i = 1; i < scorers.size(); ++i) CHECK(scorers[i - 1].ppg() >= scorers[i].ppg());
    CHECK(scorers.front().ppg() > 18.0);
    const auto race = season.mvp_race(5);
    REQUIRE(race.size() == 5);
    for (std::size_t i = 1; i < race.size(); ++i) CHECK(race[i - 1].score >= race[i].score);

    const Player& p = season.player(101);
    CHECK(p.name == "Darius Greene");
    CHECK(season.team_of(101).abbrev == "QNS");
    CHECK(season.team(5).abbrev == "CPT");
    CHECK(season.player_stats(101).player_id == 101);
    CHECK_THROWS_AS((void)season.player(42), std::out_of_range);
    CHECK_THROWS_AS((void)season.team(99), std::out_of_range);
}

TEST_CASE("season construction validates its input") {
    auto league = default_league();
    CHECK_THROWS_AS(Season(std::vector<Team>(league.begin(), league.begin() + 1), 1), std::invalid_argument);
    CHECK_THROWS_AS(Season(league, 1, 0), std::invalid_argument);
    league[1].id = league[0].id;
    CHECK_THROWS_AS(Season(league, 1), std::invalid_argument);
}

TEST_CASE("title odds are identical for any thread count") {
    const auto league = mini_league();
    TitleOddsOptions options;
    options.playoff_teams = 4;
    constexpr int kSeasons = 3 * kSeasonsPerChunk - 3; // three chunks, the last one partial
    const auto one = simulate_title_odds(league, kSeasons, 55, 1, options);
    const auto two = simulate_title_odds(league, kSeasons, 55, 2, options);
    const auto many = simulate_title_odds(league, kSeasons, 55, 7, options);
    REQUIRE(one.odds.size() == 4);
    for (std::size_t i = 0; i < one.odds.size(); ++i) {
        CHECK(one.odds[i].team_id == two.odds[i].team_id);
        CHECK(one.odds[i].team_id == many.odds[i].team_id);
        CHECK(one.odds[i].title_probability == two.odds[i].title_probability);
        CHECK(one.odds[i].title_probability == many.odds[i].title_probability);
        CHECK(one.odds[i].average_wins == many.odds[i].average_wins);
    }
    CHECK(one.games == many.games);
    CHECK(many.threads == 3); // never more workers than chunks

    double titles = 0, finals = 0, playoffs = 0, wins = 0;
    for (const TitleOdds& o : one.odds) {
        titles += o.title_probability;
        finals += o.finals_probability;
        playoffs += o.playoff_probability;
        wins += o.average_wins;
        CHECK(o.title_probability <= o.finals_probability);
        CHECK(o.finals_probability <= o.playoff_probability);
    }
    CHECK(titles == doctest::Approx(1.0));
    CHECK(finals == doctest::Approx(2.0));
    CHECK(playoffs == doctest::Approx(4.0));
    CHECK(wins == doctest::Approx(12.0)); // 4 teams x 6 games / 2
    for (std::size_t i = 1; i < one.odds.size(); ++i)
        CHECK(one.odds[i - 1].title_probability >= one.odds[i].title_probability);

    const auto different_seed = simulate_title_odds(league, kSeasons, 56, 2, options).odds;
    bool differs = false;
    for (std::size_t i = 0; i < 4; ++i) differs |= different_seed[i].average_wins != one.odds[i].average_wins;
    CHECK(differs);
}

TEST_CASE("title odds report progress and reject bad input") {
    const auto league = mini_league();
    TitleOddsOptions options;
    options.playoff_teams = 2;
    int last_done = -1, last_total = -1;
    options.progress = [&](int done, int total) {
        CHECK(done >= last_done);
        last_done = done;
        last_total = total;
    };
    const auto report = simulate_title_odds(league, 5, 1, 2, options);
    CHECK(last_done == 5);
    CHECK(last_total == 5);
    CHECK(report.seasons == 5);
    CHECK(report.seconds >= 0.0);

    CHECK_THROWS_AS(title_odds(league, 0, 1), std::invalid_argument);
    options.playoff_teams = 3;
    CHECK_THROWS_AS(simulate_title_odds(league, 5, 1, 1, options), std::invalid_argument);
}
