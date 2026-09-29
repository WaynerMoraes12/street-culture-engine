#include <doctest/doctest.h>

#include <algorithm>
#include <ostream> // lets doctest print std::string_view operands
#include <cmath>
#include <stdexcept>
#include <string>

#include "street/hoops/game.hpp"
#include "street/hoops/league.hpp"

using namespace street::hoops;

namespace {

void check_invariants(const GameResult& r, const GameOptions& options) {
    CHECK(r.home_score != r.away_score);
    CHECK(r.periods() == 4 + r.overtimes);
    CHECK(r.period_scores[0].size() == r.period_scores[1].size());
    const double expected_minutes = 4.0 * options.period_minutes * 5 + 25.0 * r.overtimes;
    for (int side = 0; side < 2; ++side) {
        const TeamTotals t = team_totals(r, side);
        int period_sum = 0;
        for (int s : r.period_scores[static_cast<std::size_t>(side)]) period_sum += s;
        CHECK(t.pts == r.score(side));
        CHECK(period_sum == r.score(side));
        CHECK(t.pts == 2 * t.fgm + t.tpm + t.ftm);
        CHECK(t.minutes == doctest::Approx(expected_minutes).epsilon(1e-6));

        int plus_minus = 0;
        for (const PlayerLine& l : r.box[static_cast<std::size_t>(side)]) {
            CAPTURE(l.player_id);
            CHECK(l.pts == 2 * l.fgm + l.tpm + l.ftm);
            CHECK(l.fgm <= l.fga);
            CHECK(l.tpm <= l.tpa);
            CHECK(l.tpa <= l.fga);
            CHECK(l.tpm <= l.fgm);
            CHECK(l.ftm <= l.fta);
            CHECK(l.pf <= 6);
            CHECK(l.minutes >= 0.0);
            CHECK(l.minutes <= 4.0 * options.period_minutes + 5.0 * r.overtimes + 1e-9);
            plus_minus += l.plus_minus;
        }
        // Five players share every point swing.
        CHECK(plus_minus == 5 * (r.score(side) - r.score(1 - side)));
    }
}

GameResult play(const Team& home, const Team& away, std::uint64_t seed,
                const GameOptions& options = {}) {
    street::Rng rng(seed);
    return simulate_game(home, away, rng, options);
}

} // namespace

TEST_CASE("box scores, scores and minutes are internally consistent") {
    const auto league = default_league();
    for (std::uint64_t seed = 1; seed <= 36; ++seed) {
        const Team& home = league[seed % 12];
        const Team& away = league[(seed * 5 + 1) % 12];
        if (home.id == away.id) continue;
        CAPTURE(seed);
        const GameOptions options;
        const GameResult r = play(home, away, seed, options);
        check_invariants(r, options);
        CHECK(r.winner_id() == (r.home_score > r.away_score ? home.id : away.id));
        CHECK(r.loser_id() != r.winner_id());

        // Every field-goal attempt is charted exactly once, inside its zone.
        int fga = 0, fgm = 0, charted = 0, charted_made = 0;
        for (int side = 0; side < 2; ++side) {
            fga += team_totals(r, side).fga;
            fgm += team_totals(r, side).fgm;
        }
        for (const Shot& s : r.shots) {
            ++charted;
            charted_made += s.made ? 1 : 0;
            CHECK(std::abs(s.x) <= 25.0);
            CHECK(s.y >= 0.0);
            CHECK(s.y <= 47.0);
            CHECK(s.value == (s.zone == ShotZone::Three ? 3 : 2));
            const double distance = std::hypot(s.x, s.y - kRimY);
            if (s.zone == ShotZone::Three) CHECK(distance >= kCornerThree - 1e-9);
            if (s.zone != ShotZone::Three) CHECK(distance < kCornerThree);
        }
        CHECK(charted == fga);
        CHECK(charted_made == fgm);

        // Play-by-play runs forward in time and ends on the final score.
        REQUIRE_FALSE(r.plays.empty());
        for (std::size_t i = 1; i < r.plays.size(); ++i) {
            const Play& a = r.plays[i - 1];
            const Play& b = r.plays[i];
            CHECK(b.period >= a.period);
            if (b.period == a.period) CHECK(b.clock <= a.clock + 1e-9);
            CHECK(b.home_score >= a.home_score);
            CHECK(b.away_score >= a.away_score);
        }
        CHECK(r.plays.back().home_score == r.home_score);
        CHECK(r.plays.back().away_score == r.away_score);
    }
}

