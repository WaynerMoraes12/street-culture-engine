#pragma once
// Just enough music theory for beat making: keys, chord symbols, voicings.
//
// Pitches are MIDI note numbers (60 = middle C, 69 = A4 = 440 Hz); pitch classes
// are 0..11 with C = 0. Chord names are spelled with sharps or flats to match the
// key they were built in ("Bbm7" in F minor, "A#m7" in F# major).

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace street::beats {

/// Equal temperament: 440 Hz · 2^((midi − 69) / 12).
double midi_to_hz(double midi) noexcept;

/// "C", "C#"/"Db", ... for a pitch class (any integer; wrapped into 0..11).
std::string note_name(int pitch_class, bool flats);

struct Key {
    int tonic = 9; ///< pitch class of the tonic (default A)
    bool minor = true;
    bool flats = false; ///< spelling preference for note names in this key

    std::string name() const;      ///< compact form: "C#m", "Bb", "Ebm"
    std::string long_name() const; ///< "C# minor", "Bb major"
    friend bool operator==(const Key&, const Key&) = default;
};

/// Parses "C#m", "F", "Ebm", "Bb", "a minor", "F# maj", "gmin"... Root letters are
/// case-insensitive; accidentals are '#' or 'b'. Returns nullopt if unrecognised.
std::optional<Key> parse_key(std::string_view text);

struct Chord {
    int root = 0;               ///< pitch class
    std::string name;           ///< display symbol, e.g. "F#m9"
    std::vector<int> intervals; ///< semitones above the root, e.g. {0, 3, 7, 10, 14}

    /// Root placed in the 808 register, E1..D#2 (MIDI 28–39, ~41–78 Hz).
    int bass_note() const noexcept;
    /// MIDI notes for the keys, centred around middle C. Extended chords (five or more
    /// notes) drop the root, the way jazz pianists voice them when a bassist plays it.
    std::vector<int> voicing() const;
    friend bool operator==(const Chord&, const Chord&) = default;
};

/// Chord qualities understood by make_chord / parse_chord (canonical spellings).
std::vector<std::string_view> chord_qualities();

/// Builds a chord from a root pitch class and a quality suffix such as "", "m", "7",
/// "m7", "maj7", "m9", "maj9", "9", "m11", "m7b5", "dim", "sus2", "sus4", "add9", "6",
/// "5" (power chord).
/// Throws std::invalid_argument for an unknown quality.
Chord make_chord(int root_pitch_class, std::string_view quality, bool flats);

/// Parses a chord symbol like "Am7", "F#m9", "Bbmaj7", "E9", "Dm7b5".
/// Throws std::invalid_argument with a helpful message on failure.
Chord parse_chord(std::string_view symbol);

} // namespace street::beats
