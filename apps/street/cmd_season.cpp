#include <format>
#include <iostream>
#include <map>
#include <string>

#include "commands.hpp"
#include "street/core/cli.hpp"
#include "street/core/rng.hpp"
#include "street/core/term.hpp"
#include "street/core/text.hpp"
#include "street/hoops/league.hpp"
#include "street/hoops/monte_carlo.hpp"
#include "street/hoops/render.hpp"
#include "street/hoops/season.hpp"

namespace street::app {
namespace {

/// In-place progress bar (only when writing to a color-capable terminal).
void progress(std::string_view label, int done, int total) {
    using namespace term;
    const double fraction = total > 0 ? static_cast<double>(done) / total : 1.0;
    std::cout << '\r' << paint(std::string(label), palette::gray) << ' '
              << paint("▕" + meter(fraction, 30) + "▏", palette::orange) << ' '
              << std::format("{}/{}", done, total) << "   " << std::flush;
}

void clear_progress() { std::cout << '\r' << std::string(70, ' ') << '\r' << std::flush; }

int parse_playoff_field(const cli::Parsed& parsed, std::size_t league_size) {
    const long long field = parsed.integer("playoffs");
    if (field < 2 || field > static_cast<long long>(league_size) || (field & (field - 1)) != 0)
        throw cli::UsageError(std::format("--playoffs must be a power of two between 2 and {}", league_size));
    return static_cast<int>(field);
}

std::string champion_panel(const hoops::Season& season, const hoops::PlayoffResult& playoffs) {
    using namespace term;
    const hoops::Team& champ = season.team(playoffs.champion_id);
    const hoops::Series& finals = playoffs.finals();
    const hoops::Team& runner_up = season.team(finals.loser_id());
    const hoops::Player& mvp = season.player(playoffs.finals_mvp_id);
    int pts = 0, reb = 0, ast = 0;
    for (const auto& g : finals.games)
        if (const hoops::PlayerLine* l = g.find_line(mvp.id)) {
            pts += l->pts;
            reb += l->reb();
            ast += l->ast;
        }
    const double n = static_cast<double>(finals.games.size());
    const std::vector<std::string> lines = {
        paint_bold(text::to_upper(champ.full_name()), champ.color) +
            std::format("  beat the {} {}-{} in the finals", runner_up.full_name(),
                        std::max(finals.high_wins, finals.low_wins),
                        std::min(finals.high_wins, finals.low_wins)),
        paint("Finals MVP  ", palette::gray) + emphasize(mvp.display_name()) +
            paint(std::format("  {:.1f} pts · {:.1f} reb · {:.1f} ast", pts / n, reb / n, ast / n),
                  palette::gray),
    };
    return panel("CHAMPIONS", lines, palette::gold);
}

int run_odds(const std::vector<hoops::Team>& league, const cli::Parsed& parsed, std::uint64_t seed,
             bool quiet) {
    const long long seasons = parsed.integer("odds");
    if (seasons < 1 || seasons > 1'000'000) throw cli::UsageError("--odds expects 1 to 1,000,000 seasons");
    const long long threads = parsed.integer("threads");
    if (threads < 0 || threads > 1024) throw cli::UsageError("--threads expects 0 (all cores) to 1024");

    hoops::TitleOddsOptions options;
    options.playoff_teams = parse_playoff_field(parsed, league.size());
    options.rounds = static_cast<int>(parsed.integer("rounds"));
    const bool show_progress = !quiet && term::color_enabled();
    if (show_progress)
        options.progress = [](int done, int total) { progress("simulating seasons", done, total); };

    const hoops::TitleOddsReport report = hoops::simulate_title_odds(
        league, static_cast<int>(seasons), seed, static_cast<unsigned>(threads), options);
    if (show_progress) clear_progress();

    std::cout << hoops::render_title_odds(report.odds, league) << '\n';
    const double rate = report.seconds > 0.0 ? static_cast<double>(report.games) / report.seconds : 0.0;
    std::cout << term::paint(std::format("simulated {} seasons · {} games in {:.2f} s on {} thread{} ({} "
                                         "games/s)\n",
                                         text::thousands(report.seasons),
                                         text::thousands(static_cast<std::int64_t>(report.games)),
                                         report.seconds, report.threads, report.threads == 1 ? "" : "s",
                                         text::thousands(static_cast<std::int64_t>(rate))),
                             term::palette::gray);
    return 0;
}

} // namespace

int cmd_season(Args args) {
    cli::Command cmd{"season", "Run a full season, playoffs and Monte Carlo title odds"};
    cmd.option("seed", "N", "Random seed (printed so any season can be replayed)")
        .option("odds", "N", "Instead of one season, simulate N seasons and print title odds")
        .option("threads", "N", "Worker threads for --odds (0 = all cores)", "0")
        .option("playoffs", "N", "Playoff field: 2, 4 or 8 teams", "8")
        .option("rounds", "N", "Round robins in the regular season", "2")
        .flag("quiet", "Standings and champion only (no progress, leaders or bracket)")
        .example("street season --seed 7", "one full season with standings, leaders, MVP race and bracket")
        .example("street season --odds 2000", "championship odds from 2,000 simulated seasons")
        .example("street season --odds 500 --threads 1", "same answer as any thread count, just slower");
    const auto parsed = cmd.parse(args);
    if (parsed.flag("help")) {
        std::cout << cmd.help();
        return 0;
    }
    const long long rounds = parsed.integer("rounds");
    if (rounds < 1 || rounds > 20) throw cli::UsageError("--rounds expects 1 to 20");
    const bool quiet = parsed.flag("quiet");

    const auto league = hoops::default_league();
    const std::uint64_t seed = parsed.given("seed") ? parsed.unsigned_integer("seed") : random_seed();
    const int field = parse_playoff_field(parsed, league.size());
    std::cout << term::paint(std::format("seed {} · replay: street season --seed {}{}\n\n", seed, seed,
                                         parsed.given("odds") ? " --odds " + parsed.str("odds") : ""),
                             term::palette::gray);
    if (parsed.given("odds")) return run_odds(league, parsed, seed, quiet);

    hoops::Season season(league, seed, static_cast<int>(rounds));
    const bool show_progress = !quiet && term::color_enabled();
    while (!season.regular_season_done()) {
        season.play_next_day();
        if (show_progress) progress("regular season", season.day(), season.total_days());
    }
    if (show_progress) clear_progress();

    std::cout << hoops::render_standings(season, field) << '\n';
    if (!quiet) {
        std::cout << hoops::render_leaders(season) << '\n';
        std::cout << hoops::render_mvp_race(season) << '\n';
    }
    const hoops::PlayoffResult playoffs = season.play_playoffs(field);
    if (!quiet) std::cout << term::rule(96, "PLAYOFFS", term::palette::orange) << "\n\n"
                          << hoops::render_bracket(playoffs, season.teams()) << '\n';
    std::cout << champion_panel(season, playoffs);
    return 0;
}

} // namespace street::app
