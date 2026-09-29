#include "street/beats/beats.hpp"
#include "street/core/stopwatch.hpp"

#include <algorithm>
#include <cmath>
#include <doctest/doctest.h>
#include <set>
#include <stdexcept>
#include <string>

using namespace street::beats;

namespace {

BeatSpec spec_for(Style style, std::uint64_t seed, int bars = 8) {
    BeatSpec spec;
    spec.style = style;
    spec.seed = seed;
    spec.bars = bars;
    return spec;
}

bool hit(const Pattern& p, Instrument i, int bar, int step) {
    const Track* t = p.find(i);
    return t != nullptr && t->steps[static_cast<std::size_t>(bar * 16 + step)].active();
}

bool any_step(const Pattern& p, Instrument i, bool (*pred)(const Step&)) {
    const Track* t = p.find(i);
    if (t == nullptr) return false;
    for (const Step& s : t->steps)
        if (pred(s)) return true;
    return false;
}

/// Index of the first sample whose magnitude exceeds `threshold`.
std::size_t first_sound(const AudioBuffer& a, float threshold = 1e-3f) {
    for (std::size_t i = 0; i < a.frames(); ++i)
        if (std::abs(a.left[i]) > threshold || std::abs(a.right[i]) > threshold) return i;
    return a.frames();
}

} // namespace

TEST_CASE("style table") {
    REQUIRE(styles().size() == 7);
    std::set<std::string_view> ids;
    for (const StyleInfo& s : styles()) {
        CAPTURE(s.id);
        ids.insert(s.id);
        CHECK(parse_style(s.id) == s.style);
        CHECK(&info(s.style) == &s);
        CHECK(s.default_bpm >= s.bpm_min);
        CHECK(s.default_bpm <= s.bpm_max);
        CHECK(s.default_swing >= 0.5);
        CHECK(parse_key(s.default_key).has_value());
        REQUIRE_FALSE(s.keys.empty());
        CHECK(s.keys.front() == s.default_key);
    }
    CHECK(ids.size() == 7);
    CHECK(parse_style("Lo-Fi") == Style::LoFi);
    CHECK(parse_style("boom bap") == Style::BoomBap);
    CHECK(parse_style("tamborzao") == Style::Funk);
    CHECK_FALSE(parse_style("polka").has_value());
    CHECK_FALSE(parse_style("").has_value());
}

TEST_CASE("resolve fills the blanks from the style and validates the rest") {
    for (const StyleInfo& s : styles()) {
        for (std::uint64_t seed = 1; seed <= 20; ++seed) {
            const BeatSpec r = resolve(spec_for(s.style, seed));
            CHECK(r.bpm >= s.bpm_min);
            CHECK(r.bpm <= s.bpm_max);
            CHECK(r.swing == s.default_swing);
            CHECK(parse_key(r.key).has_value());
            CHECK_FALSE(r.title.empty());
        }
    }
    BeatSpec custom = spec_for(Style::Trap, 3);
    custom.bpm = 150;
    custom.key = "ebm";
    custom.swing = 0.6;
    const BeatSpec r = resolve(custom);
    CHECK(r.bpm == 150);
    CHECK(r.key == "Ebm");
    CHECK(r.swing == 0.6);
    // Overriding the tempo must not re-roll the seeded key.
    BeatSpec only_bpm = spec_for(Style::Trap, 3);
    only_bpm.bpm = 133;
    CHECK(resolve(only_bpm).key == resolve(spec_for(Style::Trap, 3)).key);

    BeatSpec bad = spec_for(Style::BoomBap, 1);
    bad.bars = 0;
    CHECK_THROWS_AS(resolve(bad), std::invalid_argument);
    bad = spec_for(Style::BoomBap, 1);
    bad.bpm = 20;
    CHECK_THROWS_AS(resolve(bad), std::invalid_argument);
    bad = spec_for(Style::BoomBap, 1);
    bad.swing = 0.95;
    CHECK_THROWS_AS(resolve(bad), std::invalid_argument);
    bad = spec_for(Style::BoomBap, 1);
    bad.key = "H#";
    CHECK_THROWS_AS(resolve(bad), std::invalid_argument);
}