TEST_CASE("same seed, same game; different seed, different game") {
    const auto league = default_league();
    const GameResult a = play(league[0], league[1], 99);
    const GameResult b = play(league[0], league[1], 99);
    const GameResult c = play(league[0], league[1], 100);
    CHECK(a.home_score == b.home_score);
    CHECK(a.away_score == b.away_score);
    REQUIRE(a.plays.size() == b.plays.size());
    for (std::size_t i = 0; i < a.plays.size(); ++i) CHECK(a.plays[i].text == b.plays[i].text);
    REQUIRE(a.shots.size() == b.shots.size());
    for (std::size_t i = 0; i < a.shots.size(); ++i) CHECK(a.shots[i].x == b.shots[i].x);
    const bool differs =
        a.home_score != c.home_score || a.away_score != c.away_score || a.plays.size() != c.plays.size();
    CHECK(differs);
}

TEST_CASE("recording plays and shots never changes the game") {
    const auto league = default_league();
    for (std::uint64_t seed : {3u, 17u, 2024u}) {
        street::Rng full_rng(seed), fast_rng(seed);
        const GameResult full = simulate_game(league[4], league[9], full_rng, GameOptions{});
        const GameResult fast = simulate_game(league[4], league[9], fast_rng, GameOptions::fast());
        CHECK(fast.plays.empty());
        CHECK(fast.shots.empty());
        CHECK(full.home_score == fast.home_score);
        CHECK(full.away_score == fast.away_score);
        for (std::size_t side = 0; side < 2; ++side)
            for (std::size_t i = 0; i < full.box[side].size(); ++i) {
                CHECK(full.box[side][i].pts == fast.box[side][i].pts);
                CHECK(full.box[side][i].minutes == fast.box[side][i].minutes);
            }
        CHECK(full_rng.next_u64() == fast_rng.next_u64()); // exactly the same draws consumed
    }
}

TEST_CASE("league-wide averages look like the modern NBA") {
    const auto league = default_league();
    street::Rng rng(20240);
    double pts = 0, fgm = 0, fga = 0, tpm = 0, tpa = 0, ftm = 0, fta = 0, tov = 0, reb = 0, ast = 0;
    double stl = 0, blk = 0;
    int teams = 0, home_wins = 0, games = 0;
    for (int g = 0; g < 240; ++g) {
        const Team& home = league[static_cast<std::size_t>(g % 12)];
        const Team& away = league[static_cast<std::size_t>((g + 1 + g / 12) % 12)];
        if (home.id == away.id) continue;
        const GameResult r = simulate_game(home, away, rng, GameOptions::fast());
        ++games;
        home_wins += r.home_score > r.away_score ? 1 : 0;
        for (int side = 0; side < 2; ++side) {
            const TeamTotals t = team_totals(r, side);
            pts += t.pts;
            fgm += t.fgm;
            fga += t.fga;
            tpm += t.tpm;
            tpa += t.tpa;
            ftm += t.ftm;
            fta += t.fta;
            tov += t.tov;
            reb += t.reb();
            ast += t.ast;
            stl += t.stl;
            blk += t.blk;
            ++teams;
        }
    }
    const double n = teams;
    MESSAGE("PTS " << pts / n << "  FG% " << fgm / fga << "  3P " << tpm / n << "/" << tpa / n
                   << " (" << tpm / tpa << ")  FT% " << ftm / fta << " on " << fta / n << "  TOV "
                   << tov / n << "  REB " << reb / n << "  AST " << ast / n << "  STL " << stl / n
                   << "  BLK " << blk / n);
    CHECK(pts / n > 106.0);
    CHECK(pts / n < 122.0);
    CHECK(fgm / fga > 0.44);
    CHECK(fgm / fga < 0.50);
    CHECK(tpm / tpa > 0.33);
    CHECK(tpm / tpa < 0.39);
    CHECK(tpa / n > 30.0);
    CHECK(tpa / n < 42.0);
    CHECK(ftm / fta > 0.74);
    CHECK(ftm / fta < 0.82);
    CHECK(fta / n > 16.0);
    CHECK(fta / n < 27.0);
    CHECK(tov / n > 11.0);
    CHECK(tov / n < 16.0);
    CHECK(reb / n > 40.0);
    CHECK(reb / n < 49.0);
    CHECK(ast / n > 21.0);
    CHECK(ast / n < 30.0);
    CHECK(stl / n > 5.0);
    CHECK(stl / n < 9.5);
    CHECK(blk / n > 3.0);
    CHECK(blk / n < 7.0);
    CHECK(static_cast<double>(home_wins) / games > 0.48); // home court helps
}

