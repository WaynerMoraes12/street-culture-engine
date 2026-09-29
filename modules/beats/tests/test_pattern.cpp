#include "street/beats/pattern.hpp"

#include <doctest/doctest.h>
#include <stdexcept>
#include <string>

using namespace street::beats;

namespace {

/// Asserts that parsing fails with a message containing every fragment.
void check_error(const std::string& text, std::initializer_list<const char*> fragments) {
    try {
        parse_pattern(text);
        FAIL("expected std::invalid_argument for: " << text);
    } catch (const std::invalid_argument& e) {
        const std::string message = e.what();
        for (const char* f : fragments) {
            CAPTURE(message);
            CHECK(message.find(f) != std::string::npos);
        }
    }
}

} // namespace

TEST_CASE("a basic groove parses into tracks, steps and velocities") {
    const Pattern p = parse_pattern(R"(
# a comment line
kick  | x..o..x. ..o.x... |   # trailing comment
snare | ....x... ....x..g |
hat   | o.o.o.o. o.o.orrt |
)");
    CHECK(p.bars() == 1);
    REQUIRE(p.tracks.size() == 3);
    CHECK(p.tracks[0].instrument == Instrument::Kick); // display order, not file order
    const Track& kick = *p.find(Instrument::Kick);
    CHECK(kick.hits() == 5);
    CHECK(kick.steps[0].velocity == 1.0f);
    CHECK(kick.steps[3].velocity == doctest::Approx(0.72));
    CHECK_FALSE(kick.steps[1].active());
    CHECK(p.find(Instrument::Snare)->steps[15].velocity == doctest::Approx(0.35)); // ghost
    const Track& hat = *p.find(Instrument::HatClosed);
    CHECK(hat.steps[13].roll == 2);
    CHECK(hat.steps[15].roll == 3);
    CHECK(p.find(Instrument::Clap) == nullptr);
}

TEST_CASE("directives set title, tempo, swing, key and chords") {
    const Pattern p = parse_pattern("title  Late Pass\r\n"
                                    "bpm 92\r\n"
                                    "swing 62%\r\n"
                                    "key c#m\r\n"
                                    "style Trap\r\n"
                                    "chords C#m9 | F#m7 B9 Emaj7\r\n"
                                    "kick x... .... x... ....\r\n");
    CHECK(p.title == "Late Pass");
    CHECK(p.bpm == 92);
    CHECK(p.swing == doctest::Approx(0.62));
    CHECK(p.key == "C#m");
    CHECK(p.style == Style::Trap);
    REQUIRE(p.chords.size() == 4);
    CHECK(p.chords[2].name == "B9");
    CHECK(p.chord_at(5)->name == "F#m7"); // cycles per bar
    CHECK(parse_pattern("swing 0.58\nkick x...............").swing == doctest::Approx(0.58));
    const Pattern bare = parse_pattern("kick x...............");
    CHECK(bare.bpm == 0);
    CHECK(bare.swing < 0.0);
    CHECK(bare.chord_at(0) == nullptr);
    CHECK_FALSE(bare.style.has_value());
}

TEST_CASE("aliases, pitch marks and slides") {
    const Pattern p = parse_pattern("bd  x... .... .... ....\n"
                                    "808 x... ..^. ...~ ....\n"
                                    "lt  o... ^... .... ....\n"
                                    "oh  .... .... ..o. ....\n");
    CHECK(p.find(Instrument::Kick) != nullptr);
    CHECK(p.find(Instrument::HatOpen) != nullptr);
    const Track& bass = *p.find(Instrument::Bass);
    CHECK(bass.steps[6].note == 12);
    CHECK(bass.steps[6].slide);
    CHECK(bass.steps[11].note == 0);
    CHECK(bass.steps[11].slide);
    const Track& tom = *p.find(Instrument::Tom);
    CHECK(tom.steps[4].note == 12);
    CHECK_FALSE(tom.steps[4].slide); // only the bass glides
    const Pattern riff = parse_pattern("gtr x.xx *... .... ....\n"
                                       "kick *... .... .... ....");
    CHECK(riff.find(Instrument::Guitar)->steps[4].ring);
    CHECK_FALSE(riff.find(Instrument::Guitar)->steps[0].ring);
    CHECK_FALSE(riff.find(Instrument::Kick)->steps[0].ring); // only guitar chords ring
    CHECK(riff.find(Instrument::Kick)->steps[0].active());
}