TEST_CASE("progressions are four chords rooted in the key") {
    for (const StyleInfo& s : styles()) {
        const auto in_a = progression(s.style, *parse_key("Am"), 7);
        const auto in_c = progression(s.style, *parse_key("Cm"), 7);
        REQUIRE(in_a.size() == 4);
        REQUIRE(in_c.size() == 4);
        for (std::size_t i = 0; i < 4; ++i) { // same seed, same shape, transposed up 3 semitones
            CHECK(in_c[i].root == (in_a[i].root + 3) % 12);
            CHECK(in_c[i].intervals == in_a[i].intervals);
        }
    }
    const auto lofi_major = progression(Style::LoFi, *parse_key("F"), 1);
    for (const Chord& c : lofi_major)
        CHECK(c.intervals.size() >= 4); // maj7 / min9 colours
    const auto trap = progression(Style::Trap, *parse_key("C#m"), 1);
    for (const Chord& c : trap)
        CHECK(c.intervals.size() == 3); // bare dark triads
}

TEST_CASE("same spec, same beat: generation is bit-for-bit deterministic") {
    const BeatSpec spec = spec_for(Style::Trap, 42, 2);
    const Beat a = generate(spec);
    const Beat b = generate(spec);
    CHECK(a.pattern == b.pattern);
    CHECK(a.chords == b.chords);
    CHECK(a.audio.left == b.audio.left); // exact float equality, not approximate
    CHECK(a.audio.right == b.audio.right);

    const Beat c = generate(spec_for(Style::Trap, 43, 2));
    CHECK(c.audio.left != a.audio.left);
}

TEST_CASE("composed patterns follow each style's grammar") {
    for (std::uint64_t seed = 1; seed <= 12; ++seed) {
        CAPTURE(seed);
        const Pattern boombap = compose(spec_for(Style::BoomBap, seed));
        const Pattern trap = compose(spec_for(Style::Trap, seed));
        const Pattern lofi = compose(spec_for(Style::LoFi, seed));
        const Pattern drill = compose(spec_for(Style::Drill, seed));
        const Pattern funk = compose(spec_for(Style::Funk, seed));
        for (const Pattern* p : {&boombap, &trap, &lofi, &drill, &funk}) {
            CHECK(p->bars() == 8);
            CHECK_FALSE(hit(*p, Instrument::Kick, 0, 0)); // bar 1 is an intro
            CHECK(p->chords.size() == 4);
        }
        for (int bar = 1; bar < 8; ++bar) {
            CAPTURE(bar);
            // Boom bap: kick on the one, snare cracking on 2 and 4.
            CHECK(hit(boombap, Instrument::Kick, bar, 0));
            CHECK(hit(boombap, Instrument::Snare, bar, 4));
            CHECK(hit(boombap, Instrument::Snare, bar, 12));
            // Trap and drill: half-time, the backbeat lands on beat 3 only.
            CHECK(hit(trap, Instrument::Clap, bar, 8));
            CHECK_FALSE(hit(trap, Instrument::Clap, bar, 4));
            CHECK_FALSE(hit(trap, Instrument::Snare, bar, 4));
            CHECK(hit(drill, Instrument::Snare, bar, 8));
            // Funk carioca: claps on 2 and 4 over the tamborzão.
            CHECK(hit(funk, Instrument::Clap, bar, 4));
            CHECK(hit(funk, Instrument::Clap, bar, 12));
            // Lo-fi: a chord on every downbeat.
            CHECK(hit(lofi, Instrument::Keys, bar, 0));
        }
        CHECK(any_step(trap, Instrument::HatClosed, [](const Step& s) { return s.roll >= 2; }));
        CHECK(any_step(drill, Instrument::HatClosed, [](const Step& s) { return s.roll == 3; }));
        CHECK(any_step(drill, Instrument::Bass, [](const Step& s) { return s.slide; }));
        CHECK(funk.find(Instrument::Tom) != nullptr);
        CHECK(any_step(boombap, Instrument::Snare,
                       [](const Step& s) { return s.velocity < 0.5f; })); // ghosts
    }
}