TEST_CASE("the rotation gives starters the big minutes") {
    const auto league = default_league();
    street::Rng rng(8);
    double starters = 0.0, rotation = 0.0, deep_bench = 0.0;
    int games = 0;
    for (int g = 0; g < 40; ++g) {
        const GameResult r = simulate_game(league[2], league[7], rng, GameOptions::fast());
        if (r.overtimes > 0) continue;
        ++games;
        for (const auto& box : r.box) {
            for (std::size_t i = 0; i < box.size(); ++i) {
                if (i < 5)
                    starters += box[i].minutes / 10.0;
                else if (i < 8)
                    rotation += box[i].minutes / 6.0;
                else
                    deep_bench += box[i].minutes / 4.0;
            }
        }
    }
    REQUIRE(games > 0);
    starters /= games;
    rotation /= games;
    deep_bench /= games;
    MESSAGE("starters " << starters << " min, rotation " << rotation << ", deep bench " << deep_bench);
    CHECK(starters > 28.0);
    CHECK(starters < 38.0);
    CHECK(rotation < starters);
    CHECK(deep_bench < rotation);
}

TEST_CASE("team identity shows up in the box score") {
    const auto league = default_league();
    const Team& run_and_gun = *find_team(league, "BRX");
    const Team& grind = *find_team(league, "HOU");
    const Team& barrage = *find_team(league, "ATL");
    const Team& paint = *find_team(league, "CPT");
    const Team& opponent = *find_team(league, "DET");
    street::Rng rng(31);
    const auto average = [&](const Team& team, auto stat) {
        double total = 0.0;
        for (int g = 0; g < 30; ++g)
            total += stat(team_totals(simulate_game(team, opponent, rng, GameOptions::fast()), kHome));
        return total / 30.0;
    };
    const auto possessions = [](const TeamTotals& t) { return t.fga - t.oreb + t.tov + 0.44 * t.fta; };
    const auto threes = [](const TeamTotals& t) { return static_cast<double>(t.tpa); };
    const auto free_throws = [](const TeamTotals& t) { return static_cast<double>(t.fta); };
    CHECK(average(run_and_gun, possessions) > average(grind, possessions));
    CHECK(average(barrage, threes) > average(paint, threes) + 10.0);
    CHECK(average(paint, free_throws) > average(barrage, free_throws));
}

TEST_CASE("streaks, blocks, overtime and highlights all happen") {
    const auto league = default_league();
    bool saw_fire = false, saw_overtime = false, saw_block = false, saw_highlight = false;
    for (std::uint64_t seed = 1; seed <= 150 && !(saw_fire && saw_overtime); ++seed) {
        const GameResult r = play(league[seed % 12], league[(seed + 3) % 12], seed);
        for (const Play& p : r.plays) {
            saw_fire |= p.kind == PlayKind::Streak && p.highlight;
            saw_block |= p.kind == PlayKind::Block;
            saw_highlight |= p.highlight;
        }
        if (r.overtimes > 0) {
            saw_overtime = true;
            CHECK(r.plays.back().text.find("OT") != std::string::npos);
            double minutes = 0.0;
            for (const auto& l : r.box[0]) minutes += l.minutes;
            CHECK(minutes == doctest::Approx(240.0 + 25.0 * r.overtimes));
        }
    }
    CHECK(saw_fire);
    CHECK(saw_overtime);
    CHECK(saw_block);
    CHECK(saw_highlight);
}

