#include <doctest/doctest.h>

#include <algorithm>
#include <ostream>
#include <set>
#include <string>

#include "street/bars/lexicon.hpp"

namespace bars = street::bars;
using bars::Language;
using bars::RhymeKind;

namespace {

bool contains(const std::vector<std::string>& words, std::string_view w) {
    return std::find(words.begin(), words.end(), w) != words.end();
}

} // namespace

TEST_CASE("the lexicon is large and has no duplicates") {
    for (const Language lang : {Language::English, Language::Portuguese}) {
        const auto words = bars::lexicon(lang);
        CHECK(words.size() >= 300);
        const std::set<std::string_view> unique(words.begin(), words.end());
        CHECK(unique.size() == words.size());
    }
}

TEST_CASE("English rhymes: perfect first, then slant") {
    const auto matches = bars::find_rhymes("game", Language::English, 30);
    REQUIRE_FALSE(matches.empty());
    CHECK(matches.front().kind == RhymeKind::Perfect);
    CHECK(std::is_sorted(matches.begin(), matches.end(), [](const auto& a, const auto& b) {
        return static_cast<int>(a.kind) > static_cast<int>(b.kind);
    }));
    const auto words = bars::rhymes_for("game", Language::English, 30);
    CHECK(contains(words, "fame"));
    CHECK(contains(words, "name"));
    CHECK(contains(words, "rain")); // slant
    CHECK_FALSE(contains(words, "game"));
}

TEST_CASE("multisyllabic matches rank higher") {
    const auto matches = bars::find_rhymes("legendary", Language::English, 5);
    REQUIRE_FALSE(matches.empty());
    CHECK(matches.front().syllables_matched >= 3);
}

TEST_CASE("Portuguese rhymes and auto detection") {
    const auto words = bars::rhymes_for("coração", Language::Portuguese, 40);
    CHECK(contains(words, "razão"));
    CHECK(contains(words, "pressão"));
    CHECK(bars::rhymes_for("coração", Language::Auto, 40) == words);
    CHECK(contains(bars::rhymes_for("quadra", Language::Portuguese, 40), "jogada")); // slant a-a
}

TEST_CASE("legends are in the lexicon and rhyme back") {
    CHECK(contains(bars::rhymes_for("trooper", Language::English), "hooper"));
    CHECK(contains(bars::rhymes_for("boombox", Language::English), "box"));
    CHECK(contains(bars::rhymes_for("kobe", Language::English), "homie"));
    CHECK(contains(bars::rhymes_for("lebron", Language::English), "upon"));
    CHECK(contains(bars::rhymes_for("oitenta", Language::Portuguese), "noventa"));
    const auto words = bars::lexicon(Language::English);
    for (std::string_view legend : {"jordan", "kobe", "mamba", "iverson", "detroit", "maiden"})
        CHECK(std::find(words.begin(), words.end(), legend) != words.end());
}

TEST_CASE("limits and words without rhymes") {
    CHECK(bars::rhymes_for("game", Language::English, 3).size() == 3);
    CHECK(bars::rhymes_for("game", Language::English, 0).empty());
    CHECK(bars::find_rhymes("hmm", Language::English).empty());
}
