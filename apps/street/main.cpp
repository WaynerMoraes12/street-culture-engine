#include <algorithm>
#include <exception>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "commands.hpp"
#include "street/core/cli.hpp"
#include "street/core/term.hpp"
#include "street/core/version.hpp"

namespace {

using namespace street;

struct CommandEntry {
    std::string_view name;
    std::string_view pillar;
    std::string_view summary;
    int (*run)(app::Args);
};

constexpr CommandEntry kCommands[] = {
    {"beat", "beats", "Synthesize a hip-hop beat from scratch and export it as WAV", app::cmd_beat},
    {"bars", "bars", "Analyze lyrics: syllables, rhyme scheme, rhyme density, flow", app::cmd_bars},
    {"game", "hoops", "Simulate a basketball game with play-by-play and shot charts", app::cmd_game},
    {"season", "hoops", "Run a full season, playoffs and Monte Carlo title odds", app::cmd_season},
    {"teams", "hoops", "Browse the league's teams and rosters", app::cmd_teams},
    {"market", "kicks", "Trade sneakers on a simulated order-book resale exchange", app::cmd_market},
    {"culture", "culture", "All engines, connected: hoops move markets, drops inspire beats", app::cmd_culture},
    {"legends", "culture", "The real history: Herc, Jordan, Iverson, Kobe, LeBron, Maiden", app::cmd_legends},
};

std::size_t edit_distance(std::string_view a, std::string_view b) {
    std::vector<std::size_t> row(b.size() + 1);
    for (std::size_t j = 0; j <= b.size(); ++j) row[j] = j;
    for (std::size_t i = 1; i <= a.size(); ++i) {
        std::size_t diagonal = row[0];
        row[0] = i;
        for (std::size_t j = 1; j <= b.size(); ++j) {
            const std::size_t up = row[j];
            row[j] = std::min({row[j] + 1, row[j - 1] + 1, diagonal + (a[i - 1] == b[j - 1] ? 0 : 1)});
            diagonal = up;
        }
    }
    return row[b.size()];
}

term::Rgb pillar_color(std::string_view pillar) {
    if (pillar == "beats") return term::palette::purple;
    if (pillar == "bars") return term::palette::pink;
    if (pillar == "hoops") return term::palette::orange;
    if (pillar == "kicks") return term::palette::cyan;
    return term::palette::gold;
}

void print_commands() {
    using namespace term;
    std::cout << paint_bold("COMMANDS", palette::gold) << '\n';
    for (const auto& c : kCommands) {
        std::cout << "  " << paint_bold(pad_right(c.name, 9), pillar_color(c.pillar))
                  << paint(pad_right("[" + std::string(c.pillar) + "]", 11), palette::dark) << c.summary
                  << '\n';
    }
    std::cout << '\n'
              << paint_bold("GLOBAL", palette::gold) << '\n'
              << "  " << paint(pad_right("--no-color", 20), palette::cyan) << "Plain output (also honors NO_COLOR)\n"
              << "  " << paint(pad_right("--color", 20), palette::cyan) << "Force colors even when piped\n"
              << "  " << paint(pad_right("--version", 20), palette::cyan) << "Print the version\n\n"
              << paint("Run ", palette::gray) << "street <command> --help"
              << paint(" for details. Try: ", palette::gray) << "street culture\n";
}

int unknown_command(std::string_view name) {
    using namespace term;
    std::cerr << paint_bold("error: ", palette::red) << "unknown command '" << name << "'\n";
    const auto best = std::min_element(std::begin(kCommands), std::end(kCommands),
                                       [&](const CommandEntry& a, const CommandEntry& b) {
                                           return edit_distance(name, a.name) < edit_distance(name, b.name);
                                       });
    if (edit_distance(name, best->name) <= 2)
        std::cerr << paint("  did you mean ", palette::gray) << paint_bold(best->name, palette::gold)
                  << paint("?", palette::gray) << '\n';
    std::cerr << paint("  run 'street' to list every command\n", palette::gray);
    return 2;
}

} // namespace

int main(int argc, char** argv) {
    term::init();

    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) {
        const std::string_view token = argv[i];
        if (token == "--no-color") {
            term::set_color_enabled(false);
        } else if (token == "--color") {
            term::set_color_enabled(true);
        } else {
            args.emplace_back(token);
        }
    }

    if (args.empty() || args[0] == "help" || args[0] == "--help" || args[0] == "-h") {
        app::print_banner();
        print_commands();
        return 0;
    }
    if (args[0] == "--version" || args[0] == "-V" || args[0] == "version") {
        std::cout << "street " << kVersion << '\n';
        return 0;
    }

    const auto it = std::find_if(std::begin(kCommands), std::end(kCommands),
                                 [&](const CommandEntry& c) { return c.name == args[0]; });
    if (it == std::end(kCommands)) return unknown_command(args[0]);

    try {
        return it->run(std::span<const std::string>(args).subspan(1));
    } catch (const cli::UsageError& e) {
        std::cerr << term::paint_bold("error: ", term::palette::red) << e.what() << '\n'
                  << term::paint("  see 'street " + args[0] + " --help'\n", term::palette::gray);
        return 2;
    } catch (const std::exception& e) {
        std::cerr << term::paint_bold("error: ", term::palette::red) << e.what() << '\n';
        return 1;
    }
}
