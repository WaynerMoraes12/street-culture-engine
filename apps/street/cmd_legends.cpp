#include <iostream>

#include "commands.hpp"
#include "street/core/cli.hpp"
#include "street/culture/legends.hpp"

namespace street::app {

int cmd_legends(Args args) {
    cli::Command cmd{"legends", "The real history behind the simulation, 1973 to today, on a C-90 cassette"};
    cmd.option("from", "YEAR", "First year to show", "0")
        .option("to", "YEAR", "Last year to show", "9999")
        .option("pillar", "NAME", "Only one pillar: music, metal, tech, hoops, kicks, film")
        .example("street legends", "the whole tape")
        .example("street legends --from 1980 --to 1989", "side A, the neon decade")
        .example("street legends --pillar hoops", "Jordan, Iverson, Kobe, LeBron");
    const auto parsed = cmd.parse(args);
    if (parsed.flag("help")) {
        std::cout << cmd.help();
        return 0;
    }

    culture::TimelineOptions options;
    options.from = static_cast<int>(parsed.integer("from"));
    options.to = static_cast<int>(parsed.integer("to"));
    if (parsed.given("pillar")) {
        options.pillar = culture::parse_pillar(parsed.str("pillar"));
        if (!options.pillar)
            throw cli::UsageError("--pillar must be one of music, metal, tech, hoops, kicks, film");
    }
    std::cout << '\n' << culture::render_timeline(options) << '\n';
    return 0;
}

} // namespace street::app
