#include <filesystem>
#include <format>
#include <iostream>

#include "commands.hpp"
#include "street/core/cli.hpp"
#include "street/core/rng.hpp"
#include "street/core/stopwatch.hpp"
#include "street/core/term.hpp"
#include "street/core/text.hpp"
#include "street/culture/render.hpp"
#include "street/culture/universe.hpp"

namespace street::app {

int cmd_culture(Args args) {
    cli::Command cmd{"culture", "All engines, connected: hoops move markets, stars get verses and anthems"};
    cmd.option("seed", "N", "Random seed (default: random, printed so you can replay it)")
        .option("weeks", "N", "Culture weeks to play (0 = the whole regular season)", "0")
        .option("days-per-week", "N", "Game days per culture week", "3")
        .option("signatures", "N", "Stars who drop a signature shoe", "6")
        .option("lang", "en|pt", "Language of the ghostwritten verses", "en")
        .option("mixtape", "DIR", "Save every Player of the Week anthem as a WAV in DIR")
        .flag("no-anthems", "Skip audio synthesis (faster)")
        .flag("quiet", "Only print opening night and the recap")
        .example("street culture --seed 7", "a full season, week by week")
        .example("street culture --weeks 3 --mixtape mixtape/", "three weeks, anthems saved as WAV")
        .example("street culture --lang pt", "verses in Brazilian Portuguese");
    const auto parsed = cmd.parse(args);
    if (parsed.flag("help")) {
        std::cout << cmd.help();
        return 0;
    }

    culture::Config config;
    config.seed = parsed.given("seed") ? parsed.unsigned_integer("seed") : random_seed();
    config.weeks = static_cast<int>(parsed.integer("weeks"));
    config.days_per_week = static_cast<int>(parsed.integer("days-per-week"));
    config.signatures = static_cast<std::size_t>(parsed.unsigned_integer("signatures"));
    config.anthems = !parsed.flag("no-anthems");
    if (config.weeks < 0) throw cli::UsageError("--weeks cannot be negative");
    if (config.days_per_week < 1) throw cli::UsageError("--days-per-week must be at least 1");
    if (config.signatures < 1 || config.signatures > 24)
        throw cli::UsageError("--signatures must be between 1 and 24");

    const std::string& lang = parsed.str("lang");
    if (lang == "en")
        config.language = bars::Language::English;
    else if (lang == "pt")
        config.language = bars::Language::Portuguese;
    else
        throw cli::UsageError("--lang must be 'en' or 'pt'");
    if (parsed.given("mixtape")) config.mixtape = std::filesystem::path(parsed.str("mixtape"));

    const Stopwatch clock;
    culture::Universe universe(config);
    std::cout << '\n'
              << term::paint_bold("STREET CULTURE", term::palette::gold)
              << term::paint("  one season · one sneaker exchange · one studio · seed ", term::palette::gray)
              << term::paint_bold(std::to_string(config.seed), term::palette::white) << "\n\n";
    std::cout << culture::render_opening(universe) << std::flush;

    while (!universe.done()) {
        const culture::WeekReport week = universe.play_week();
        if (!parsed.flag("quiet")) std::cout << culture::render_week(week, universe) << std::flush;
    }
    std::cout << culture::render_recap(universe.finish(), universe);
    std::cout << term::paint(std::format("\n  simulated in {}s · replay with --seed {}\n",
                                         text::fixed(clock.seconds(), 2), config.seed),
                             term::palette::gray);
    return 0;
}

} // namespace street::app
