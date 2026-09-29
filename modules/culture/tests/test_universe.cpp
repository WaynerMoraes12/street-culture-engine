#include <doctest/doctest.h>

#include <filesystem>
#include <set>

#include "street/core/term.hpp"
#include "street/core/text.hpp"
#include "street/culture/render.hpp"
#include "street/culture/universe.hpp"
#include "street/hoops/league.hpp"

using namespace street;
using namespace street::culture;

namespace {

Config quick(std::uint64_t seed, int weeks) {
    Config c;
    c.seed = seed;
    c.weeks = weeks;
    c.anthems = false; // audio synthesis is covered by the beats tests; keep these fast
    return c;
}

hoops::PlayerLine line_with(int pts, int reb, int ast, int fgm, int fga) {
    hoops::PlayerLine l;
    l.minutes = 36;
    l.pts = pts;
    l.dreb = reb;
    l.ast = ast;
    l.fgm = fgm;
    l.fga = fga;
    return l;
}

} // namespace

TEST_CASE("signature shoes go to the league's stars with unique SKUs") {
    const auto league = hoops::default_league();
    const auto athletes = signature_athletes(league, 6);
    REQUIRE(athletes.size() == 6);
    CHECK(athletes.front()->star);

    std::vector<std::string> taken;
    std::set<std::string> skus;
    for (const auto* player : athletes) {
        const hoops::Team* team = nullptr;
        for (const auto& t : league)
            if (t.find_player(player->id)) team = &t;
        REQUIRE(team != nullptr);
        const auto shoe = design_signature(*player, *team, taken);
        taken.push_back(shoe.sneaker.sku);
        CHECK(shoe.sneaker.sku.starts_with("SIG-"));
        CHECK(shoe.sneaker.retail >= kicks::dollars(120));
        CHECK(shoe.sneaker.retail <= kicks::dollars(200));
        CHECK(shoe.sneaker.retail % 500 == 0);
        CHECK(shoe.sneaker.pairs > 0);
        CHECK(shoe.sneaker.base_hype > 0.0);
        CHECK(shoe.sneaker.base_hype < 1.0);
        skus.insert(shoe.sneaker.sku);
    }
    CHECK(skus.size() == 6);

    const auto again = design_signature(*athletes[0], league[0], {taken[0]});
    CHECK(again.sneaker.sku != taken[0]); // collisions get a suffix
}

TEST_CASE("the market reacts to surprise, not to reputation") {
    const auto monster = performance_impulse(line_with(52, 9, 7, 19, 30), 20.0, true, "Flash", "Flash 1");
    REQUIRE(monster);
    CHECK(monster->magnitude > 0.25);
    CHECK(monster->headline.find("52") != std::string::npos);

    const auto dud = performance_impulse(line_with(6, 2, 1, 2, 14), 20.0, false, "Flash", "Flash 1");
    REQUIRE(dud);
    CHECK(dud->magnitude < 0.0);
    CHECK(dud->magnitude >= -0.08);

    // An ordinary night at the player's usual level is priced in.
    auto usual = line_with(22, 5, 4, 8, 17);
    const auto priced_in = performance_impulse(usual, usual.game_score(), true, "Flash", "Flash 1");
    CHECK_FALSE(priced_in);

    CHECK_FALSE(performance_impulse(hoops::PlayerLine{}, 10.0, true, "DNP", "x"));
}

TEST_CASE("big nights are recognised") {
    CHECK(is_big_night(line_with(38, 4, 3, 13, 24)));
    CHECK(is_big_night(line_with(15, 11, 12, 6, 12))); // triple-double
    CHECK_FALSE(is_big_night(line_with(18, 5, 5, 7, 15)));
}

TEST_CASE("opening night lists every signature shoe on the exchange") {
    Universe universe(quick(3, 1));
    REQUIRE(universe.signatures().size() == 6);
    for (const auto& shoe : universe.signatures()) {
        CHECK(universe.market().has(shoe.sneaker.sku));
        CHECK(universe.signature_of(shoe.player_id) == &shoe);
    }
    CHECK(universe.signature_of(999999) == nullptr);
    CHECK(universe.total_weeks() == 1);
}

