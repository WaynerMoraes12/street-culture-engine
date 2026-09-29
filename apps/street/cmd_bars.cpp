// street bars — lyric & flow analyzer: rhyme scheme, rhyme density, syllables and flow
// speed for English and Brazilian Portuguese, plus rhyme lookup and a ghostwriter.

#include <algorithm>
#include <cstdint>
#include <format>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

#include "commands.hpp"
#include "street/bars/bars.hpp"
#include "street/core/cli.hpp"
#include "street/core/term.hpp"
#include "street/core/text.hpp"

namespace street::app {
namespace {

bars::Language language_option(const cli::Parsed& args, std::string_view name) {
    const auto lang = bars::parse_language(args.str(name));
    if (!lang)
        throw cli::UsageError(
            std::format("--{} expects auto, en or pt, got '{}'", name, args.str(name)));
    return *lang;
}

int bounded(const cli::Parsed& args, std::string_view name, long long lo, long long hi) {
    const long long value = args.integer(name);
    if (value < lo || value > hi)
        throw cli::UsageError(std::format("--{} must be between {} and {}", name, lo, hi));
    return static_cast<int>(value);
}

std::string read_lyrics(const std::string& source) {
    if (source == "-")
        return std::string(std::istreambuf_iterator<char>(std::cin), std::istreambuf_iterator<char>());
    return text::read_file(source);
}

std::string verse_panel(const std::string& verse, std::uint64_t seed) {
    std::vector<std::string> rows;
    for (const std::string& line : text::split_lines(verse)) rows.push_back(line);
    return term::panel(std::format("GHOSTWRITER  seed {}", seed), rows, term::palette::pink);
}

} // namespace

int cmd_bars(Args args) {
    cli::Command cmd{"bars", "Analyze lyrics: syllables, rhyme scheme, rhyme density and flow"};
    cmd.positional("FILE", "Lyrics to analyze, or - for stdin (default: a built-in sample verse)")
        .option("lang", "auto|en|pt", "Language of the lyrics", "auto")
        .option("bpm", "N", "Tempo for the flow readout (one line = one 4/4 bar)", "90")
        .option("sample", "en|pt|tribute", "Built-in verse to analyze when no FILE is given", "en")
        .option("rhymes", "WORD", "List rhymes for WORD from the built-in lexicon")
        .option("limit", "N", "How many rhymes --rhymes lists", "24")
        .option("ghostwrite", "NAME", "Write a verse about NAME, then analyze it")
        .option("team", "TEAM", "Ghostwriter: the team to shout out")
        .option("shoe", "SHOE", "Ghostwriter: the sneaker to name-drop")
        .option("points", "N", "Ghostwriter: points scored (default: drawn from the seed)")
        .option("lines", "N", "Ghostwriter: number of lines, 2-32", "8")
        .option("seed", "N", "Ghostwriter: random seed", "1")
        .flag("no-stats", "Print only the annotated lyric, without the flow report")
        .example("street bars", "Analyze the built-in English sample verse")
        .example("street bars --sample pt --bpm 95", "The Portuguese sample at 95 BPM")
        .example("street bars --sample tribute", "A roll call of legends: MJ, Kobe, AI, Em, Maiden")
        .example("street bars examples/lyrics/hardwood_hymn.txt", "Analyze a lyrics file")
        .example("cat verse.txt | street bars - --lang en", "Read lyrics from stdin")
        .example("street bars --rhymes game", "Perfect and slant rhymes for 'game'")
        .example("street bars --ghostwrite Jay --team Uptown --points 41 --seed 7",
                 "Ghostwrite eight bars about Jay and analyze them")
        .example("street bars --ghostwrite Jota --lang pt --lines 4", "Ghostwrite in Portuguese");

    const cli::Parsed p = cmd.parse(args);
    if (p.flag("help")) {
        std::cout << cmd.help();
        return 0;
    }

    const bars::Language lang = language_option(p, "lang");
    const bars::RenderOptions render_options{
        .bpm = bounded(p, "bpm", 20, 400),
        .show_stats = !p.flag("no-stats"),
        .width = std::clamp(term::columns(), 60, 100),
    };
    if (p.positional().size() > 1) throw cli::UsageError("expected at most one lyrics FILE");
    const int modes = static_cast<int>(!p.positional().empty()) + static_cast<int>(p.given("rhymes")) +
                      static_cast<int>(p.given("ghostwrite"));
    if (modes > 1) throw cli::UsageError("choose one of FILE, --rhymes or --ghostwrite");

    if (p.given("rhymes")) {
        const std::string& word = p.str("rhymes");
        if (bars::count_syllables(word, lang) == 0)
            throw cli::UsageError(std::format("--rhymes needs a word, got '{}'", word));
        const auto limit = static_cast<std::size_t>(bounded(p, "limit", 1, 200));
        std::cout << bars::render_rhymes(word, lang, limit, render_options.width);
        return 0;
    }

    if (p.given("ghostwrite")) {
        bars::VerseRequest request;
        request.subject = p.str("ghostwrite");
        request.team = p.str("team");
        request.shoe = p.str("shoe");
        request.points = p.given("points") ? bounded(p, "points", 1, 200) : 0;
        request.language = lang == bars::Language::Portuguese ? lang : bars::Language::English;
        request.seed = p.unsigned_integer("seed");
        request.lines = bounded(p, "lines", 2, 32);
        const std::string verse = bars::ghostwrite(request);
        std::cout << verse_panel(verse, request.seed) << '\n'
                  << bars::render(bars::analyze(verse, request.language), render_options);
        return 0;
    }

    std::string lyrics;
    if (p.positional().empty()) {
        const auto sample = bars::parse_sample(p.str("sample"));
        if (!sample)
            throw cli::UsageError(
                std::format("--sample expects en, pt or tribute, got '{}'", p.str("sample")));
        lyrics = std::string(bars::sample_verse(*sample));
    } else {
        lyrics = read_lyrics(p.positional().front());
    }
    const bars::Analysis analysis = bars::analyze(lyrics, lang);
    if (analysis.bars == 0)
        throw std::runtime_error(std::format(
            "no lyrics found in '{}'", p.positional().empty() ? "sample" : p.positional().front()));
    std::cout << bars::render(analysis, render_options);
    return 0;
}

} // namespace street::app