TEST_CASE("multi-bar tracks, repeated names append, shorter tracks loop") {
    const Pattern p = parse_pattern("kick | x... .... .... .... | x.x. .... .... .... |\n"
                                    "hat  | o.o. o.o. o.o. o.o. |\n"
                                    "kick | .... x... .... .... |\n");
    CHECK(p.bars() == 3);
    const Track& kick = *p.find(Instrument::Kick);
    REQUIRE(kick.steps.size() == 48);
    CHECK(kick.steps[18].active());
    CHECK(kick.steps[36].active());
    const Track& hat = *p.find(Instrument::HatClosed);
    REQUIRE(hat.steps.size() == 48);
    CHECK(hat.steps[16 * 2 + 2] == hat.steps[2]); // looped
}

TEST_CASE("errors name the line and explain the fix") {
    check_error("kick x...............\n\nkik x...............",
                {"line 3", "unknown instrument", "kick"});
    check_error("kick x...q...........", {"line 1", "column 10", "'q'"});
    check_error("# header\nhat x.x.x.x.", {"line 2", "8 steps", "16"});
    check_error("bpm fast\nkick x...............", {"line 1", "bpm"});
    check_error("swing 0.95\nkick x...............", {"line 1", "swing"});
    check_error("key H\nkick x...............", {"line 1", "unknown key 'H'"});
    check_error("chords Am7 Q9\nkick x...............", {"line 1", "Q9"});
    check_error("title\nkick x...............", {"line 1", "needs a value"});
    check_error("style polka\nkick x...............", {"line 1", "unknown style 'polka'"});
    check_error("# nothing here\n\n", {"no tracks"});
    check_error("kick", {"line 1", "0 steps"});
}

TEST_CASE("Pattern keeps tracks in display order and resizes by looping") {
    Pattern p;
    p.resize(2);
    p.at(Instrument::Keys, 0, 0).velocity = 0.5f;
    p.at(Instrument::Kick, 1, 4).velocity = 1.0f;
    p.track(Instrument::Snare);
    REQUIRE(p.tracks.size() == 3);
    CHECK(p.tracks[0].instrument == Instrument::Kick);
    CHECK(p.tracks[1].instrument == Instrument::Snare);
    CHECK(p.tracks[2].instrument == Instrument::Keys);
    p.resize(4);
    CHECK(p.find(Instrument::Kick)->steps[3 * 16 + 4].active()); // bar 4 repeats bar 2
    CHECK_THROWS_AS(p.at(Instrument::Kick, 4, 0), std::out_of_range);
    CHECK_THROWS_AS(p.at(Instrument::Kick, 0, 16), std::out_of_range);
}

TEST_CASE("instrument names round-trip") {
    for (Instrument i : kAllInstruments) {
        CHECK(parse_instrument(instrument_id(i)) == i);
        CHECK_FALSE(instrument_label(i).empty());
    }
    CHECK(parse_instrument("HH") == Instrument::HatClosed);
    CHECK_FALSE(parse_instrument("vuvuzela").has_value());
    CHECK(parse_instrument("gtr") == Instrument::Guitar);
    CHECK(parse_instrument("cy") == Instrument::Crash);
}

TEST_CASE("step_time applies MPC swing to off-beat sixteenths only") {
    const double step = 60.0 / 120.0 / 4.0; // 125 ms at 120 BPM
    CHECK(step_time(0, 120, 0.5) == 0.0);
    CHECK(step_time(1, 120, 0.5) == doctest::Approx(step));
    CHECK(step_time(2, 120, 0.66) == doctest::Approx(2 * step)); // on-beat: untouched
    CHECK(step_time(3, 120, 0.66) == doctest::Approx(3 * step + 0.32 * step));
    // At swing 2/3 the off-beat lands exactly on the triplet grid.
    CHECK(step_time(1, 120, 2.0 / 3.0) == doctest::Approx(2 * step * 2.0 / 3.0));
}
