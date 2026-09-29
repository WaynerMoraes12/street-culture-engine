#pragma once
// street::beats — a procedural hip-hop beat engine that renders straight to WAV.
//
//   BeatSpec spec;                       // style, tempo, key, swing, bars, seed
//   spec.style = Style::Trap;
//   spec.seed = 42;
//   Beat beat = generate(spec);          // compose a pattern + render the audio
//   write_wav("trap-42.wav", beat.audio);
//   std::cout << render_grid(beat.pattern) << render_waveform(beat.audio, 80, 4);
//
// Pipeline: resolve() fills unspecified fields from the style (seeded), compose()
// writes the drum grammar, bass line and chord cycle into a Pattern, render() plays
// the pattern through the style's synthesised kit: sequencer (swing, rolls,
// humanisation) → voices → per-track gain/pan → bus effects → master limiter.
//
// Deterministic: the same BeatSpec (seed included) gives bit-identical audio on a
// given build. Every random decision draws from street::Rng streams derived from the seed.

#include "street/beats/audio.hpp"
#include "street/beats/pattern.hpp"
#include "street/beats/style.hpp"
#include "street/beats/theory.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace street::beats {

struct BeatSpec {
    Style style = Style::BoomBap;
    int bpm = 0;         ///< 0 = seeded pick from the style's tempo range
    int bars = 8;        ///< generate(): length; render(): <= 0 means the pattern's own length
    std::string key;     ///< empty = seeded pick from the style's keys ("C#m", "F", "Ebm", "Bb")
    double swing = -1.0; ///< < 0 = the style's swing; else 0.5 (straight) … 0.8
    std::uint64_t seed = 1;
    std::string title;    ///< empty = a generated title
    bool humanize = true; ///< seeded timing drift and velocity variation
};

struct Beat {
    BeatSpec spec; ///< resolved: every field filled in
    Pattern pattern;
    std::vector<std::string> chords; ///< the chord cycle, one chord per bar
    AudioBuffer audio;
};

/// Fills unspecified fields (bpm, key, swing, title) from the style and seed and
/// normalises the key name. Throws std::invalid_argument for out-of-range values
/// (bars 1–256, bpm 40–300, swing 0.5–0.8) or an unknown key.
BeatSpec resolve(const BeatSpec& spec);

/// Writes the style's drum grammar, bass line and chord cycle for a spec: seeded
/// grooves with per-bar variation, fills or drop-outs closing each 4-bar phrase and
/// an intro feel on bar 1 (for beats of 4+ bars).
Pattern compose(const BeatSpec& spec);

/// resolve + compose + render.
Beat generate(const BeatSpec& spec);

/// A Beat from a hand-written pattern: the spec is resolved against the pattern's own
/// directives (explicit spec values win), the groove is looped to spec.bars (<= 0 keeps
/// its length), chords default to the style's progression, and the result is rendered.
/// A pattern without a title keeps `spec.title` or gets a generated one.
Beat from_pattern(Pattern pattern, const BeatSpec& spec);

/// Renders a pattern with the sound palette of `spec.style`. Tempo, swing and key come
/// from the spec when set, else from the pattern's directives, else the style.
/// Patterns without chords get the style's progression in that key.
AudioBuffer render(const Pattern& pattern, const BeatSpec& spec);

/// A 4-chord progression in `key` drawn from the style's chord language.
std::vector<Chord> progression(Style style, const Key& key, std::uint64_t seed);

/// Colored step-sequencer view: one row per instrument, beats grouped, bars separated,
/// velocity shading, rolls and 808 slides marked, chord names above each bar.
std::string render_grid(const Pattern& pattern);

struct GridOptions {
    int max_bars = 8;     ///< bars shown (0 = all); a note says how many were omitted
    int bars_per_row = 4; ///< 4 bars fit in ~92 columns
};
std::string render_grid(const Pattern& pattern, const GridOptions& options);

/// Braille overview of the audio: `width` columns × `height` rows, each cell two
/// time slices wide and four dots tall. Amplitude uses a -48…0 dB scale so quiet
/// hats stay visible next to kicks. Colored by loudness.
std::string render_waveform(const AudioBuffer& audio, int width, int height);

/// Boxed summary of a beat: style, title, tempo, key, swing, bars, seed, chords.
std::string render_summary(const Beat& beat);

/// Table of the available styles.
std::string render_style_list();

} // namespace street::beats
