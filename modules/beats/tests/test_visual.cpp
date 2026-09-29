#include "street/beats/beats.hpp"
#include "street/core/term.hpp"
#include "street/core/text.hpp"

#include <doctest/doctest.h>
#include <string>
#include <vector>

using namespace street::beats;
namespace term = street::term;

namespace {

/// Runs `render` with colors off and on; returns {plain, colored}.
template <class Render> std::pair<std::string, std::string> both_modes(Render&& render) {
    const bool before = term::color_enabled();
    term::set_color_enabled(false);
    std::string plain = render();
    term::set_color_enabled(true);
    std::string colored = render();
    term::set_color_enabled(before);
    return {plain, colored};
}

bool contains(const std::string& s, std::string_view part) {
    return s.find(part) != std::string::npos;
}

const Pattern& sample_pattern() {
    static const Pattern p = [] {
        BeatSpec spec;
        spec.style = Style::Trap;
        spec.seed = 42;
        return compose(spec);
    }();
    return p;
}

} // namespace

TEST_CASE("grid: identical text with and without color, aligned rows, fits 100 columns") {
    const auto [plain, colored] = both_modes([] { return render_grid(sample_pattern()); });
    CHECK_FALSE(contains(plain, "\x1b["));
    CHECK(contains(colored, "\x1b["));
    CHECK(term::strip_ansi(colored) == plain);

    std::size_t row_width = 0;
    for (const std::string& line : street::text::split_lines(plain)) {
        CHECK(term::width(line) <= 100);
        if (contains(line, "│")) { // track rows all share one width
            if (row_width == 0) row_width = term::width(line);
            CHECK(term::width(line) == row_width);
        }
    }
    CHECK(row_width > 0);
    for (const char* part : {"kick", "clap", "hat", "bass", "keys", "accent", "≡"})
        CHECK(contains(plain, part));
    CHECK(contains(plain, sample_pattern().chords.front().name));
}

TEST_CASE("grid glyphs encode velocity, rolls and slides") {
    const Pattern p = parse_pattern("chords Am\n"
                                    "hat  x o g . r t . . . . . . . . . .\n"
                                    "bass x . . . ^ . . ~ . . . . . . . .\n");
    const auto [plain, colored] = both_modes([&] { return render_grid(p); });
    (void)colored;
    CHECK(contains(plain, "●•○· =≡··"));
    CHECK(contains(plain, "╱"));
    CHECK(contains(plain, "╲"));
    CHECK(contains(plain, "1 Am"));
}

TEST_CASE("grid shows at most max_bars and says how many were left out") {
    BeatSpec spec;
    spec.bars = 12;
    const Pattern p = compose(spec);
    const auto [plain, colored] = both_modes([&] { return render_grid(p, GridOptions{4, 4}); });
    (void)colored;
    CHECK(contains(plain, "8 more bars"));
    CHECK_FALSE(contains(plain, " 5 "));
    const auto all = both_modes([&] { return render_grid(p, GridOptions{0, 4}); }).first;
    CHECK(contains(all, "12 "));
}

TEST_CASE("waveform: requested size, silent center line, full-scale blocks") {
    AudioBuffer audio(44100);
    for (std::size_t i = 22050; i < 44100; ++i)
        audio.left[i] = audio.right[i] = (i % 2 == 0) ? 0.9f : -0.9f;
    const auto [plain, colored] = both_modes([&] { return render_waveform(audio, 40, 3); });
    CHECK(term::strip_ansi(colored) == plain);
    const auto lines = street::text::split_lines(plain);
    REQUIRE(lines.size() == 3);
    for (const auto& line : lines)
        CHECK(term::width(line) == 40);
    CHECK(contains(lines[0], "⠀")); // silence in the first half: blank top row…
    CHECK(contains(lines[0], "⣿")); // …full scale in the second half
    CHECK(lines[0].rfind("⣿") > lines[0].find("⠀"));

    const std::string empty =
        both_modes([] { return render_waveform(AudioBuffer{}, 10, 2); }).first;
    CHECK(street::text::split_lines(empty).size() == 2);
}

TEST_CASE("summary panel and style list") {
    BeatSpec spec;
    spec.style = Style::Drill;
    spec.seed = 777;
    spec.bars = 1;
    const Beat beat = generate(spec);
    const auto [plain, colored] = both_modes([&] { return render_summary(beat); });
    CHECK(term::strip_ansi(colored) == plain);
    for (const std::string& part : {beat.spec.title, std::string("Drill"), std::string("777"),
                                    std::to_string(beat.spec.bpm) + " BPM", beat.chords.front()})
        CHECK(contains(plain, part));
    for (const std::string& line : street::text::split_lines(plain))
        CHECK(term::width(line) <= 100);

    const std::string list = both_modes([] { return render_style_list(); }).first;
    for (const StyleInfo& s : styles())
        CHECK(contains(list, s.id));
    for (const std::string& line : street::text::split_lines(list))
        CHECK(term::width(line) <= 100);
}
