#include <doctest/doctest.h>

#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "street/bars/phonetics.hpp"

namespace bars = street::bars;
using bars::Language;

namespace {

struct Expected {
    std::string_view word;
    int syllables;
};

/// Counts how many words the heuristic gets right and reports the misses, so the suite
/// documents accuracy instead of hiding it behind a handful of easy cases.
double accuracy(const std::vector<Expected>& words, Language lang) {
    int right = 0;
    for (const auto& [word, expected] : words) {
        const int got = bars::count_syllables(word, lang);
        if (got == expected)
            ++right;
        else
            MESSAGE(word << ": expected " << expected << ", got " << got);
    }
    const double score = static_cast<double>(right) / static_cast<double>(words.size());
    MESSAGE("syllable accuracy: " << right << "/" << words.size());
    return score;
}

} // namespace

// Reference counts follow standard dictionaries (Merriam-Webster), e.g. "fire" = 1.
TEST_CASE("English syllables: rap, hoops and sneaker vocabulary") {
    const std::vector<Expected> words = {
        {"basketball", 3}, {"sneakers", 2},  {"rhythm", 2},     {"fire", 1},
        {"crossover", 3},  {"legendary", 4}, {"hoops", 1},      {"dribble", 2},
        {"rebound", 2},    {"championship", 4}, {"jumper", 2},  {"buzzer", 2},
        {"microphone", 3}, {"freestyle", 2}, {"hustle", 2},     {"beautiful", 3},
        {"everybody", 4},  {"sneakerhead", 3}, {"playoffs", 2}, {"trophy", 2},
        {"rookie", 2},     {"veteran", 3},   {"arena", 3},      {"stadium", 3},
        {"lyrics", 2},     {"poetry", 3},    {"cypher", 2},     {"hip-hop", 2},
        {"bouncing", 2},   {"ballin'", 2},   {"jordans", 2},    {"retro", 2},
        {"leather", 2},    {"suede", 1},     {"laces", 2},      {"concrete", 2},
        {"mixtape", 2},    {"recorded", 3},  {"alley-oop", 3},  {"layup", 2},
        {"overtime", 3},   {"dynasty", 3},   {"pressure", 2},   {"knowledge", 2},
        {"champion", 3},   {"legend", 2},    {"higher", 2},     {"desire", 2},
        {"power", 2},      {"hour", 1},      {"time", 1},       {"games", 1},
        {"played", 1},     {"wanted", 2},    {"places", 2},     {"boxes", 2},
        {"little", 2},     {"table", 2},     {"people", 2},     {"smile", 1},
        {"believe", 2},    {"creation", 3},  {"science", 2},    {"idea", 3},
        {"quiet", 2},      {"being", 2},     {"radio", 3},      {"video", 3},
        {"family", 3},     {"business", 2},  {"don't", 1},      {"didn't", 2},
        {"amazing", 3},    {"whatever", 3},  {"together", 3},   {"nation", 2},
        {"50", 2},         {"23", 3},        {"through", 1},    {"flow", 1},
    };
    REQUIRE(words.size() >= 40);
    CHECK(accuracy(words, Language::English) >= 0.9);
}