TEST_CASE("phrases vary: bars differ and every 4-bar phrase ends with a fill or drop-out") {
    for (const StyleInfo& s : styles()) {
        CAPTURE(s.id);
        const Pattern p = compose(spec_for(s.style, 5, 16));
        std::vector<std::vector<Step>> distinct_bars;
        for (int bar = 1; bar < 16; ++bar) {
            std::vector<Step> all;
            for (const Track& t : p.tracks)
                all.insert(all.end(), t.steps.begin() + bar * 16, t.steps.begin() + bar * 16 + 16);
            if (std::find(distinct_bars.begin(), distinct_bars.end(), all) == distinct_bars.end())
                distinct_bars.push_back(all);
        }
        CHECK(distinct_bars.size() >= 4);
    }
}

TEST_CASE("rendered audio: length, level, stereo, finite") {
    const Beat beat = generate(spec_for(Style::BoomBap, 11, 2));
    const double step = 60.0 / beat.spec.bpm / 4.0;
    CHECK(beat.audio.seconds() == doctest::Approx((2 * 16 + 4) * step).epsilon(1e-3));
    CHECK(peak_dbfs(beat.audio) == doctest::Approx(-1.0).epsilon(1e-3));
    CHECK(rms_dbfs(beat.audio) > -24.0);
    bool finite = true, stereo = false;
    for (std::size_t i = 0; i < beat.audio.frames(); ++i) {
        finite = finite && std::isfinite(beat.audio.left[i]) && std::isfinite(beat.audio.right[i]);
        stereo = stereo || std::abs(beat.audio.left[i] - beat.audio.right[i]) > 1e-3f;
    }
    CHECK(finite);
    CHECK(stereo); // hats and keys are panned
    CHECK(beat.chords.size() == 4);
}

TEST_CASE("the sequencer places hits on the swung grid") {
    const Pattern one_kick = parse_pattern("kick .... x... .... ....");
    BeatSpec spec;
    spec.style = Style::Trap; // no vinyl crackle before the first hit
    spec.bpm = 120;
    spec.bars = 1;
    spec.swing = 0.5;
    spec.humanize = false;
    const std::size_t on_beat = first_sound(render(one_kick, spec));
    CHECK(on_beat >= 22050);     // step 4 at 120 BPM = 0.5 s
    CHECK(on_beat < 22050 + 60); // within the 1.2 ms attack

    const Pattern offbeat = parse_pattern("kick .x.. .... .... ....");
    spec.swing = 0.66;
    const std::size_t swung = first_sound(render(offbeat, spec));
    const auto expected = static_cast<std::size_t>(step_time(1, 120, 0.66) * 44100);
    CHECK(swung >= expected);
    CHECK(swung < expected + 60);
}

TEST_CASE("from_pattern loops a hand-written groove and honours its directives") {
    const Pattern groove =
        parse_pattern("bpm 96\nkey Dm\nkick x... ..x. .... ....\nkeys x... .... .... ....");
    BeatSpec spec;
    spec.style = Style::LoFi;
    spec.bars = 3;
    spec.seed = 9;
    const Beat beat = from_pattern(groove, spec);
    CHECK(beat.spec.bpm == 96);
    CHECK(beat.spec.key == "Dm");
    CHECK(beat.pattern.bars() == 3);
    CHECK(hit(beat.pattern, Instrument::Kick, 2, 6));
    CHECK(beat.pattern.chords ==
          progression(Style::LoFi, *parse_key("Dm"), 9)); // lo-fi chords in Dm

    spec.bpm = 80; // explicit values beat the file's directives
    CHECK(from_pattern(groove, spec).spec.bpm == 80);
    CHECK_THROWS_AS(from_pattern(Pattern{}, spec), std::invalid_argument);
}

