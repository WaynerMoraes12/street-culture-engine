#include <doctest/doctest.h>

#include <ostream>
#include <string>
#include <vector>

#include "street/bars/phonetics.hpp"
#include "street/bars/rhyme.hpp"

namespace bars = street::bars;
using bars::Language;
using bars::RhymeKind;

namespace {

void check_family(const std::vector<std::string>& words, Language lang) {
    const std::string key = bars::rhyme_key(words.front(), lang);
    CAPTURE(words.front());
    CHECK_FALSE(key.empty());
    for (const std::string& w : words) {
        CAPTURE(w);
        CHECK(bars::rhyme_key(w, lang) == key);
    }
}

} // namespace

TEST_CASE("English perfect rhymes share a key") {
    check_family({"game", "fame", "claim", "name", "frame"}, Language::English);
    check_family({"flow", "show", "though", "glow", "pro"}, Language::English);
    check_family({"night", "bite", "height", "light", "tonight"}, Language::English);
    check_family({"fire", "higher", "desire", "inspire"}, Language::English); // -ire ~ -igher
    check_family({"hour", "power", "flower"}, Language::English);
    check_family({"sneakers", "speakers"}, Language::English);
    check_family({"legendary", "necessary", "January"}, Language::English);
    check_family({"ballin'", "callin'"}, Language::English);
    check_family({"nation", "station", "creation"}, Language::English);
    check_family({"hustle", "muscle"}, Language::English);
    check_family({"trooper", "hooper", "super"}, Language::English);
    check_family({"boombox", "box", "rocks"}, Language::English);
    check_family({"cassette", "net", "jet"}, Language::English);
    check_family({"lebron", "upon"}, Language::English);
}

TEST_CASE("Portuguese perfect rhymes share a key") {
    check_family({"coração", "razão", "pressão", "campeão", "estão"}, Language::Portuguese);
    check_family({"falam", "calam"}, Language::Portuguese); // unstressed -am sounds like -ão
    CHECK(bars::pronounce("falam", Language::Portuguese).phonemes == "falAw");
    check_family({"quadra", "enquadra"}, Language::Portuguese);
    check_family({"jogo", "fogo", "logo"}, Language::Portuguese);
    check_family({"rima", "clima", "autoestima"}, Language::Portuguese);
    check_family({"gente", "frente", "quente", "mente"}, Language::Portuguese);
    check_family({"também", "ninguém", "além", "trem"}, Language::Portuguese);
    check_family({"Brasil", "partiu", "saiu"}, Language::Portuguese); // vocalised l
    check_family({"paz", "atrás"}, Language::Portuguese);             // final z sounds like s
    check_family({"história", "memória", "vitória"}, Language::Portuguese);
    check_family({"oitenta", "setenta", "noventa", "cinquenta"}, Language::Portuguese);
    check_family({"cassete", "basquete"}, Language::Portuguese);
}

TEST_CASE("clearly different words do not rhyme") {
    CHECK(bars::classify_rhyme("game", "flow", Language::English) == RhymeKind::None);
    CHECK(bars::classify_rhyme("night", "show", Language::English) == RhymeKind::None);
    CHECK(bars::classify_rhyme("basketball", "sneakers", Language::English) == RhymeKind::None);
    CHECK(bars::classify_rhyme("fire", "game", Language::English) == RhymeKind::None);
    CHECK(bars::classify_rhyme("money", "city", Language::English) == RhymeKind::None);
    CHECK(bars::classify_rhyme("rima", "jogo", Language::Portuguese) == RhymeKind::None);
    CHECK(bars::classify_rhyme("coração", "quadra", Language::Portuguese) == RhymeKind::None);
    CHECK(bars::classify_rhyme("fogo", "fé", Language::Portuguese) == RhymeKind::None);
}

TEST_CASE("slant rhymes share vowels but not consonants") {
    CHECK(bars::classify_rhyme("game", "rain", Language::English) == RhymeKind::Slant);
    CHECK(bars::classify_rhyme("night", "time", Language::English) == RhymeKind::Slant);
    CHECK(bars::classify_rhyme("sneakers", "dreamers", Language::English) == RhymeKind::Slant);
    CHECK(bars::classify_rhyme("quadra", "casa", Language::Portuguese) == RhymeKind::Slant);
    CHECK(bars::classify_rhyme("carro", "caro", Language::Portuguese) == RhymeKind::Slant);
    CHECK(bars::classify_rhyme("game", "fame", Language::English) == RhymeKind::Perfect);
    CHECK(bars::classify_rhyme("kobe", "homie", Language::English) == RhymeKind::Slant);
}

TEST_CASE("a word never rhymes with itself") {
    CHECK(bars::classify_rhyme("game", "Game", Language::English) == RhymeKind::None);
    CHECK(bars::classify_rhyme("jogo", "jogo", Language::Portuguese) == RhymeKind::None);
    CHECK(bars::rhyme_kind_name(RhymeKind::Perfect) == "perfect");
}

TEST_CASE("multisyllabic tails line up vowel by vowel") {
    const auto legendary = bars::pronounce("legendary", Language::English).vowels();
    const auto necessary = bars::pronounce("necessary", Language::English).vowels();
    CHECK(bars::matching_tail(legendary, necessary, Language::English) == 4);

    const auto memoria = bars::pronounce("memória", Language::Portuguese).vowels();
    const auto vitoria = bars::pronounce("vitória", Language::Portuguese).vowels();
    CHECK(bars::matching_tail(memoria, vitoria, Language::Portuguese) == 4); // e ~ i unstressed

    const auto game = bars::pronounce("game", Language::English).vowels();
    const auto flow = bars::pronounce("flow", Language::English).vowels();
    CHECK(bars::matching_tail(game, flow, Language::English) == 0);

    CHECK(bars::similar_vowels("@", "i", Language::English));
    CHECK_FALSE(bars::similar_vowels("A", "E", Language::English));
    CHECK(bars::similar_vowels("o", "u", Language::Portuguese));
    CHECK_FALSE(bars::similar_vowels("Aw", "a", Language::Portuguese));
}
