#include <doctest/doctest.h>

#include <string>
#include <ostream> // lets doctest print std::string_view operands

#include "street/core/term.hpp"
#include "street/core/text.hpp"
#include "street/hoops/hoops.hpp"

using namespace street::hoops;
namespace term = street::term;

namespace {

/// Restores the global color switch when a test ends.
struct ColorScope {
    explicit ColorScope(bool enabled) : previous(term::color_enabled()) { term::set_color_enabled(enabled); }
    ~ColorScope() { term::set_color_enabled(previous); }
    ColorScope(const ColorScope&) = delete;
    ColorScope& operator=(const ColorScope&) = delete;
    bool previous;
};

std::size_t widest_line(const std::string& text) {
    std::size_t widest = 0;
    for (const auto& line : street::text::split_lines(text)) widest = std::max(widest, term::width(line));
    return widest;
}

bool has_escape(const std::string& s) { return s.find('\x1b') != std::string::npos; }

struct Fixture {
    std::vector<Team> league = default_league();
    GameResult game;
    Fixture() {
        street::Rng rng(4242);
        game = simulate_game(league[0], league[1], rng);
    }
};

} // namespace

TEST_CASE("game renderers: plain text is clean, aligned and under 100 columns") {
    const ColorScope plain(false);
    const Fixture f;
    const Team& home = f.league[0];
    const Team& away = f.league[1];

    const std::string box = render_box_score(f.game, home, away);
    CHECK_FALSE(has_escape(box));
    CHECK(box.find("QUEENS BRIDGE POETS") != std::string::npos);
    CHECK(box.find("BRONX BREAKBEATS") != std::string::npos);
    CHECK(box.find("TOTALS") != std::string::npos);
    CHECK(box.find("top performer") != std::string::npos);
    CHECK(widest_line(box) <= 100);

    const std::string board = render_scoreboard(f.game, home, away);
    CHECK(board.find("FINAL") != std::string::npos);
    CHECK(board.find(std::to_string(f.game.home_score)) != std::string::npos);
    CHECK(widest_line(board) <= 100);

    const std::string chart = render_shot_chart(f.game, kHome, home);
    CHECK_FALSE(has_escape(chart));
    CHECK(chart.find("RIM") != std::string::npos);
    CHECK(chart.find("3PT") != std::string::npos);
    CHECK(chart.find("●") != std::string::npos);
    CHECK(street::text::split_lines(chart).size() >= 26); // 24 court rows + borders
    CHECK(widest_line(chart) <= 100);

    for (const Play& p : f.game.plays) {
        const std::string line = render_play(p, home, away);
        const bool starts_with_clock = line.starts_with("Q") || line.starts_with("OT");
        CHECK(starts_with_clock);
        CHECK(widest_line(line) <= 100);
    }
}

TEST_CASE("colors only decorate: stripping them gives the plain rendering") {
    const Fixture f;
    std::string plain_box, plain_chart, colored_box, colored_chart;
    {
        const ColorScope plain(false);
        plain_box = render_box_score(f.game, f.league[0], f.league[1]);
        plain_chart = render_shot_chart(f.game, kAway, f.league[1]);
    }
    {
        const ColorScope colored(true);
        colored_box = render_box_score(f.game, f.league[0], f.league[1]);
        colored_chart = render_shot_chart(f.game, kAway, f.league[1]);
    }
    CHECK(has_escape(colored_box));
    CHECK(term::strip_ansi(colored_box) == plain_box);
    CHECK(term::strip_ansi(colored_chart) == plain_chart);
}

TEST_CASE("season renderers") {
    const ColorScope plain(false);
    Season season(default_league(), 31);
    season.play_regular_season();
    const PlayoffResult playoffs = season.play_playoffs(8);

    const std::string standings = render_standings(season, 8);
    for (const Team& t : season.teams()) CHECK(standings.find(t.abbrev) != std::string::npos);
    CHECK(standings.find("PCT") != std::string::npos);
    CHECK(standings.find("ELO") != std::string::npos);
    CHECK(widest_line(standings) <= 100);

    const std::string leaders = render_leaders(season);
    CHECK(leaders.find("POINTS") != std::string::npos);
    CHECK(leaders.find("ASSISTS") != std::string::npos);
    CHECK(widest_line(leaders) <= 100);

    const std::string mvp = render_mvp_race(season);
    CHECK(mvp.find("MVP RACE") != std::string::npos);
    CHECK(widest_line(mvp) <= 100);

    const std::string bracket = render_bracket(playoffs, season.teams());
    CHECK(bracket.find("CHAMPION") != std::string::npos);
    CHECK(bracket.find(season.team(playoffs.champion_id).full_name()) != std::string::npos);
    CHECK(bracket.find("(1)") != std::string::npos);
    CHECK(bracket.find("(8)") != std::string::npos);
    CHECK(widest_line(bracket) <= 100);

    std::vector<TitleOdds> odds;
    for (const Team& t : season.teams()) odds.push_back({t.id, 1.0 / 12, 2.0 / 12, 8.0 / 12, 11.0});
    const std::string table = render_title_odds(odds, season.teams());
    CHECK(table.find("8.3%") != std::string::npos);
    CHECK(widest_line(table) <= 100);
}

TEST_CASE("league renderers") {
    const ColorScope plain(false);
    const auto league = default_league();
    const std::string all = render_league(league);
    for (const Team& t : league) CHECK(all.find(t.full_name()) != std::string::npos);
    CHECK(widest_line(all) <= 100);
    for (const Team& t : league) {
        const std::string roster = render_roster(t);
        CHECK(roster.find(t.roster.front().display_name()) != std::string::npos);
        CHECK(roster.find("CLU") != std::string::npos);
        CHECK(widest_line(roster) <= 100);
    }
}
