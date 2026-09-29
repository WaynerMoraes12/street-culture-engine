#include <doctest/doctest.h>

#include <ostream>

#include "street/bars/ghostwriter.hpp"
#include "street/bars/language.hpp"

namespace bars = street::bars;
using bars::Language;

TEST_CASE("language detection on verses") {
    CHECK(bars::detect_language(bars::sample_verse(Language::English)) == Language::English);
    CHECK(bars::detect_language(bars::sample_verse(Language::Portuguese)) == Language::Portuguese);
    CHECK(bars::detect_language("I got the game on lock and you know it") == Language::English);
    CHECK(bars::detect_language("Eu tenho o jogo na mão e você sabe") == Language::Portuguese);
    CHECK(bars::detect_language("Não para, não para") == Language::Portuguese);
}

TEST_CASE("language detection on single words uses orthography") {
    CHECK(bars::detect_language("coração") == Language::Portuguese);
    CHECK(bars::detect_language("pressão") == Language::Portuguese);
    CHECK(bars::detect_language("sneakers") == Language::English);
    CHECK(bars::detect_language("throwback") == Language::English);
    CHECK(bars::detect_language("") == Language::English); // no cue: default
}

TEST_CASE("resolve, names and parsing") {
    CHECK(bars::resolve_language(Language::Portuguese, "the game") == Language::Portuguese);
    CHECK(bars::resolve_language(Language::Auto, "o jogo é nosso") == Language::Portuguese);
    CHECK(bars::language_code(Language::English) == "en");
    CHECK(bars::language_name(Language::Portuguese) == "Portuguese");
    CHECK(bars::parse_language("EN") == Language::English);
    CHECK(bars::parse_language("pt-BR") == Language::Portuguese);
    CHECK(bars::parse_language("português") == Language::Portuguese);
    CHECK(bars::parse_language("auto") == Language::Auto);
    CHECK_FALSE(bars::parse_language("klingon").has_value());
}

TEST_CASE("function words") {
    CHECK(bars::is_function_word("the", Language::English));
    CHECK_FALSE(bars::is_function_word("game", Language::English));
    CHECK(bars::is_function_word("que", Language::Portuguese));
    CHECK_FALSE(bars::is_function_word("quadra", Language::Portuguese));
}
