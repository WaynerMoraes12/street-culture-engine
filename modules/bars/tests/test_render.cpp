#include <doctest/doctest.h>

#include <ostream>
#include <string>

#include "street/bars/analysis.hpp"
#include "street/bars/ghostwriter.hpp"
#include "street/bars/render.hpp"
#include "street/core/term.hpp"
#include "street/core/text.hpp"

namespace bars = street::bars;
namespace term = street::term;
using bars::Language;

namespace {

/// Restores the global colour switch when a test ends, pass or fail.
struct ColorGuard {
    bool saved = term::color_enabled();
    explicit ColorGuard(bool enabled) { term::set_color_enabled(enabled); }
    ~ColorGuard() { term::set_color_enabled(saved); }
    ColorGuard(const ColorGuard&) = delete;
    ColorGuard& operator=(const ColorGuard&) = delete;
};

} // namespace

TEST_CASE("plain render is clean, aligned and complete") {
    const ColorGuard plain(false);
    const bars::Analysis a = bars::analyze(bars::sample_verse(Language::English));
    const std::string out = bars::render(a, {.bpm = 90, .show_stats = true, .width = 100});

    CHECK(out.find('\x1b') == std::string::npos);
    CHECK(out.find("scheme  AABB CCDD EEFF GGGG") != std::string::npos);
    CHECK(out.find("Laced up tight for the late-night fight,") != std::string::npos);
    CHECK(out.find(" 16  G  ") != std::string::npos); // bar number, scheme letter
    CHECK(out.find("FLOW REPORT") != std::string::npos);
    CHECK(out.find("rhyme density") != std::string::npos);
    CHECK(out.find("at 90 BPM") != std::string::npos);
    CHECK(out.find("RHYME FAMILIES") != std::string::npos);
    for (const std::string& line : street::text::split_lines(out)) {
        CAPTURE(line);
        CHECK(term::width(line) <= 100);
    }
}

TEST_CASE("long lines wrap inside narrow widths") {
    const ColorGuard plain(false);
    const std::string verse = "this is a very long line that keeps on going with words and more "
                              "words until it has to wrap around the edge of the screen\nshort one";
    const std::string out = bars::render(bars::analyze(verse), {.bpm = 90, .show_stats = false, .width = 60});
    for (const std::string& line : street::text::split_lines(out)) CHECK(term::width(line) <= 60);
    CHECK(out.find("FLOW REPORT") == std::string::npos);
    CHECK(out.find("screen") != std::string::npos);
}

TEST_CASE("coloured render paints rhymes") {
    const ColorGuard color(true);
    const std::string out = bars::render(bars::analyze("the game\nmy name\n"));
    CHECK(out.find("\x1b[") != std::string::npos);
    CHECK(term::strip_ansi(out).find("the game") != std::string::npos);
}

TEST_CASE("flow labels") {
    CHECK(bars::flow_label(2.0) == "laid-back");
    CHECK(bars::flow_label(4.0) == "steady");
    CHECK(bars::flow_label(6.0) == "rapid-fire");
    CHECK(bars::flow_label(9.0) == "chopper");
}

TEST_CASE("rhyme list render") {
    const ColorGuard plain(false);
    const std::string out = bars::render_rhymes("game", Language::English);
    CHECK(out.find("perfect") != std::string::npos);
    CHECK(out.find("fame") != std::string::npos);
    CHECK(out.find("slant") != std::string::npos);
    const std::string pt = bars::render_rhymes("coração", Language::Auto);
    CHECK(pt.find("Portuguese") != std::string::npos);
    CHECK(pt.find("razão") != std::string::npos);
}

TEST_CASE("empty analysis renders without stats noise") {
    const ColorGuard plain(false);
    const std::string out = bars::render(bars::analyze(""));
    CHECK(out.find("(no lyrics)") != std::string::npos);
}
