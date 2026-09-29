#include "street/beats/theory.hpp"

#include "street/core/text.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <stdexcept>

namespace street::beats {
namespace {

constexpr std::array<std::string_view, 12> kSharpNames = {"C",  "C#", "D",  "D#", "E",  "F",
                                                          "F#", "G",  "G#", "A",  "A#", "B"};
constexpr std::array<std::string_view, 12> kFlatNames = {"C",  "Db", "D",  "Eb", "E",  "F",
                                                         "Gb", "G",  "Ab", "A",  "Bb", "B"};

struct Quality {
    std::string_view suffix;
    std::vector<int> intervals;
};

// First entry of each interval set is the canonical spelling; later ones are aliases.
const std::vector<Quality>& qualities() {
    static const std::vector<Quality> table = {
        {"", {0, 4, 7}},
        {"maj", {0, 4, 7}},
        {"m", {0, 3, 7}},
        {"min", {0, 3, 7}},
        {"-", {0, 3, 7}},
        {"7", {0, 4, 7, 10}},
        {"maj7", {0, 4, 7, 11}},
        {"M7", {0, 4, 7, 11}},
        {"m7", {0, 3, 7, 10}},
        {"min7", {0, 3, 7, 10}},
        {"-7", {0, 3, 7, 10}},
        {"m9", {0, 3, 7, 10, 14}},
        {"maj9", {0, 4, 7, 11, 14}},
        {"9", {0, 4, 7, 10, 14}},
        {"m11", {0, 3, 7, 10, 14, 17}},
        {"m7b5", {0, 3, 6, 10}},
        {"dim", {0, 3, 6}},
        {"sus2", {0, 2, 7}},
        {"sus4", {0, 5, 7}},
        {"add9", {0, 4, 7, 14}},
        {"6", {0, 4, 7, 9}},
        {"m6", {0, 3, 7, 9}},
        {"7b9", {0, 4, 7, 10, 13}},
        {"aug", {0, 4, 8}},
        {"5", {0, 7}}, // power chord
    };
    return table;
}

std::string_view canonical_suffix(const std::vector<int>& intervals) {
    for (const auto& q : qualities())
        if (q.intervals == intervals) return q.suffix;
    return {};
}

int wrap12(int pc) noexcept {
    return ((pc % 12) + 12) % 12;
}

/// Letter + optional accidental at the front of `s`; returns pitch class and consumed length.
std::optional<std::pair<int, std::size_t>> parse_root(std::string_view s, char* accidental) {
    if (s.empty()) return std::nullopt;
    static constexpr std::array<int, 7> kLetterPc = {9, 11, 0, 2, 4, 5, 7}; // A B C D E F G
    const char letter = static_cast<char>(s[0] >= 'a' && s[0] <= 'z' ? s[0] - 'a' + 'A' : s[0]);
    if (letter < 'A' || letter > 'G') return std::nullopt;
    int pc = kLetterPc[static_cast<std::size_t>(letter - 'A')];
    std::size_t used = 1;
    *accidental = 0;
    if (s.size() > 1 && (s[1] == '#' || s[1] == 'b')) {
        *accidental = s[1];
        pc += s[1] == '#' ? 1 : -1;
        used = 2;
    }
    return std::pair{wrap12(pc), used};
}

} // namespace

double midi_to_hz(double midi) noexcept {
    return 440.0 * std::pow(2.0, (midi - 69.0) / 12.0);
}

std::string note_name(int pitch_class, bool flats) {
    const auto pc = static_cast<std::size_t>(wrap12(pitch_class));
    return std::string(flats ? kFlatNames[pc] : kSharpNames[pc]);
}

std::string Key::name() const {
    return note_name(tonic, flats) + (minor ? "m" : "");
}

std::string Key::long_name() const {
    return note_name(tonic, flats) + (minor ? " minor" : " major");
}

std::optional<Key> parse_key(std::string_view text) {
    const std::string_view trimmed = street::text::trim(text);
    char accidental = 0;
    const auto root = parse_root(trimmed, &accidental);
    if (!root) return std::nullopt;

    const std::string mode =
        street::text::to_lower(street::text::trim(trimmed.substr(root->second)));
    Key key;
    key.tonic = root->first;
    if (mode.empty() || mode == "maj" || mode == "major") {
        key.minor = false;
    } else if (mode == "m" || mode == "min" || mode == "minor" || mode == "-") {
        key.minor = true;
    } else {
        return std::nullopt;
    }
    if (accidental != 0) {
        key.flats = accidental == 'b';
    } else {
        // Natural tonics whose key signatures use flats: F major; D, G, C, F minor.
        constexpr std::array<int, 4> kFlatMinors = {2, 7, 0, 5};
        key.flats = key.minor ? std::find(kFlatMinors.begin(), kFlatMinors.end(), key.tonic) !=
                                    kFlatMinors.end()
                              : key.tonic == 5;
    }
    return key;
}

int Chord::bass_note() const noexcept {
    const int midi = 24 + wrap12(root);
    return midi < 28 ? midi + 12 : midi;
}

std::vector<int> Chord::voicing() const {
    std::vector<int> shape = intervals;
    if (shape.size() >= 5) shape.erase(shape.begin());          // rootless voicing
    if (shape.size() <= 3) shape.push_back(shape.front() + 12); // spread triads, power chords
    if (shape.empty()) return {};
    double mean = 0.0;
    for (int i : shape)
        mean += i;
    mean /= static_cast<double>(shape.size());
    // Choose the octave that puts the chord's centre of gravity nearest middle C.
    int base = 48 + wrap12(root);
    for (int candidate : {base - 12, base + 12})
        if (std::abs(candidate + mean - 60.0) < std::abs(base + mean - 60.0)) base = candidate;
    std::vector<int> notes;
    notes.reserve(shape.size());
    for (int i : shape)
        notes.push_back(base + i);
    return notes;
}

std::vector<std::string_view> chord_qualities() {
    std::vector<std::string_view> out;
    for (const auto& q : qualities())
        if (canonical_suffix(q.intervals) == q.suffix) out.push_back(q.suffix);
    return out;
}

Chord make_chord(int root_pitch_class, std::string_view quality, bool flats) {
    const auto& table = qualities();
    const auto it = std::find_if(table.begin(), table.end(),
                                 [&](const Quality& q) { return q.suffix == quality; });
    if (it == table.end())
        throw std::invalid_argument(std::format("unknown chord quality '{}'", quality));
    Chord chord;
    chord.root = wrap12(root_pitch_class);
    chord.intervals = it->intervals;
    chord.name = note_name(chord.root, flats) + std::string(canonical_suffix(it->intervals));
    return chord;
}

Chord parse_chord(std::string_view symbol) {
    const std::string_view s = street::text::trim(symbol);
    char accidental = 0;
    const auto root = parse_root(s, &accidental);
    if (!root || s[0] < 'A' || s[0] > 'G')
        throw std::invalid_argument(std::format(
            "invalid chord '{}': expected a root A-G (optionally # or b) then a quality like m7",
            symbol));
    try {
        return make_chord(root->first, s.substr(root->second), accidental == 'b');
    } catch (const std::invalid_argument&) {
        std::string known;
        for (auto q : chord_qualities())
            if (!q.empty()) known += (known.empty() ? "" : " ") + std::string(q);
        throw std::invalid_argument(
            std::format("invalid chord '{}': unknown quality '{}' (known: {})", symbol,
                        s.substr(root->second), known));
    }
}

} // namespace street::beats
