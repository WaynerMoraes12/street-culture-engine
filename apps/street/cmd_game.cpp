#include <chrono>
#include <format>
#include <iostream>
#include <string>
#include <thread>

#include "commands.hpp"
#include "street/core/cli.hpp"
#include "street/core/rng.hpp"
#include "street/core/term.hpp"
#include "street/core/text.hpp"
#include "street/hoops/game.hpp"
#include "street/hoops/league.hpp"
#include "street/hoops/render.hpp"

namespace street::app {
namespace {

const hoops::Team& resolve_team(const std::vector<hoops::Team>& league, const std::string& key) {
    if (const hoops::Team* team = hoops::find_team(league, key)) return *team;
    std::vector<std::string> abbrevs;
    for (const auto& t : league) abbrevs.push_back(t.abbrev);
    throw cli::UsageError("unknown team '" + key + "' (try one of: " + text::join(abbrevs, ", ") + ")");
}

/// Picks a team for an unspecified side, never the same as `other`.
const hoops::Team& random_team(const std::vector<hoops::Team>& league, Rng& rng,
                               const hoops::Team* other) {
    for (;;) {
        const hoops::Team& t = league[rng.below(static_cast<std::uint32_t>(league.size()))];
        if (other == nullptr || t.id != other->id) return t;
    }
}

std::string matchup_header(const hoops::Team& home, const hoops::Team& away) {
    using namespace term;
    const auto side = [](const hoops::Team& t) {
        return paint_bold(t.full_name(), t.color) + paint("  (" + t.identity + ")", palette::gray);
    };
    return "  " + side(away) + paint("  at  ", palette::dark) + side(home) + '\n' +
           paint("  " + home.arena + ", " + home.city, palette::dark) + "\n";
}

} // namespace

int cmd_game(Args args) {
    cli::Command cmd{"game", "Simulate a basketball game with play-by-play and shot charts"};
    cmd.option("home", "ABR", "Home team: abbreviation, name or city (default: random)")
        .option("away", "ABR", "Away team (default: random)")
        .option("seed", "N", "Random seed; every game prints its seed so it can be replayed")
        .flag("live", "Stream the full play-by-play in real time")
        .option("speed", "X", "Playback speed for --live (2 = twice as fast)", "1")
        .option("plays", "MODE", "Play-by-play to print: all, highlights or none", "highlights")
        .option("shot-chart", "SIDE", "Shot chart: winner, home, away, both or none", "winner")
        .example("street game --home QNS --away BRX --seed 42", "a reproducible Queens vs Bronx game")
        .example("street game --live --speed 4", "watch a random game unfold, four times faster")
        .example("street game --plays none --shot-chart both", "just the box score and both charts");
    const auto parsed = cmd.parse(args);
    if (parsed.flag("help")) {
        std::cout << cmd.help();
        return 0;
    }

    const std::string plays = parsed.str("plays");
    if (plays != "all" && plays != "highlights" && plays != "none")
        throw cli::UsageError("--plays expects all, highlights or none, got '" + plays + "'");
    const std::string chart = parsed.str("shot-chart");
    if (chart != "winner" && chart != "home" && chart != "away" && chart != "both" && chart != "none")
        throw cli::UsageError("--shot-chart expects winner, home, away, both or none, got '" + chart + "'");
    const double speed = parsed.number("speed");
    if (!(speed > 0.0)) throw cli::UsageError("--speed must be positive");
    const bool live = parsed.flag("live");

    const auto league = hoops::default_league();
    const std::uint64_t seed = parsed.given("seed") ? parsed.unsigned_integer("seed") : random_seed();
    Rng picker(seed, 0x7EA35); // team picks never touch the game's own stream
    const hoops::Team* home = parsed.given("home") ? &resolve_team(league, parsed.str("home")) : nullptr;
    const hoops::Team* away = parsed.given("away") ? &resolve_team(league, parsed.str("away")) : nullptr;
    if (home == nullptr) home = &random_team(league, picker, away);
    if (away == nullptr) away = &random_team(league, picker, home);
    if (home->id == away->id) throw cli::UsageError("a team cannot play itself");

    Rng rng(seed);
    const hoops::GameResult game = hoops::simulate_game(*home, *away, rng);

    using namespace term;
    std::cout << paint(std::format("seed {} · replay: street game --home {} --away {} --seed {}\n\n",
                                   seed, home->abbrev, away->abbrev, seed),
                       palette::gray);
    std::cout << matchup_header(*home, *away) << '\n';

    if (live || plays != "none") {
        const bool all = live || plays == "all";
        std::cout << rule(96, all ? "PLAY-BY-PLAY" : "HIGHLIGHTS", palette::orange) << '\n';
        for (const hoops::Play& play : game.plays) {
            if (!all && !play.highlight) continue;
            std::cout << hoops::render_play(play, *home, *away) << '\n';
            if (live) {
                std::cout.flush();
                const double base = play.kind == hoops::PlayKind::Info ? 1.6 : play.highlight ? 0.9 : 0.3;
                std::this_thread::sleep_for(std::chrono::duration<double>(base / speed));
            }
        }
        std::cout << '\n';
    }

    std::cout << hoops::render_scoreboard(game, *home, *away) << '\n';
    std::cout << hoops::render_box_score(game, *home, *away) << '\n';

    const auto print_chart = [&](int side) {
        std::cout << hoops::render_shot_chart(game, side, side == hoops::kHome ? *home : *away) << '\n';
    };
    if (chart == "winner") print_chart(game.winner_side());
    if (chart == "home" || chart == "both") print_chart(hoops::kHome);
    if (chart == "away" || chart == "both") print_chart(hoops::kAway);
    return 0;
}

} // namespace street::app
