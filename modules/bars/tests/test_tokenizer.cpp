#include <doctest/doctest.h>

#include <ostream>
#include <string>
#include <vector>

#include "street/bars/tokenizer.hpp"

namespace bars = street::bars;

namespace {

std::vector<std::string> words(std::string_view line) {
    std::vector<std::string> out;
    for (const bars::Token& t : bars::split_words(line)) out.push_back(t.text);
    return out;
}

} // namespace

TEST_CASE("apostrophes stay inside words, punctuation goes") {
    CHECK(words("Don't stop, I'm ballin' like it's '96!") ==
          std::vector<std::string>{"Don't", "stop", "I'm", "ballin'", "like", "it's", "96"});
    CHECK(words("the players' lounge") == std::vector<std::string>{"the", "players'", "lounge"});
    CHECK(words("'cause 'em 'til") == std::vector<std::string>{"'cause", "'em", "'til"});
    CHECK(words("she said 'hello' twice") ==
          std::vector<std::string>{"she", "said", "hello", "twice"});
}

TEST_CASE("curly apostrophes are normalised") {
    CHECK(words("don’t trip") == std::vector<std::string>{"don't", "trip"});
    CHECK(bars::normalize_word("Don’T") == "don't");
    CHECK(bars::normalize_word("CORAÇÃO") == "coração");
}

TEST_CASE("hyphens join compounds, dashes separate") {
    CHECK(words("hip-hop pick-and-roll") == std::vector<std::string>{"hip-hop", "pick-and-roll"});
    CHECK(words("fast -- break") == std::vector<std::string>{"fast", "break"});
    CHECK(words("fim- do jogo") == std::vector<std::string>{"fim", "do", "jogo"});
}

TEST_CASE("UTF-8 words and byte offsets") {
    const std::string line = "Coração, não! (23 pontos)";
    const auto tokens = bars::split_words(line);
    REQUIRE(tokens.size() == 4);
    CHECK(tokens[0].text == "Coração");
    CHECK(tokens[1].text == "não");
    CHECK(tokens[2].text == "23");
    for (const auto& t : tokens) CHECK(line.substr(t.begin, t.end - t.begin) == t.text);
}

TEST_CASE("verses split into lines with stanza breaks") {
    const auto lines = bars::tokenize("\n\nfirst bar\r\nsecond bar\n\n\n\nthird bar\n...\nfourth\n\n");
    REQUIRE(lines.size() == 6);
    CHECK(lines[0].text == "first bar");
    CHECK(lines[1].text == "second bar");
    CHECK(lines[2].is_break());
    CHECK(lines[3].text == "third bar");
    CHECK(lines[4].is_break()); // a line of pure punctuation also separates stanzas
    CHECK(lines[5].text == "fourth");
}

TEST_CASE("section headers are kept as stanza separators without words") {
    const auto lines = bars::tokenize("[Verse 1]\nline one\n\n[Hook]\nline two\n");
    REQUIRE(lines.size() == 4);
    CHECK(lines[0].header);
    CHECK(lines[0].tokens.empty());
    CHECK(lines[1].text == "line one");
    CHECK(lines[2].header); // the blank line before a header is absorbed
    CHECK(lines[2].text == "[Hook]");
    CHECK(lines[3].tokens.size() == 2);
}

TEST_CASE("empty input yields no lines") {
    CHECK(bars::tokenize("").empty());
    CHECK(bars::tokenize("\n \n\t\n").empty());
    CHECK(bars::split_words("?!...").empty());
}
