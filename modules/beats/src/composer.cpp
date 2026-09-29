// Composer front end: spec resolution, chord language, titles, and the dispatch
// into each style's drum grammar (grooves.cpp).

#include "grooves.hpp"
#include "streams.hpp"
#include "street/beats/beats.hpp"
#include "street/core/rng.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <span>
#include <stdexcept>
#include <utility>

namespace street::beats {
namespace {

using detail::Stream;
using detail::stream;

/// A chord as a scale step: semitones above the tonic plus a quality suffix.
struct Degree {
    int semitones;
    std::string_view quality;
};
using Progression = std::array<Degree, 4>;

// Chord languages. Boom bap and lo-fi borrow jazz: minor 7ths and 9ths, rootless voicings,
// ii–V movement. Trap and drill stay in bare, dark triads (drill leans on the harmonic
// minor's major V and the Phrygian bII). Funk carioca vamps on one or two chords.
constexpr Progression kBoomBapMinor[] = {
    {{{0, "m9"}, {5, "m7"}, {10, "9"}, {3, "maj7"}}},
    {{{0, "m7"}, {0, "m7"}, {5, "m9"}, {7, "m7"}}},
    {{{0, "m9"}, {8, "maj7"}, {5, "m7"}, {7, "7"}}},
    {{{0, "m7"}, {2, "m7b5"}, {7, "7b9"}, {0, "m9"}}},
};
constexpr Progression kBoomBapMajor[] = {
    {{{0, "maj7"}, {9, "m7"}, {2, "m9"}, {7, "9"}}},
    {{{2, "m9"}, {7, "9"}, {0, "maj9"}, {0, "maj9"}}},
    {{{0, "maj9"}, {4, "m7"}, {5, "maj7"}, {7, "9"}}},
};
constexpr Progression kLoFiMinor[] = {
    {{{0, "m9"}, {5, "m9"}, {8, "maj7"}, {7, "m7"}}},
    {{{5, "m9"}, {10, "9"}, {3, "maj7"}, {8, "maj7"}}},
    {{{0, "m11"}, {8, "maj7"}, {3, "maj7"}, {10, "6"}}},
    {{{0, "m9"}, {3, "maj7"}, {5, "m7"}, {8, "maj9"}}},
};
constexpr Progression kLoFiMajor[] = {
    {{{0, "maj7"}, {4, "m7"}, {9, "m9"}, {5, "maj7"}}},
    {{{5, "maj7"}, {4, "m7"}, {2, "m9"}, {0, "maj9"}}},
    {{{2, "m9"}, {7, "9"}, {0, "maj7"}, {9, "m9"}}},
    {{{0, "maj9"}, {9, "m9"}, {5, "maj9"}, {7, "9"}}},
};
constexpr Progression kTrapMinor[] = {
    {{{0, "m"}, {8, ""}, {3, ""}, {10, ""}}},
    {{{0, "m"}, {0, "m"}, {8, ""}, {7, ""}}},
    {{{0, "m"}, {1, ""}, {0, "m"}, {10, ""}}},
    {{{0, "m"}, {5, "m"}, {8, ""}, {7, "m"}}},
};
constexpr Progression kDrillMinor[] = {
    {{{0, "m"}, {8, ""}, {5, "m"}, {7, ""}}},
    {{{0, "m"}, {1, ""}, {8, ""}, {7, ""}}},
    {{{0, "m"}, {3, ""}, {8, ""}, {7, ""}}},
    {{{0, "m"}, {0, "m"}, {5, "m"}, {7, ""}}},
};
constexpr Progression kDarkMajor[] = {
    {{{0, ""}, {9, "m"}, {5, ""}, {7, ""}}},
    {{{0, ""}, {3, ""}, {10, ""}, {5, ""}}},
};
constexpr Progression kFunkMinor[] = {
    {{{0, "m"}, {0, "m"}, {10, ""}, {10, ""}}},
    {{{0, "m"}, {5, "m"}, {0, "m"}, {7, "m"}}},
    {{{0, "m7"}, {8, "maj7"}, {0, "m7"}, {10, "7"}}},
    {{{0, "m"}, {8, ""}, {10, ""}, {0, "m"}}},
};
constexpr Progression kFunkMajor[] = {
    {{{0, ""}, {0, ""}, {5, ""}, {7, ""}}},
    {{{0, ""}, {10, ""}, {5, ""}, {0, ""}}},
};
// Electro vamps on bright minor triads and sevenths, stabbed on the off-beats. The gallop
// speaks in power chords ("5"): the aeolian i-bVII-bVI moves of British metal.
constexpr Progression kElectroMinor[] = {
    {{{0, "m"}, {0, "m"}, {8, ""}, {10, ""}}},
    {{{0, "m"}, {3, ""}, {10, ""}, {5, "m"}}},
    {{{0, "m7"}, {5, "m7"}, {0, "m7"}, {7, "m7"}}},
    {{{0, "m"}, {10, ""}, {8, ""}, {10, ""}}},
};
constexpr Progression kElectroMajor[] = {
    {{{0, ""}, {10, ""}, {5, ""}, {0, ""}}},
    {{{0, "7"}, {0, "7"}, {5, "7"}, {7, ""}}},
};
constexpr Progression kGallopMinor[] = {
    {{{0, "5"}, {10, "5"}, {8, "5"}, {10, "5"}}},
    {{{0, "5"}, {8, "5"}, {10, "5"}, {0, "5"}}},
    {{{0, "5"}, {5, "5"}, {8, "5"}, {10, "5"}}},
    {{{0, "5"}, {0, "5"}, {10, "5"}, {8, "5"}}},
};
constexpr Progression kGallopMajor[] = {
    {{{0, "5"}, {10, "5"}, {5, "5"}, {0, "5"}}},
    {{{0, "5"}, {7, "5"}, {5, "5"}, {7, "5"}}},
};

std::span<const Progression> chord_language(Style style, bool minor) {
    switch (style) {
    case Style::BoomBap:
        return minor ? std::span<const Progression>(kBoomBapMinor) : kBoomBapMajor;
    case Style::LoFi:
        return minor ? std::span<const Progression>(kLoFiMinor) : kLoFiMajor;
    case Style::Trap:
        return minor ? std::span<const Progression>(kTrapMinor) : kDarkMajor;
    case Style::Drill:
        return minor ? std::span<const Progression>(kDrillMinor) : kDarkMajor;
    case Style::Funk:
        return minor ? std::span<const Progression>(kFunkMinor) : kFunkMajor;
    case Style::Electro:
        return minor ? std::span<const Progression>(kElectroMinor) : kElectroMajor;
    case Style::Gallop:
        return minor ? std::span<const Progression>(kGallopMinor) : kGallopMajor;
    }
    return kTrapMinor;
}

// Original title vocabulary: street, court and studio imagery.
constexpr std::string_view kTitleFirst[] = {
    "Midnight", "Concrete", "Velvet",  "Golden",  "Rusted",  "Neon",   "Quiet", "Heavy",
    "Late",     "Faded",    "Crooked", "Paper",   "Smoke",   "Iron",   "Blue",  "Low",
    "Cold",     "Sunday",   "Rooftop", "Borough", "Asphalt", "Chrome", "Hazy",  "Northside"};
constexpr std::string_view kTitleFunkFirst[] = {"Baile",  "Morro", "Laje",
                                                "Tambor", "Rio",   "Quebrada"};
constexpr std::string_view kTitleSecond[] = {
    "Cipher",  "Stoop",  "Halo",     "Transit",   "Anthem",   "Parquet",  "Backboard", "Frequency",
    "Dynasty", "Hustle", "Fadeaway", "Crossover", "Skyline",  "Corner",   "Static",    "Motion",
    "Tape",    "Signal", "Bounce",   "Rotation",  "Blacktop", "Overtime", "Echoes",    "Satellite"};

constexpr std::string_view kTitleMetalFirst[] = {"Iron",   "Steel",   "Thunder",  "Burning",
                                                 "Silver", "Seventh", "Midnight", "Wild"};
constexpr std::string_view kTitleMetalSecond[] = {"Charge",  "Cavalry", "Stampede", "Crusade",
                                                  "Horizon", "Rider",   "Siege",    "Frontier"};

std::string make_title(Style style, std::uint64_t seed) {
    Rng rng(seed ^ hash_seed(info(style).id), stream(Stream::Title));
    if (style == Style::Gallop)
        return std::format("{} {}", rng.pick(kTitleMetalFirst), rng.pick(kTitleMetalSecond));
    const std::string_view first =
        style == Style::Funk && rng.chance(0.6) ? rng.pick(kTitleFunkFirst) : rng.pick(kTitleFirst);
    return std::format("{} {}", first, rng.pick(kTitleSecond));
}

} // namespace

std::vector<Chord> progression(Style style, const Key& key, std::uint64_t seed) {
    Rng rng(seed, stream(Stream::Harmony));
    const std::span<const Progression> pool = chord_language(style, key.minor);
    const Progression& choice = pool[rng.below(static_cast<std::uint32_t>(pool.size()))];
    std::vector<Chord> chords;
    for (const Degree& d : choice)
        chords.push_back(make_chord(key.tonic + d.semitones, d.quality, key.flats));
    return chords;
}

BeatSpec resolve(const BeatSpec& spec) {
    BeatSpec r = spec;
    const StyleInfo& style = info(spec.style);
    if (r.bars < 1 || r.bars > 256)
        throw std::invalid_argument(std::format("bars must be between 1 and 256, got {}", r.bars));

    // Draw both picks unconditionally so an explicit --bpm never changes the seeded key.
    Rng rng(spec.seed, stream(Stream::Resolve));
    const int drawn_bpm = rng.range(style.bpm_min, style.bpm_max);
    const std::string_view drawn_key =
        style.keys[rng.below(static_cast<std::uint32_t>(style.keys.size()))];

    if (r.bpm == 0) r.bpm = drawn_bpm;
    if (r.bpm < 40 || r.bpm > 300)
        throw std::invalid_argument(std::format("bpm must be between 40 and 300, got {}", r.bpm));
    if (r.swing < 0.0) r.swing = style.default_swing;
    if (r.swing < 0.5 || r.swing > 0.8)
        throw std::invalid_argument(
            std::format("swing must be between 0.5 and 0.8, got {}", r.swing));
    if (r.key.empty()) r.key = std::string(drawn_key);
    const auto key = parse_key(r.key);
    if (!key)
        throw std::invalid_argument(
            std::format("unknown key '{}' (try Am, C#m, F, Ebm, Bb)", r.key));
    r.key = key->name();
    if (r.title.empty()) r.title = make_title(r.style, r.seed);
    return r;
}

Pattern compose(const BeatSpec& spec) {
    const BeatSpec r = resolve(spec);
    Pattern pattern;
    pattern.title = r.title;
    pattern.bpm = r.bpm;
    pattern.swing = r.swing;
    pattern.key = r.key;
    pattern.chords = progression(r.style, *parse_key(r.key), r.seed);
    pattern.resize(r.bars);

    Rng rng(r.seed, stream(Stream::Groove));
    switch (r.style) {
    case Style::BoomBap:
        detail::groove_boombap(pattern, rng);
        break;
    case Style::Trap:
        detail::groove_trap(pattern, rng);
        break;
    case Style::LoFi:
        detail::groove_lofi(pattern, rng);
        break;
    case Style::Drill:
        detail::groove_drill(pattern, rng);
        break;
    case Style::Funk:
        detail::groove_funk(pattern, rng);
        break;
    case Style::Electro:
        detail::groove_electro(pattern, rng);
        break;
    case Style::Gallop:
        detail::groove_gallop(pattern, rng);
        break;
    }
    std::erase_if(pattern.tracks, [](const Track& t) { return t.hits() == 0; });
    return pattern;
}

Beat generate(const BeatSpec& spec) {
    Beat beat;
    beat.spec = resolve(spec);
    beat.pattern = compose(beat.spec);
    for (const Chord& c : beat.pattern.chords)
        beat.chords.push_back(c.name);
    beat.audio = render(beat.pattern, beat.spec);
    return beat;
}

Beat from_pattern(Pattern pattern, const BeatSpec& spec) {
    if (pattern.bars() <= 0 || pattern.tracks.empty())
        throw std::invalid_argument("from_pattern: the pattern is empty");
    BeatSpec wanted = spec;
    if (wanted.bpm == 0) wanted.bpm = pattern.bpm;
    if (wanted.swing < 0.0) wanted.swing = pattern.swing;
    if (wanted.key.empty()) wanted.key = pattern.key;
    if (wanted.title.empty()) wanted.title = pattern.title;
    if (wanted.bars <= 0) wanted.bars = pattern.bars();

    Beat beat;
    beat.spec = resolve(wanted);
    pattern.resize(beat.spec.bars);
    if (pattern.chords.empty())
        pattern.chords = progression(beat.spec.style, *parse_key(beat.spec.key), beat.spec.seed);
    pattern.bpm = beat.spec.bpm;
    pattern.swing = beat.spec.swing;
    pattern.key = beat.spec.key;
    pattern.title = beat.spec.title;
    for (const Chord& c : pattern.chords)
        beat.chords.push_back(c.name);
    beat.audio = render(pattern, beat.spec);
    beat.pattern = std::move(pattern);
    return beat;
}

} // namespace street::beats