TEST_CASE("a week plays games, trades and crowns a Player of the Week") {
    Universe universe(quick(11, 2));
    const auto week = universe.play_week();
    CHECK(week.week == 1);
    CHECK(week.first_day == 1);
    CHECK(week.last_day == 3);
    CHECK(week.games.size() == 18); // 12 teams, 6 games per day
    CHECK(universe.market().day() == 3);
    REQUIRE(week.player_of_the_week);
    CHECK(week.player_of_the_week->games >= 2);
    REQUIRE(week.verse);
    CHECK_FALSE(week.verse->text.empty());
    CHECK_FALSE(week.anthem); // anthems disabled in this config
    CHECK_FALSE(universe.done());
    universe.play_week();
    CHECK(universe.done());
    CHECK_THROWS_AS(universe.play_week(), std::logic_error);
}

TEST_CASE("the same seed replays the same universe") {
    Universe a(quick(21, 3)), b(quick(21, 3));
    while (!a.done()) {
        const auto wa = a.play_week();
        const auto wb = b.play_week();
        REQUIRE(wa.player_of_the_week);
        CHECK(wa.player_of_the_week->player_id == wb.player_of_the_week->player_id);
        CHECK(wa.hype_moves.size() == wb.hype_moves.size());
        CHECK(wa.verse->text == wb.verse->text);
    }
    for (const auto& shoe : a.signatures())
        CHECK(a.market().last_price(shoe.sneaker.sku) == b.market().last_price(shoe.sneaker.sku));
}

TEST_CASE("events are observable from outside through the bus") {
    Universe universe(quick(5, 0));
    int finals = 0, moves = 0, big = 0;
    universe.bus().subscribe<GameFinal>([&](const GameFinal&) { ++finals; });
    universe.bus().subscribe<HypeMove>([&](const HypeMove&) { ++moves; });
    universe.bus().subscribe<BigNight>([&](const BigNight&) { ++big; });
    while (!universe.done()) universe.play_week();
    CHECK(finals == 132); // 12 teams × 22 games / 2
    CHECK(moves > 0);
    CHECK(big > 0);

    const Recap recap = universe.finish();
    CHECK(recap.season_complete);
    REQUIRE(recap.playoffs);
    CHECK(universe.season().team(recap.playoffs->champion_id).roster.size() == 10);
    REQUIRE(recap.signatures.size() == 6);
    for (const auto& s : recap.signatures) {
        CHECK(s.correlation >= -1.0);
        CHECK(s.correlation <= 1.0);
        CHECK(s.close > 0);
        CHECK(s.closes.size() == static_cast<std::size_t>(recap.days_played));
    }
    for (std::size_t i = 1; i < recap.signatures.size(); ++i)
        CHECK(recap.signatures[i - 1].change >= recap.signatures[i].change);
}

TEST_CASE("anthems land in the mixtape folder") {
    const auto dir = std::filesystem::temp_directory_path() / "street-mixtape-test";
    std::filesystem::remove_all(dir);
    Config config = quick(8, 1);
    config.anthems = true;
    config.anthem_bars = 1;
    config.mixtape = dir;
    Universe universe(config);
    const auto week = universe.play_week();
    REQUIRE(week.anthem);
    REQUIRE(week.anthem->file);
    CHECK(std::filesystem::exists(*week.anthem->file));
    CHECK(std::filesystem::file_size(*week.anthem->file) > 44);
    CHECK(week.anthem->bpm > 0);
    CHECK(universe.finish().mixtape.size() == 1);
    std::filesystem::remove_all(dir);
}

TEST_CASE("renderers produce aligned plain text") {
    term::set_color_enabled(false);
    Universe universe(quick(2, 1));
    const std::string opening = render_opening(universe);
    CHECK(opening.find("SIGNATURE DROPS") != std::string::npos);
    const auto week = universe.play_week();
    const std::string feed = render_week(week, universe);
    CHECK(feed.find("WEEK 1") != std::string::npos);
    CHECK(feed.find("PLAYER OF THE WEEK") != std::string::npos);
    CHECK(feed.find('\x1b') == std::string::npos);
    const std::string recap = render_recap(universe.finish(), universe);
    CHECK(recap.find("CULTURE INDEX") != std::string::npos);
    for (const auto& line : text::split_lines(opening + feed + recap)) {
        INFO(line);
        CHECK(term::width(line) <= 110);
    }
}
