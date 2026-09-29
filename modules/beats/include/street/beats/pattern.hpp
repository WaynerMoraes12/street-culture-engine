#pragma once
// Step-sequencer patterns and the groove DSL.
//
// A Pattern is a grid: one Track per instrument, 16 steps (sixteenth notes) per
// bar, each Step carrying a velocity, an optional roll and, for tuned voices, a
// pitch offset and slide flag. Harmony travels with the pattern as a chord cycle
// (bar b plays chords[b % chords.size()]), so a pattern fully describes a beat.
//
// ── Groove DSL ──────────────────────────────────────────────────────────────
//
//   # Sunday Loop — '#' at line start or after a space opens a comment
//   # (so 'C#m' still works); blank lines are ignored
//   title   Sunday Loop
//   bpm     90                 # optional directives (command-line flags win)
//   swing   62%                # 0.5–0.8, or a percentage 50%–80%
//   key     Am
//   style   boombap            # suggested kit: any style id (boombap, gallop…)
//   chords  Am9 Dm7 | G9 Cmaj7 # one per bar, cycled; default: the style's progression
//
//   kick    | x..o..x. ..o.x... |
//   snare   | ....x... ....x..g |
//   hat     | o.o.o.o. o.o.orr. |
//   bass    | x.....x. ..^.x... |
//   keys    | x....... ....o... |
//
// A track line is an instrument name followed by step characters. Spaces and '|'
// are only visual grouping. Each track must contain whole bars (multiples of 16
// steps); repeating a track name appends more bars, and shorter tracks loop to the
// length of the longest one.
//
//   x  accent        o  hit             g  ghost note        .  -  _  ·  rest
//   r  1/32 roll     t  triplet roll    R T  accented rolls
//   ^  pitched up an octave (on bass: slide up into it)
//   ~  bass: slide back down to the chord root (elsewhere: a hit)
//   *  guitar: open power chord left ringing (plain hits are palm-muted chugs)
//
// Instruments: kick, snare, clap, rim, cowbell, hat (closed), openhat, ride, crash,
// shaker, tom, bass, guitar, keys — plus aliases bd, sd, cp, rs, cb, hh, ch, oh, rd,
// cy, sh, lt, 808, gtr, chords, ep. Guitar plays power chords on the chord roots
// (a `note` offset moves the riff); keys play the full chord.
// Errors throw std::invalid_argument naming the line (and column) at fault.

#include "street/beats/style.hpp"
#include "street/beats/theory.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace street::beats {

/// Display / mixing order of the instruments.
enum class Instrument {
    Kick,
    Snare,
    Clap,
    Rim,
    Cowbell,
    HatClosed,
    HatOpen,
    Ride,
    Crash,
    Shaker,
    Tom,
    Bass,
    Guitar,
    Keys
};
inline constexpr std::size_t kInstrumentCount = 14;
inline constexpr std::array<Instrument, kInstrumentCount> kAllInstruments = {
    Instrument::Kick,    Instrument::Snare,     Instrument::Clap,    Instrument::Rim,
    Instrument::Cowbell, Instrument::HatClosed, Instrument::HatOpen, Instrument::Ride,
    Instrument::Crash,   Instrument::Shaker,    Instrument::Tom,     Instrument::Bass,
    Instrument::Guitar,  Instrument::Keys};

std::string_view instrument_id(Instrument instrument) noexcept;    ///< DSL name: "openhat"
std::string_view instrument_label(Instrument instrument) noexcept; ///< display name: "open hat"
std::optional<Instrument> parse_instrument(std::string_view name); ///< ids and aliases

struct Step {
    float velocity = 0.0f; ///< 0 = rest, otherwise (0, 1]
    std::uint8_t roll = 0; ///< 0 = single hit, n >= 2 = n evenly spaced hits inside the step
    std::int8_t note = 0;  ///< semitones: bass → above the chord root; tom → above its tuning
    bool slide = false;    ///< bass: glide into this note from the previous one
    bool ring = false;     ///< guitar: open chord left ringing (default: palm-muted chug)

    bool active() const noexcept { return velocity > 0.0f; }
    friend bool operator==(const Step&, const Step&) = default;
};

struct Track {
    Instrument instrument = Instrument::Kick;
    std::vector<Step> steps; ///< always bars × kStepsPerBar long

    int hits() const noexcept; ///< number of active steps
    friend bool operator==(const Track&, const Track&) = default;
};

struct Pattern {
    static constexpr int kStepsPerBar = 16;

    std::string title;
    int bpm = 0;                ///< 0 = not specified
    double swing = -1.0;        ///< < 0 = not specified
    std::string key;            ///< empty = not specified
    std::optional<Style> style; ///< suggested sound palette (the command uses it unless --style)
    std::vector<Chord> chords;
    std::vector<Track> tracks;

    int bars() const noexcept { return bars_; }
    int total_steps() const noexcept { return bars_ * kStepsPerBar; }
    /// Changes the length of every track, looping existing material into new bars.
    void resize(int bars);

    const Track* find(Instrument instrument) const noexcept;
    /// The track for `instrument`, created empty (and kept in display order) if missing.
    Track& track(Instrument instrument);
    /// Step `step` (0..15) of bar `bar` on the given track; creates the track if needed.
    Step& at(Instrument instrument, int bar, int step);
    /// The chord that plays in `bar` (nullptr if the pattern has no harmony).
    const Chord* chord_at(int bar) const noexcept;

    friend bool operator==(const Pattern&, const Pattern&) = default;

private:
    int bars_ = 0;
};

/// Parses the groove DSL documented above. Throws std::invalid_argument.
Pattern parse_pattern(std::string_view text);

/// Onset of sixteenth-note `step` (counted from the top of the song) in seconds.
/// MPC-style swing delays every off-beat sixteenth: within each eighth-note pair the
/// second note lands at `swing` × the pair's length, i.e. (2·swing − 1) steps late.
/// swing = 0.5 is straight, ≈ 0.667 is a full triplet shuffle.
double step_time(int step, double bpm, double swing) noexcept;

} // namespace street::beats
