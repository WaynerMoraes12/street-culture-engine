#include "street/beats/theory.hpp"

#include <doctest/doctest.h>
#include <numeric>
#include <stdexcept>
#include <string>

using namespace street::beats;

TEST_CASE("equal-temperament frequencies") {
    CHECK(midi_to_hz(69) == doctest::Approx(440.0));
    CHECK(midi_to_hz(57) == doctest::Approx(220.0));
    CHECK(midi_to_hz(60) == doctest::Approx(261.6256).epsilon(1e-6));
    CHECK(midi_to_hz(28) == doctest::Approx(41.2034).epsilon(1e-5)); // E1, bottom of the 808 range
}

TEST_CASE("note names wrap and follow the spelling preference") {
    CHECK(note_name(1, false) == "C#");
    CHECK(note_name(1, true) == "Db");
    CHECK(note_name(-2, true) == "Bb");
    CHECK(note_name(23, false) == "B");
}

TEST_CASE("parse_key understands the common spellings") {
    struct Case {
        const char* text;
        int tonic;
        bool minor;
        const char* name;
    };
    const Case cases[] = {
        {"C#m", 1, true, "C#m"},    {"F", 5, false, "F"},       {"Ebm", 3, true, "Ebm"},
        {"Bb", 10, false, "Bb"},    {"a minor", 9, true, "Am"}, {"gmin", 7, true, "Gm"},
        {"F# maj", 6, false, "F#"}, {" dm ", 2, true, "Dm"},    {"E major", 4, false, "E"},
    };
    for (const Case& c : cases) {
        CAPTURE(c.text);
        const auto key = parse_key(c.text);
        REQUIRE(key.has_value());
        CHECK(key->tonic == c.tonic);
        CHECK(key->minor == c.minor);
        CHECK(key->name() == c.name);
    }
    CHECK(parse_key("Gm")->flats);  // G minor has two flats
    CHECK(!parse_key("Em")->flats); // E minor has one sharp
    CHECK(parse_key("Cm")->long_name() == "C minor");
    for (const char* bad : {"", "H", "Cx", "Am7", "minor", "C##"}) {
        CAPTURE(bad);
        CHECK_FALSE(parse_key(bad).has_value());
    }
}

TEST_CASE("chord symbols parse into root, intervals and canonical names") {
    const Chord am7 = parse_chord("Am7");
    CHECK(am7.root == 9);
    CHECK(am7.intervals == std::vector<int>{0, 3, 7, 10});
    CHECK(am7.name == "Am7");

    CHECK(parse_chord("F#m9").intervals == std::vector<int>{0, 3, 7, 10, 14});
    CHECK(parse_chord("Bbmaj7").root == 10);
    CHECK(parse_chord("Bbmaj7").name == "Bbmaj7");
    CHECK(parse_chord("Dm7b5").intervals == std::vector<int>{0, 3, 6, 10});
    CHECK(parse_chord("Cmin7").name == "Cm7"); // aliases are canonicalised
    CHECK(parse_chord("E").intervals == std::vector<int>{0, 4, 7});
    CHECK(parse_chord("E5").intervals == std::vector<int>{0, 7}); // power chord
    CHECK(parse_chord("F#5").voicing().size() == 3);              // root, fifth, octave
    CHECK(make_chord(1, "m", true).name == "Dbm");

    CHECK_THROWS_AS(parse_chord("Xm"), std::invalid_argument);
    CHECK_THROWS_AS(make_chord(0, "wobble", false), std::invalid_argument);
    try {
        parse_chord("Cfoo");
        FAIL("expected an exception");
    } catch (const std::invalid_argument& e) {
        CHECK(std::string(e.what()).find("unknown quality 'foo'") != std::string::npos);
    }
}

TEST_CASE("voicings: 808 register, keys around middle C, rootless extended chords") {
    for (int root = 0; root < 12; ++root) {
        const Chord c = make_chord(root, "m9", false);
        CHECK(c.bass_note() >= 28);
        CHECK(c.bass_note() <= 39);
        CHECK(c.bass_note() % 12 == root);
        const auto notes = c.voicing();
        REQUIRE(notes.size() == 4); // m9 without its root
        for (int n : notes)
            CHECK(n % 12 != root);
        const double centre = std::accumulate(notes.begin(), notes.end(), 0.0) / 4.0;
        CHECK(centre == doctest::Approx(60.0).epsilon(0.11)); // within ~6 semitones
    }
    const auto triad = make_chord(0, "", false).voicing();
    CHECK(triad.size() == 4); // triads are spread with the octave on top
    CHECK(triad.back() - triad.front() == 12);
}