TEST_CASE("rendering is fast") {
#if defined(NDEBUG) && !defined(__SANITIZE_ADDRESS__)
    constexpr int kBars = 8;
#else
    constexpr int kBars = 2; // unoptimised / sanitised builds are 30-50x slower
#endif
    const street::Stopwatch watch;
    const Beat beat = generate(spec_for(Style::LoFi, 3, kBars)); // the heaviest style: longest bars
    const double seconds = watch.seconds();
    MESSAGE(kBars << " bars of lo-fi (" << beat.audio.seconds() << " s of audio) rendered in "
                  << seconds << " s");
    if constexpr (kBars == 8)
        CHECK(seconds < 2.0); // typically ~0.3 s; generous for loaded CI machines
}

TEST_CASE("era styles: TR-808 electro and the NWOBHM gallop follow their grammar") {
    CHECK(parse_style("electro") == Style::Electro);
    CHECK(parse_style("gallop") == Style::Gallop);
    CHECK(parse_style("NWOBHM") == Style::Gallop);
    CHECK(parse_style("metal") == Style::Gallop);
    for (std::uint64_t seed = 1; seed <= 10; ++seed) {
        CAPTURE(seed);
        const Pattern electro = compose(spec_for(Style::Electro, seed));
        CHECK(electro.find(Instrument::Cowbell) != nullptr);
        CHECK(any_step(electro, Instrument::Bass, [](const Step& s) { return s.note == 12; }));
        for (int bar = 1; bar < 8; ++bar) {
            CHECK(hit(electro, Instrument::Kick, bar, 0));
            CHECK(hit(electro, Instrument::Snare, bar, 4)); // snare + 808 clap on 2 and 4
            CHECK(hit(electro, Instrument::Clap, bar, 12));
        }

        const Pattern gallop = compose(spec_for(Style::Gallop, seed));
        CHECK(gallop.chords.front().intervals == std::vector<int>{0, 7}); // power chords
        CHECK(hit(gallop, Instrument::Crash, 1, 0)); // the drums enter on a crash
        CHECK(any_step(gallop, Instrument::Guitar, [](const Step& s) { return s.ring; }));
        for (int bar = 0; bar < 8; ++bar) {
            if (bar % 4 == 3) continue; // phrase ends break the pattern on purpose
            CAPTURE(bar);
            // The gallop: an eighth and two sixteenths on every beat, x.xx x.xx x.xx x.xx.
            for (int beat = 0; beat < 4; ++beat) {
                for (Instrument part : {Instrument::Guitar, Instrument::Bass}) {
                    CHECK(hit(gallop, part, bar, beat * 4));
                    CHECK_FALSE(hit(gallop, part, bar, beat * 4 + 1));
                    CHECK(hit(gallop, part, bar, beat * 4 + 2));
                    CHECK(hit(gallop, part, bar, beat * 4 + 3));
                }
            }
            if (bar == 0) continue;               // intro: the riff alone
            for (int step : {0, 2, 3, 8, 10, 11}) // the kick gallops at least on beats 1 and 3
                CHECK(hit(gallop, Instrument::Kick, bar, step));
            CHECK(hit(gallop, Instrument::Snare, bar, 4));
            CHECK(hit(gallop, Instrument::Snare, bar, 12));
        }
    }
}

TEST_CASE("era styles render deterministically, in tempo, without clipping") {
    for (Style style : {Style::Electro, Style::Gallop}) {
        CAPTURE(info(style).id);
        const Beat a = generate(spec_for(style, 21, 2));
        const Beat b = generate(spec_for(style, 21, 2));
        CHECK(a.audio.left == b.audio.left);
        CHECK(a.audio.right == b.audio.right);
        CHECK(a.spec.bpm >= info(style).bpm_min);
        CHECK(a.spec.bpm <= info(style).bpm_max);
        CHECK(a.spec.swing == 0.5); // both eras are dead straight
        CHECK(peak_dbfs(a.audio) <= -0.99);
        CHECK(rms_dbfs(a.audio) > -24.0);
        bool finite = true;
        for (std::size_t i = 0; i < a.audio.frames(); ++i)
            finite = finite && std::isfinite(a.audio.left[i]) && std::isfinite(a.audio.right[i]);
        CHECK(finite);
    }
}
