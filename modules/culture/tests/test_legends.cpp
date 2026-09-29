#include <doctest/doctest.h>

#include <set>
#include <string>

#include "street/core/term.hpp"
#include "street/core/text.hpp"
#include "street/culture/legends.hpp"

using namespace street;
using namespace street::culture;

TEST_CASE("the timeline is chronological and complete") {
    const auto all = moments();
    REQUIRE(all.size() >= 25);
    CHECK(all.front().year == 1973);
    for (std::size_t i = 1; i < all.size(); ++i) CHECK(all[i - 1].year <= all[i].year);
    std::set<Pillar> pillars;
    for (const auto& m : all) {
        CHECK_FALSE(m.title.empty());
        CHECK_FALSE(m.story.empty());
        CHECK_FALSE(m.engine.empty());
        pillars.insert(m.pillar);
    }
    CHECK(pillars.size() == 6); // music, metal, tech, hoops, kicks, film
}

TEST_CASE("the legends the culture is built on are all there") {
    std::string everything;
    for (const auto& m : moments()) everything += std::string(m.title) + ' ' + std::string(m.story) + '\n';
    for (const char* name : {"Jordan", "Kobe", "LeBron", "Iverson", "Eminem", "Iron Maiden", "Run-DMC",
                             "Kool Herc", "TR-808"})
        CHECK_MESSAGE(everything.find(name) != std::string::npos, name);
}

TEST_CASE("pillars parse case-insensitively") {
    CHECK(parse_pillar("hoops") == Pillar::Hoops);
    CHECK(parse_pillar("METAL") == Pillar::Metal);
    CHECK_FALSE(parse_pillar("jazz"));
    CHECK(pillar_name(Pillar::Kicks) == "KICKS");
}

TEST_CASE("the cassette renders both sides and respects filters") {
    term::set_color_enabled(false);
    const std::string full = render_timeline();
    CHECK(full.find("SIDE A") != std::string::npos);
    CHECK(full.find("SIDE B") != std::string::npos);
    CHECK(full.find("THE 80s") != std::string::npos);
    CHECK(full.find('\x1b') == std::string::npos);
    for (const auto& line : text::split_lines(full)) {
        INFO(line);
        CHECK(term::width(line) <= 100);
    }

    TimelineOptions eighties;
    eighties.from = 1980;
    eighties.to = 1989;
    const std::string side_a = render_timeline(eighties);
    CHECK(side_a.find("SIDE B") == std::string::npos);
    CHECK(side_a.find("The Shot") != std::string::npos);
    CHECK(side_a.find("Flu Game") == std::string::npos);

    TimelineOptions metal;
    metal.pillar = Pillar::Metal;
    CHECK(render_timeline(metal).find("HOOPS") == std::string::npos);

    TimelineOptions none;
    none.from = 3000;
    CHECK(render_timeline(none).find("no moments") != std::string::npos);
}
