#include <doctest/doctest.h>

#include <algorithm>
#include <ostream> // lets doctest print std::string_view operands
#include <set>
#include <string>

#include "street/hoops/league.hpp"
#include "street/hoops/model.hpp"

using namespace street::hoops;

TEST_CASE("the default league has twelve complete, well-formed teams") {
    const auto league = default_league();
    REQUIRE(league.size() == 12);
    std::set<std::string> abbrevs;
    std::set<std::uint32_t> player_ids;
    std::set<std::string> last_names;
    for (std::size_t t = 0; t < league.size(); ++t) {
        const Team& team = league[t];
        CAPTURE(team.abbrev);
        CHECK(team.id == t + 1);
        CHECK(team.abbrev.size() == 3);
        CHECK(std::all_of(team.abbrev.begin(), team.abbrev.end(), [](char c) { return c >= 'A' && c <= 'Z'; }));
        CHECK(abbrevs.insert(team.abbrev).second);
        CHECK_FALSE(team.city.empty());
        CHECK_FALSE(team.name.empty());
        CHECK_FALSE(team.arena.empty());
        CHECK_FALSE(team.sound.empty());
        REQUIRE(team.roster.size() == 10);

        const auto stars = team.stars();
        CHECK(stars.size() >= 1);
        CHECK(stars.size() <= 2);

        std::set<int> jerseys;
        for (std::size_t s = 0; s < team.roster.size(); ++s) {
            const Player& p = team.roster[s];
            CHECK(p.id == team.id * 100 + s + 1);
            CHECK(player_ids.insert(p.id).second);
            CHECK(jerseys.insert(p.jersey).second);
            CHECK(last_names.insert(p.last_name()).second); // commentary relies on unique names
            CHECK_FALSE(p.nickname.empty());
            CHECK_FALSE(p.archetype.empty());
            CHECK(p.age >= 19);
            CHECK(p.age <= 40);
            const Ratings& r = p.ratings;
            for (int v : {r.inside, r.mid, r.three, r.free_throw, r.passing, r.handling, r.perimeter_d,
                          r.interior_d, r.rebounding, r.athleticism, r.stamina, r.clutch}) {
                CHECK(v >= 25);
                CHECK(v <= 99);
            }
        }
        // Depth chart: a starting five with at least one guard and one big.
        const bool has_guard = std::any_of(team.roster.begin(), team.roster.begin() + 5,
                                           [](const Player& p) { return is_guard(p.position); });
        const bool has_big = std::any_of(team.roster.begin(), team.roster.begin() + 5,
                                         [](const Player& p) { return is_big(p.position); });
        CHECK(has_guard);
        CHECK(has_big);
    }
}

TEST_CASE("the league is identical on every call") {
    const auto a = default_league();
    const auto b = default_league();
    REQUIRE(a.size() == b.size());
    for (std::size_t t = 0; t < a.size(); ++t)
        for (std::size_t p = 0; p < a[t].roster.size(); ++p) {
            const Ratings& x = a[t].roster[p].ratings;
            const Ratings& y = b[t].roster[p].ratings;
            CHECK(x.inside == y.inside);
            CHECK(x.three == y.three);
            CHECK(x.clutch == y.clutch);
            CHECK(x.overall() == y.overall());
        }
}

TEST_CASE("stars rate higher than the bench and archetypes shape ratings") {
    for (const Team& team : default_league()) {
        int best_star = 0, best_bench = 0;
        for (std::size_t i = 0; i < team.roster.size(); ++i) {
            const Player& p = team.roster[i];
            if (p.star) best_star = std::max(best_star, p.ratings.overall());
            if (i >= 8) best_bench = std::max(best_bench, p.ratings.overall());
            if (p.archetype == "Sharpshooter") CHECK(p.ratings.three > p.ratings.inside);
            if (p.archetype == "Rim Protector") CHECK(p.ratings.interior_d > p.ratings.three);
        }
        CHECK(best_star > best_bench);
        CHECK(team.overall() >= 40);
        CHECK(team.overall() <= 99);
    }
}

TEST_CASE("every identity maps to a real play style") {
    const auto league = default_league();
    std::set<std::string> identities;
    for (const Team& team : league) {
        const PlayStyle& style = play_style(team.identity);
        CHECK(style.name == team.identity);
        identities.insert(team.identity);
    }
    CHECK(identities.size() >= 6);
    CHECK(play_style("unknown style").name == "Balanced");
    CHECK(play_style("run & gun").pace > play_style("Grit & Grind").pace);
    CHECK(play_style("Three-Point Barrage").three > play_style("Paint Beasts").three);
    CHECK(play_style("Lockdown D").defense < 0.0);
}

TEST_CASE("find_team matches abbreviations, names and cities, ignoring case and accents") {
    const auto league = default_league();
    REQUIRE(find_team(league, "QNS") != nullptr);
    CHECK(find_team(league, "qns")->abbrev == "QNS");
    CHECK(find_team(league, "bridge poets")->abbrev == "QNS");
    CHECK(find_team(league, "Atlanta")->abbrev == "ATL");
    CHECK(find_team(league, "  Chicago Footwork ")->abbrev == "CHI");
    CHECK(find_team(league, "sao paulo")->abbrev == "SAO");
    CHECK(find_team(league, "tamborzao")->abbrev == "RIO");
    CHECK(find_team(league, "Knicks") == nullptr);
    CHECK(find_team(league, "") == nullptr);
}

TEST_CASE("player names handle nicknames and UTF-8") {
    Player p;
    p.name = "Vinícius Araújo";
    p.nickname = "Pixo";
    CHECK(p.last_name() == "Araújo");
    CHECK(p.short_name() == "V. Araújo");
    CHECK(p.display_name() == "Vinícius \"Pixo\" Araújo");
    p.name = "Ícaro Lima";
    CHECK(p.short_name() == "Í. Lima");
    p.name = "Mononym";
    p.nickname.clear();
    CHECK(p.short_name() == "Mononym");
    CHECK(p.display_name() == "Mononym");
    CHECK(position_name(Position::PG) == "PG");
    CHECK(position_name(Position::C) == "C");
}
