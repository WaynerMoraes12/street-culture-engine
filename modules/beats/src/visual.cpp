// Text renderers: step-sequencer grid, braille waveform, beat summary, style table.
// All pure functions of their inputs; plain text when colors are disabled.

#include "street/beats/beats.hpp"
#include "street/core/table.hpp"
#include "street/core/term.hpp"
#include "street/core/text.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <vector>

namespace street::beats {
namespace {

namespace pal = term::palette;

constexpr std::size_t kLabelWidth = 11; // "  open hat "
constexpr term::Rgb kRest{70, 70, 84};
constexpr term::Rgb kRestDownbeat{120, 120, 136};

term::Rgb color_of(Instrument i) noexcept {
    switch (i) {
    case Instrument::Kick:
        return pal::red;
    case Instrument::Snare:
        return pal::gold;
    case Instrument::Clap:
        return pal::pink;
    case Instrument::Rim:
        return pal::cement;
    case Instrument::HatClosed:
        return pal::cyan;
    case Instrument::HatOpen:
        return pal::teal;
    case Instrument::Shaker:
        return pal::green;
    case Instrument::Tom:
        return pal::hardwood;
    case Instrument::Bass:
        return pal::purple;
    case Instrument::Keys:
        return pal::blue;
    case Instrument::Cowbell:
        return term::Rgb{250, 204, 21};
    case Instrument::Ride:
        return term::Rgb{180, 200, 255};
    case Instrument::Crash:
        return pal::white;
    case Instrument::Guitar:
        return pal::orange;
    }
    return pal::white;
}

/// Absolute pitch of a tuned step, for drawing slide direction.
int pitch_of(const Pattern& p, Instrument i, int global_step) {
    const Step& s = p.find(i)->steps[static_cast<std::size_t>(global_step)];
    const Chord* chord = p.chord_at(global_step / Pattern::kStepsPerBar);
    return (chord != nullptr ? chord->bass_note() : 36) + s.note;
}

std::string cell(const Pattern& p, const Track& t, int g, int& last_pitch) {
    const Step& s = t.steps[static_cast<std::size_t>(g)];
    if (!s.active()) return term::paint("·", g % 4 == 0 ? kRestDownbeat : kRest);

    std::string_view glyph = s.velocity >= 0.85f ? "●" : s.velocity >= 0.5f ? "•" : "○";
    if (s.roll == 2) glyph = "=";
    if (s.roll >= 3) glyph = "≡";
    if (s.ring) glyph = "█"; // open chord left ringing
    if (t.instrument == Instrument::Bass) {
        const int pitch = pitch_of(p, t.instrument, g);
        if (s.slide && s.roll < 2)
            glyph = pitch > last_pitch ? "╱" : pitch < last_pitch ? "╲" : "─";
        last_pitch = pitch;
    }
    const term::Rgb base = color_of(t.instrument);
    const term::Rgb shade =
        term::lerp(term::lerp(base, kRest, 0.7), base, 0.25 + 0.75 * s.velocity);
    return s.velocity >= 0.85f ? term::paint_bold(glyph, shade) : term::paint(glyph, shade);
}

std::string swing_feel(double swing) {
    if (swing < 0.53) return "straight";
    if (swing < 0.57) return "light";
    if (swing < 0.61) return "lazy";
    if (swing < 0.65) return "heavy";
    return "shuffle";
}

/// Greedy word wrap for plain text.
std::vector<std::string> wrap(std::string_view text, std::size_t width) {
    std::vector<std::string> lines(1);
    for (const std::string& word : text::split(text, ' ', true)) {
        if (!lines.back().empty() && term::width(lines.back()) + 1 + term::width(word) > width)
            lines.emplace_back();
        if (!lines.back().empty()) lines.back() += ' ';
        lines.back() += word;
    }
    return lines;
}

// Braille dots: bit for (column 0..1, row 0..3) within a cell.
constexpr std::array<std::array<unsigned, 4>, 2> kDots = {
    {{0x01, 0x02, 0x04, 0x40}, {0x08, 0x10, 0x20, 0x80}}};

} // namespace

std::string render_grid(const Pattern& pattern) {
    return render_grid(pattern, GridOptions{});
}

std::string render_grid(const Pattern& pattern, const GridOptions& options) {
    const int total_bars = pattern.bars();
    const int shown = options.max_bars > 0 ? std::min(total_bars, options.max_bars) : total_bars;
    const int per_row = std::max(1, options.bars_per_row);
    const term::Rgb frame = pal::dark;

    std::string out;
    for (int first = 0; first < shown; first += per_row) {
        const int last = std::min(shown, first + per_row);
        if (first > 0) out += '\n';

        std::string header(kLabelWidth, ' ');
        for (int bar = first; bar < last; ++bar) {
            const Chord* chord = pattern.chord_at(bar);
            std::string label = term::paint_bold(std::to_string(bar + 1), pal::gold);
            if (chord != nullptr) label += " " + term::paint(chord->name, pal::gray);
            header += bar + 1 < last ? term::pad_right(" " + label, 20) : " " + label;
        }
        out += header + '\n';

        for (const Track& t : pattern.tracks) {
            // Hide tracks that are silent in this block (e.g. drums during the intro bar).
            bool any = false;
            for (int g = first * Pattern::kStepsPerBar; g < last * Pattern::kStepsPerBar && !any;
                 ++g)
                any = t.steps[static_cast<std::size_t>(g)].active();
            if (!any) continue;

            std::string row = term::paint_bold(
                term::pad_right("  " + std::string(instrument_label(t.instrument)), kLabelWidth),
                color_of(t.instrument));
            int last_pitch = 0;
            if (t.instrument == Instrument::Bass && first > 0)
                for (int g = first * Pattern::kStepsPerBar; g-- > 0;)
                    if (t.steps[static_cast<std::size_t>(g)].active()) {
                        last_pitch = pitch_of(pattern, t.instrument, g);
                        break;
                    }
            for (int bar = first; bar < last; ++bar) {
                row += term::paint("│", frame);
                for (int s = 0; s < Pattern::kStepsPerBar; ++s) {
                    if (s > 0 && s % 4 == 0) row += ' ';
                    row += cell(pattern, t, bar * Pattern::kStepsPerBar + s, last_pitch);
                }
            }
            out += row + term::paint("│", frame) + '\n';
        }
    }
    if (shown < total_bars)
        out +=
            term::paint(std::format("{}… {} more bar{} not shown\n", std::string(kLabelWidth, ' '),
                                    total_bars - shown, total_bars - shown == 1 ? "" : "s"),
                        pal::gray);
    out += '\n' +
           term::paint(
               "  ● accent  • hit  ○ ghost  = 1/32 roll  ≡ triplet roll  ╱ ╲ bass slide  █ let ring",
               pal::gray) +
           '\n';
    return out;
}

std::string render_waveform(const AudioBuffer& audio, int width, int height) {
    width = std::max(1, width);
    height = std::max(1, height);
    const int columns = width * 2;
    const int dots = height * 4;
    const std::size_t frames = audio.frames();

    // Peak per time slice, relative to the loudest slice (linear, like a DAW overview).
    std::vector<double> level(static_cast<std::size_t>(columns), 0.0);
    for (int c = 0; c < columns && frames > 0; ++c) {
        const std::size_t begin =
            frames * static_cast<std::size_t>(c) / static_cast<std::size_t>(columns);
        const std::size_t end = std::max(begin + 1, frames * static_cast<std::size_t>(c + 1) /
                                                        static_cast<std::size_t>(columns));
        float p = 0.0f;
        for (std::size_t i = begin; i < std::min(end, frames); ++i)
            p = std::max({p, std::abs(audio.left[i]), std::abs(audio.right[i])});
        level[static_cast<std::size_t>(c)] = p;
    }
    const double loudest_slice = std::max(1e-9, *std::max_element(level.begin(), level.end()));
    for (double& v : level)
        v /= loudest_slice;

    constexpr std::array<term::Rgb, 3> kHeat = {term::Rgb{110, 80, 200}, pal::pink, pal::gold};
    const auto heat = [&](double t) {
        const double scaled = std::clamp(t, 0.0, 1.0) * 2.0;
        const auto seg = std::min<std::size_t>(static_cast<std::size_t>(scaled), 1);
        return term::lerp(kHeat[seg], kHeat[seg + 1], scaled - static_cast<double>(seg));
    };

    std::string out;
    const double half = dots / 2.0;
    for (int row = 0; row < height; ++row) {
        for (int x = 0; x < width; ++x) {
            unsigned bits = 0;
            double loudest = 0.0;
            for (int sub = 0; sub < 2; ++sub) {
                const double v = level[static_cast<std::size_t>(x * 2 + sub)];
                loudest = std::max(loudest, v);
                // Symmetric about the centre line; always at least the centre dots.
                const int reach = std::max(1, static_cast<int>(std::lround(v * half)));
                for (int r = 0; r < 4; ++r) {
                    const int d = row * 4 + r;
                    const double centre_distance = d + 0.5 - half;
                    if (std::abs(centre_distance) < reach)
                        bits |= kDots[static_cast<std::size_t>(sub)][static_cast<std::size_t>(r)];
                }
            }
            std::string glyph;
            text::utf8_append(glyph, static_cast<char32_t>(0x2800 + bits));
            out += term::paint(glyph, heat(loudest));
        }
        out += '\n';
    }
    return out;
}

std::string render_summary(const Beat& beat) {
    const BeatSpec& s = beat.spec;
    const StyleInfo& style = info(s.style);
    const auto key = parse_key(s.key);
    const double seconds =
        beat.audio.frames() > 0 ? beat.audio.seconds() : s.bars * 4 * 60.0 / std::max(1, s.bpm);
    const auto field = [](std::string_view label, const std::string& value) {
        return term::pad_right(term::paint(std::string(label), pal::gray) + "  " + value, 30);
    };

    std::string chords;
    for (std::size_t i = 0; i < beat.chords.size(); ++i)
        chords +=
            (i ? term::paint(" → ", pal::dark) : "") + term::paint_bold(beat.chords[i], pal::white);

    std::vector<std::string> lines;
    lines.push_back(field("style ", term::paint_bold(std::string(style.name), style.color)) +
                    field("tempo ", std::format("{} BPM", s.bpm)) +
                    field("key ", key ? key->long_name() : s.key));
    lines.push_back(
        field("swing ", std::format("{:.0f}% {}", s.swing * 100.0,
                                    term::paint(swing_feel(s.swing), pal::gray))) +
        field("bars  ", std::format("{} {}", s.bars,
                                    term::paint(std::format("({:.1f} s)", seconds), pal::gray))) +
        field("seed", term::paint_bold(std::to_string(s.seed), pal::gold)));
    if (!chords.empty()) lines.push_back(term::paint("chords", pal::gray) + "  " + chords);
    for (const std::string& line : wrap(style.description, 88))
        lines.push_back(term::paint(line, pal::gray));
    return term::panel("BEAT · " + (s.title.empty() ? std::string("untitled") : s.title), lines,
                       style.color, 88);
}

std::string render_style_list() {
    Table table({"style", "name", "tempo", "swing", "key", "feel"});
    table.accent(pal::purple).align(3, Align::Right);
    for (const StyleInfo& s : styles())
        table.add_row({term::paint_bold(std::string(s.id), s.color), std::string(s.name),
                       std::format("{}-{} BPM", s.bpm_min, s.bpm_max),
                       std::format("{:.0f}%", s.default_swing * 100.0), std::string(s.default_key),
                       term::paint(std::string(s.tagline), pal::gray)});
    return table.render();
}

} // namespace street::beats
