#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "street/bars/analysis.hpp"
#include "street/bars/ghostwriter.hpp"
#include "street/core/text.hpp"

namespace bars = street::bars;
using bars::Language;

namespace {

/// A request with every field spelled out (GCC warns on partially designated aggregates).
bars::VerseRequest request(std::uint64_t seed, int lines, Language lang = Language::English,
                           std::string subject = {}, std::string team = {}, std::string shoe = {},
                           int points = 0) {
    bars::VerseRequest r;
    r.subject = std::move(subject);
    r.team = std::move(team);
    r.shoe = std::move(shoe);
    r.points = points;
    r.language = lang;
    r.seed = seed;
    r.lines = lines;
    return r;
}

std::vector<const bars::Line*> bars_of(const bars::Analysis& a) {
    std::vector<const bars::Line*> out;
    for (const bars::Line& line : a.lines)
        if (!line.is_break()) out.push_back(&line);
    return out;
}

} // namespace

TEST_CASE("ghostwriting is deterministic in its request") {
    bars::VerseRequest req = request(7, 8, Language::English, "Jay", "Uptown", "Retro 4s", 41);
    const std::string verse = bars::ghostwrite(req);
    CHECK(verse == bars::ghostwrite(req));

    int different = 0;
    for (std::uint64_t seed = 8; seed < 18; ++seed) {
        req.seed = seed;
        if (bars::ghostwrite(req) != verse) ++different;
    }
    CHECK(different == 10);
}

TEST_CASE("line counts, stanzas and slots") {
    for (int lines = 2; lines <= 13; ++lines) {
        const auto a = bars::analyze(bars::ghostwrite(request(3, lines)));
        CHECK(a.bars == lines);
    }
    CHECK(bars::analyze(bars::ghostwrite(request(1, 100))).bars == 32); // clamped
    CHECK(bars::analyze(bars::ghostwrite(request(1, 8))).scheme.find(' ') == 4);

    // Long enough to use every opener, so every slot shows up.
    const std::string en =
        bars::ghostwrite(request(1, 24, Language::English, "Jay", "Uptown", "Retro 4s", 41));
    CHECK(en.find("Jay") != std::string::npos);
    CHECK(en.find("Uptown") != std::string::npos);
    CHECK(en.find("Retro 4s") != std::string::npos);
    CHECK(en.find("41") != std::string::npos);
    CHECK(en.find('{') == std::string::npos);

    const std::string pt =
        bars::ghostwrite(request(1, 20, Language::Portuguese, "Jota", "Leões", "Air Max", 33));
    CHECK(pt.find("Jota") != std::string::npos);
    CHECK(pt.find("Leões") != std::string::npos);
    CHECK(pt.find("Air Max") != std::string::npos);
    CHECK(pt.find("33") != std::string::npos);
    CHECK(bars::detect_language(pt) == Language::Portuguese);
}

TEST_CASE("the analyzer hears every ghostwritten couplet rhyme") {
    for (const Language lang : {Language::English, Language::Portuguese}) {
        for (const int lines : {4, 7, 8, 16}) {
            for (std::uint64_t seed = 1; seed <= 40; ++seed) {
                const std::string verse = bars::ghostwrite(request(seed, lines, lang, "Jay"));
                const bars::Analysis a = bars::analyze(verse, lang);
                const auto rows = bars_of(a);
                REQUIRE(static_cast<int>(rows.size()) == lines);
                CAPTURE(verse);
                for (int c = 0; c + 1 < lines; c += 2) {
                    const bars::Line& first = *rows[static_cast<std::size_t>(c)];
                    const bars::Line& second = *rows[static_cast<std::size_t>(c + 1)];
                    CHECK(first.scheme == second.scheme);
                    CHECK(first.words.back().group >= 0);
                    CHECK(first.words.back().group == second.words.back().group);
                    if (c >= 2) CHECK(rows[static_cast<std::size_t>(c - 1)]->scheme != first.scheme);
                }
                if (lines % 2 == 1) CHECK(rows[rows.size() - 1]->scheme == rows[rows.size() - 2]->scheme);
            }
        }
    }
}

TEST_CASE("sample verses are original, structured and in the right language") {
    for (const Language lang : {Language::English, Language::Portuguese}) {
        const std::string_view verse = bars::sample_verse(lang);
        const bars::Analysis a = bars::analyze(verse);
        CHECK(a.language == lang);
        CHECK(a.bars >= 12);
        CHECK(a.bars <= 16);
        CHECK(std::count(a.scheme.begin(), a.scheme.end(), ' ') >= 2); // stanzas
    }
    CHECK(bars::sample_verse(Language::Auto) == bars::sample_verse(Language::English));
    CHECK(bars::sample_verse(bars::Sample::Portuguese) == bars::sample_verse(Language::Portuguese));
    CHECK(bars::parse_sample("tribute") == bars::Sample::Tribute);
    CHECK(bars::parse_sample("PT") == bars::Sample::Portuguese);
    CHECK(bars::parse_sample("en") == bars::Sample::English);
    CHECK_FALSE(bars::parse_sample("auto").has_value());
    CHECK(bars::sample_language(bars::Sample::Tribute) == Language::English);
}
