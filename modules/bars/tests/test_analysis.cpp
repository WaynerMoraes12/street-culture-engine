#include <doctest/doctest.h>

#include <ostream>
#include <string>

#include "street/bars/analysis.hpp"
#include "street/bars/ghostwriter.hpp"

namespace bars = street::bars;
using bars::Language;

TEST_CASE("end-rhyme schemes: couplets, alternating, stanzas") {
    CHECK(bars::analyze("I got the game\nThey know my name\nI run the night\nI hold the light\n")
              .scheme == "AABB");
    CHECK(bars::analyze("I got the game\nI run the night\nThey know my name\nI hold the light\n")
              .scheme == "ABAB");
    CHECK(bars::analyze("I got the game\nThey know my name\n\nI run the night\nI hold the light\n")
              .scheme == "AA BB");
    // Letters continue across stanzas and come back when a rhyme returns.
    CHECK(bars::analyze("the game\nmy name\n\nthe night\nthe light\n\nthe flame\nthe fame")
              .scheme == "AA BB AA");
}

TEST_CASE("scheme letters live on each line") {
    const bars::Analysis a = bars::analyze("I got the game\n\nThey know my name\n");
    REQUIRE(a.lines.size() == 3);
    CHECK(a.lines[0].scheme == 'A');
    CHECK(a.lines[1].is_break());
    CHECK(a.lines[1].scheme == ' ');
    CHECK(a.lines[2].scheme == 'A');
    CHECK(a.lines[2].stanza == 1);
}

TEST_CASE("repeating a word is not a rhyme, but keeps the scheme letter") {
    const bars::Analysis a = bars::analyze("I love the game\nI love the game\n");
    CHECK(a.scheme == "AA");
    CHECK(a.rhyme_groups == 0);
    CHECK(a.rhymed_syllables == 0);
    CHECK(a.rhyme_density == doctest::Approx(0.0));
}

TEST_CASE("internal rhymes join families and count") {
    const bars::Analysis a = bars::analyze("Tight fight in the late night\nwe keep it moving\n");
    CHECK(a.internal_rhymes >= 2);
    REQUIRE_FALSE(a.families.empty());
    const auto& words = a.lines[0].words;
    CHECK(words[0].group >= 0);
    CHECK(words[0].group == words[1].group); // tight / fight
    CHECK(words[0].group == words[5].group); // ... night
    CHECK(words[2].group == -1);             // "in" is a function word
    CHECK(a.families[0].sound == "igh");
}

TEST_CASE("multisyllabic rhymes are measured across words") {
    const bars::Analysis a =
        bars::analyze("my story's legendary\nthis glory's necessary\n", Language::English);
    CHECK(a.longest_multisyllabic == 6); // (st)ory's legendary / (gl)ory's necessary
    CHECK(a.lines[0].multisyllabic == a.longest_multisyllabic);
    CHECK(a.longest_multisyllabic_words[0].find("legendary") != std::string::npos);
    CHECK(a.longest_multisyllabic_words[1].find("necessary") != std::string::npos);
    CHECK(a.rhyme_density > 0.5);

    // Repeated words stop the walk: "is" / "is" is repetition, not rhyme.
    CHECK(bars::analyze("it is legendary\nit is necessary\n").longest_multisyllabic == 4);
}

TEST_CASE("word and flow metrics") {
    const bars::Analysis a = bars::analyze("one two three four\none two three more\n");
    CHECK(a.language == Language::English);
    CHECK(a.bars == 2);
    CHECK(a.total_words == 8);
    CHECK(a.unique_words == 5);
    CHECK(a.vocabulary_richness == doctest::Approx(5.0 / 8.0));
    CHECK(a.total_syllables == 8);
    CHECK(a.lines[0].syllables == 4);
    CHECK(a.syllables_per_bar() == doctest::Approx(4.0));
    // One bar of 4/4 at 120 BPM lasts two seconds.
    CHECK(a.syllables_per_second(120) == doctest::Approx(2.0));
    CHECK(a.syllables_per_second(0) == doctest::Approx(0.0));
    CHECK(a.scheme == "AA"); // four / more
    CHECK(a.rhyme_density == doctest::Approx(2.0 / 8.0));
}

TEST_CASE("words carry their pronunciation and position") {
    const bars::Analysis a = bars::analyze("Coração na mão!", Language::Auto);
    CHECK(a.language == Language::Portuguese);
    const auto& words = a.lines[0].words;
    REQUIRE(words.size() == 3);
    CHECK(words[0].syllables == 3);
    CHECK(words[0].key == "Aw");
    CHECK(words[0].group == words[2].group);
    CHECK(a.lines[0].text.substr(words[2].begin, words[2].end - words[2].begin) == "mão");
}

TEST_CASE("headers and empty input") {
    const bars::Analysis a = bars::analyze("[Verse]\nthe game\nmy name\n[Hook]\nthe night\nthe light");
    CHECK(a.scheme == "AA BB");
    CHECK(a.lines.front().header);

    const bars::Analysis empty = bars::analyze("");
    CHECK(empty.bars == 0);
    CHECK(empty.scheme.empty());
    CHECK(empty.rhyme_density == doctest::Approx(0.0));
    CHECK(empty.syllables_per_second(90) == doctest::Approx(0.0));
}

TEST_CASE("the shipped sample verses score like real bars") {
    const bars::Analysis en = bars::analyze(bars::sample_verse(Language::English));
    CHECK(en.language == Language::English);
    CHECK(en.bars == 16);
    CHECK(en.scheme == "AABB CCDD EEFF GGGG");
    CHECK(en.rhyme_density > 0.3);
    CHECK(en.longest_multisyllabic >= 4);
    CHECK(en.internal_rhymes >= 8);

    const bars::Analysis pt = bars::analyze(bars::sample_verse(Language::Portuguese));
    CHECK(pt.language == Language::Portuguese);
    CHECK(pt.bars == 16);
    CHECK(pt.scheme == "AABB CCDD EEBB FFGG"); // "ão" returns in the third stanza
    CHECK(pt.rhyme_density > 0.3);
    CHECK(pt.longest_multisyllabic >= 4);
    CHECK(pt.internal_rhymes >= 8);
}

TEST_CASE("the tribute verse rolls through the legends with tight rhymes") {
    const std::string_view verse = bars::sample_verse(bars::Sample::Tribute);
    for (std::string_view name : {"Jordan", "Kobe", "LeBron", "Iverson", "Eminem", "Detroit",
                                  "Bronx", "Run-DMC", "Iron Maiden", "Eddie", "The Trooper"})
        CHECK(verse.find(name) != std::string_view::npos);

    const bars::Analysis a = bars::analyze(verse);
    MESSAGE("tribute: " << a.scheme << ", density " << a.rhyme_density << ", longest multi "
                        << a.longest_multisyllabic);
    CHECK(a.language == Language::English);
    CHECK(a.bars == 16);
    CHECK(a.scheme == "AABB CCDD EEFF GGHH");
    CHECK(a.rhyme_density > 0.3);
    CHECK(a.longest_multisyllabic >= 3);
    CHECK(a.internal_rhymes >= 10);
}