// Portuguese counts are grammatical (his-tó-ri-a = 4), the way schools split syllables.
TEST_CASE("Portuguese syllables: diphthongs, hiatus and silent u") {
    const std::vector<Expected> words = {
        {"basquete", 3},  {"tênis", 2},     {"coração", 3},   {"quadra", 2},
        {"saudade", 3},   {"história", 4},  {"saúde", 3},     {"país", 2},
        {"razão", 2},     {"pressão", 2},   {"enquadra", 3},  {"jogo", 2},
        {"fogo", 2},      {"rima", 2},      {"clima", 2},     {"guerra", 2},
        {"quero", 2},     {"aquele", 3},    {"enquanto", 3},  {"água", 2},
        {"rainha", 3},    {"juiz", 2},      {"ruim", 2},      {"muito", 2},
        {"cheio", 2},     {"praia", 2},     {"ideia", 3},     {"mãe", 1},
        {"pão", 1},       {"corações", 3},  {"sair", 2},      {"saiu", 2},
        {"ainda", 3},     {"baixo", 2},     {"noite", 2},     {"série", 3},
        {"tatuagem", 4},  {"jogador", 3},   {"arremesso", 4}, {"enterrada", 4},
        {"rebote", 3},    {"garrafão", 3},  {"campeão", 3},   {"leão", 2},
        {"voo", 2},       {"também", 2},    {"ninguém", 2},   {"homem", 2},
        {"Brasil", 2},    {"futebol", 3},   {"cesta", 2},     {"torcida", 3},
        {"madrugada", 4}, {"vitória", 4},   {"microfone", 4}, {"pisante", 3},
        {"cadarço", 3},   {"quebrada", 3},
    };
    REQUIRE(words.size() >= 30);
    CHECK(accuracy(words, Language::Portuguese) >= 0.9);
}

TEST_CASE("transcriptions expose the pieces the analyzer uses") {
    const bars::Pronunciation night = bars::pronounce("night", Language::English);
    CHECK(night.phonemes == "nIt");
    CHECK(night.syllables() == 1);
    CHECK(night.rhyme_key() == "It");

    const bars::Pronunciation crossover = bars::pronounce("crossover", Language::English);
    CHECK(crossover.vowels() == std::vector<std::string>{"o", "O", "R"});
    CHECK(crossover.rhyme_key() == "OvR"); // stress steps back over the weak -er

    const bars::Pronunciation coracao = bars::pronounce("coração", Language::Portuguese);
    CHECK(coracao.phonemes == "korasAw");
    CHECK(coracao.stress == 2);
    CHECK(coracao.vowel_skeleton() == "Aw");

    // Paroxytone default (-a) vs. oxytone (-r), and the accent that overrides both.
    CHECK(bars::pronounce("quadra", Language::Portuguese).stress == 0);
    CHECK(bars::pronounce("jogador", Language::Portuguese).stress == 2);
    CHECK(bars::pronounce("história", Language::Portuguese).stress == 1);
}

TEST_CASE("legends and era vocabulary") {
    const std::vector<Expected> words = {
        {"Kobe", 2},     {"LeBron", 2},   {"Iverson", 3},  {"Eminem", 3},  {"Detroit", 2},
        {"boombox", 2},  {"cassette", 2}, {"Mamba", 2},    {"maiden", 2},  {"trooper", 2},
        {"galloping", 3}, {"eighties", 2}, {"Iguodala", 4}, {"Run-DMC", 4}, {"comeback", 2},
    };
    CHECK(accuracy(words, Language::English) == doctest::Approx(1.0));
}

TEST_CASE("edge cases do not crash and count sensibly") {
    CHECK(bars::count_syllables("", Language::English) == 0);
    CHECK(bars::count_syllables("hmm", Language::English) == 0);
    CHECK(bars::count_syllables("'til", Language::English) == 1);
    CHECK(bars::count_syllables("1996", Language::English) == 5); // nineteen ninety six
    CHECK(bars::count_syllables("MVP", Language::English) == 3);
    CHECK(bars::count_syllables("coração", Language::Auto) == 3);
    CHECK(bars::rhyme_key("", Language::Portuguese).empty());
}

TEST_CASE("sound labels are readable") {
    CHECK(bars::sound_label("ER", Language::English) == "ee-er");
    CHECK(bars::sound_label("A", Language::English) == "ay");
    CHECK(bars::sound_label("Aw", Language::Portuguese) == "ão");
    CHECK(bars::sound_label("aa", Language::Portuguese) == "a-a");
    CHECK(bars::sound_label("oia", Language::Portuguese) == "o-i-a");
}