TEST_CASE("shorter quarters, tiny rosters and bad inputs") {
    const auto league = default_league();
    GameOptions options;
    options.period_minutes = 6;
    const GameResult r = play(league[0], league[1], 5, options);
    check_invariants(r, options);
    CHECK(r.home_score + r.away_score < 180);

    Team iron_five = league[0];
    iron_five.roster.resize(5);
    const GameResult five = play(iron_five, league[1], 6, GameOptions::fast());
    for (const auto& l : five.box[kHome]) CHECK(l.minutes == doctest::Approx(48.0 + 5.0 * five.overtimes));

    Team tiny = league[0];
    tiny.roster.resize(4);
    street::Rng rng(1);
    CHECK_THROWS_AS(simulate_game(tiny, league[1], rng), std::invalid_argument);
    options.period_minutes = 0;
    CHECK_THROWS_AS(simulate_game(league[0], league[1], rng, options), std::invalid_argument);
}

TEST_CASE("game result helpers") {
    const auto league = default_league();
    const GameResult r = play(league[3], league[8], 12);
    const PlayerLine& top = r.top_performer();
    for (const auto& box : r.box)
        for (const auto& l : box) CHECK(l.game_score() <= top.game_score());
    CHECK(r.find_line(top.player_id) == &top);
    CHECK(r.side_of(league[3].roster[0].id) == kHome);
    CHECK(r.side_of(league[8].roster[0].id) == kAway);
    CHECK(r.side_of(999999) == -1);
    CHECK(r.margin() == std::abs(r.home_score - r.away_score));

    PlayerLine line;
    line.pts = 30;
    line.fgm = 11;
    line.fga = 20;
    line.ftm = 6;
    line.fta = 7;
    line.dreb = 5;
    line.ast = 5;
    line.tov = 2;
    CHECK(line.game_score() == doctest::Approx(30 + 4.4 - 14 - 0.4 + 1.5 + 3.5 - 2));
}

TEST_CASE("format_clock reads like a scoreboard") {
    CHECK(format_clock(1, 720.0) == "Q1 12:00");
    CHECK(format_clock(4, 72.0) == "Q4 01:12");
    CHECK(format_clock(4, 71.2) == "Q4 01:12");
    CHECK(format_clock(4, 3.47) == "Q4 00:03.4");
    CHECK(format_clock(5, 0.0) == "OT1 00:00");
    CHECK(format_clock(6, 300.0) == "OT2 05:00");
    CHECK(zone_name(ShotZone::Three) == "3PT");
}

TEST_CASE("the legends in the booth show up, but rarely") {
    const auto league = default_league();
    int legend_lines = 0, games = 0;
    bool compared_not_claimed = true;
    for (std::uint64_t seed = 1; seed <= 80; ++seed) {
        const GameResult r = play(league[seed % 12], league[(seed + 5) % 12], seed);
        ++games;
        for (const Play& p : r.plays) {
            const bool legend = p.text.find("Jordan") != std::string::npos ||
                                p.text.find("Kobe") != std::string::npos ||
                                p.text.find("LeBron") != std::string::npos ||
                                p.text.find("Iverson") != std::string::npos ||
                                p.text.find("Mamba") != std::string::npos ||
                                p.text.find("MJ") != std::string::npos;
            if (!legend) continue;
            ++legend_lines;
            // Homages compare; they never name a fictional player as the legend.
            compared_not_claimed &= p.text.find(" is Jordan") == std::string::npos &&
                                    p.text.find(" is Kobe") == std::string::npos;
        }
    }
    MESSAGE(legend_lines << " legend lines in " << games << " games");
    CHECK(legend_lines >= 5);
    CHECK(legend_lines < games * 2); // special, not constant
    CHECK(compared_not_claimed);
}
