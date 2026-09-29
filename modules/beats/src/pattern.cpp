#include "street/beats/pattern.hpp"

#include "street/core/text.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <format>
#include <stdexcept>

namespace street::beats {
namespace {

struct InstrumentName {
    Instrument instrument;
    std::string_view id, label, aliases;
};

constexpr std::array<InstrumentName, kInstrumentCount> kNames = {{
    {Instrument::Kick, "kick", "kick", "bd"},
    {Instrument::Snare, "snare", "snare", "sd"},
    {Instrument::Clap, "clap", "clap", "cp"},
    {Instrument::Rim, "rim", "rim", "rs rimshot"},
    {Instrument::Cowbell, "cowbell", "cowbell", "cb bell"},
    {Instrument::HatClosed, "hat", "hat", "hh ch hihat closedhat"},
    {Instrument::HatOpen, "openhat", "open hat", "oh"},
    {Instrument::Ride, "ride", "ride", "rd"},
    {Instrument::Crash, "crash", "crash", "cy cr cymbal"},
    {Instrument::Shaker, "shaker", "shaker", "sh"},
    {Instrument::Tom, "tom", "tom", "lt"},
    {Instrument::Bass, "bass", "bass", "808"},
    {Instrument::Guitar, "guitar", "guitar", "gtr riff"},
    {Instrument::Keys, "keys", "keys", "chords ep piano"},
}};

const InstrumentName& names_of(Instrument instrument) noexcept {
    return kNames[static_cast<std::size_t>(instrument)];
}

constexpr float kAccent = 1.0f;
constexpr float kHit = 0.72f;
constexpr float kGhost = 0.35f;

/// A '#' opens a comment at the start of a line or after whitespace; elsewhere it is a
/// sharp ('key C#m', 'chords F#m9').
std::size_t comment_start(std::string_view line) {
    for (std::size_t i = 0; i < line.size(); ++i)
        if (line[i] == '#' && (i == 0 || line[i - 1] == ' ' || line[i - 1] == '	')) return i;
    return line.size();
}

[[noreturn]] void fail(int line, std::string_view message) {
    throw std::invalid_argument(std::format("pattern line {}: {}", line, message));
}

/// Translates one step character; nullopt for characters that are not steps.
std::optional<Step> step_from(char32_t c, Instrument instrument) {
    const bool bass = instrument == Instrument::Bass;
    switch (c) {
    case U'.':
    case U'-':
    case U'_':
    case U'·':
        return Step{};
    case U'x':
    case U'X':
        return Step{kAccent};
    case U'o':
    case U'O':
        return Step{kHit};
    case U'g':
        return Step{kGhost};
    case U'r':
        return Step{kHit, 2};
    case U't':
        return Step{kHit, 3};
    case U'R':
        return Step{kAccent, 2};
    case U'T':
        return Step{kAccent, 3};
    case U'^':
        return Step{kHit, 0, 12, bass};
    case U'~':
        return Step{kHit, 0, 0, bass};
    case U'*':
        return Step{kAccent, 0, 0, false, instrument == Instrument::Guitar};
    default:
        return std::nullopt;
    }
}

int parse_bpm(std::string_view value, int line) {
    const std::string s(value);
    char* end = nullptr;
    errno = 0;
    const long bpm = std::strtol(s.c_str(), &end, 10);
    if (s.empty() || end != s.c_str() + s.size() || errno == ERANGE || bpm < 40 || bpm > 300)
        fail(line, std::format("bpm must be a whole number from 40 to 300, got '{}'", value));
    return static_cast<int>(bpm);
}

double parse_swing(std::string_view value, int line) {
    std::string s(value);
    const bool percent = !s.empty() && s.back() == '%';
    if (percent) s.pop_back();
    char* end = nullptr;
    errno = 0;
    double swing = std::strtod(s.c_str(), &end);
    const bool numeric = !s.empty() && end == s.c_str() + s.size() && errno != ERANGE;
    if (numeric && (percent || swing > 1.0)) swing /= 100.0;
    if (!numeric || swing < 0.5 || swing > 0.8)
        fail(line, std::format("swing must be 0.5-0.8 or 50%-80%, got '{}'", value));
    return swing;
}

} // namespace

std::string_view instrument_id(Instrument instrument) noexcept {
    return names_of(instrument).id;
}
std::string_view instrument_label(Instrument instrument) noexcept {
    return names_of(instrument).label;
}

std::optional<Instrument> parse_instrument(std::string_view name) {
    const std::string lower = street::text::to_lower(street::text::trim(name));
    for (const auto& n : kNames) {
        if (lower == n.id) return n.instrument;
        for (const auto& alias : street::text::split(n.aliases, ' ', true))
            if (lower == alias) return n.instrument;
    }
    return std::nullopt;
}

int Track::hits() const noexcept {
    return static_cast<int>(
        std::count_if(steps.begin(), steps.end(), [](const Step& s) { return s.active(); }));
}

void Pattern::resize(int bars) {
    bars_ = std::max(0, bars);
    const auto total = static_cast<std::size_t>(total_steps());
    for (auto& t : tracks) {
        const std::size_t old = t.steps.size();
        t.steps.resize(total);
        if (old > 0)
            for (std::size_t i = old; i < total; ++i)
                t.steps[i] = t.steps[i % old];
    }
}

const Track* Pattern::find(Instrument instrument) const noexcept {
    const auto it = std::find_if(tracks.begin(), tracks.end(),
                                 [&](const Track& t) { return t.instrument == instrument; });
    return it == tracks.end() ? nullptr : &*it;
}

Track& Pattern::track(Instrument instrument) {
    const auto it = std::find_if(tracks.begin(), tracks.end(),
                                 [&](const Track& t) { return t.instrument >= instrument; });
    if (it != tracks.end() && it->instrument == instrument) return *it;
    Track fresh{instrument, std::vector<Step>(static_cast<std::size_t>(total_steps()))};
    return *tracks.insert(it, std::move(fresh));
}

Step& Pattern::at(Instrument instrument, int bar, int step) {
    Track& t = track(instrument);
    const int index = bar * kStepsPerBar + step;
    if (bar < 0 || bar >= bars_ || step < 0 || step >= kStepsPerBar)
        throw std::out_of_range(
            std::format("Pattern::at: bar {} step {} is outside the pattern", bar, step));
    return t.steps[static_cast<std::size_t>(index)];
}

const Chord* Pattern::chord_at(int bar) const noexcept {
    if (chords.empty()) return nullptr;
    const auto n = static_cast<int>(chords.size());
    return &chords[static_cast<std::size_t>(((bar % n) + n) % n)];
}

Pattern parse_pattern(std::string_view text) {
    Pattern pattern;
    std::vector<Track> tracks;  // in order of first appearance
    std::vector<int> last_line; // line where each track was last extended
    int line_no = 0;

    for (const std::string& raw : street::text::split_lines(text)) {
        ++line_no;
        const std::string_view line =
            street::text::trim(std::string_view(raw).substr(0, comment_start(raw)));
        if (line.empty()) continue;

        const std::size_t word_end = std::min(line.find_first_of(" \t|"), line.size());
        const std::string word = street::text::to_lower(line.substr(0, word_end));
        const std::string_view rest = street::text::trim(line.substr(word_end));

        if (word == "title" || word == "bpm" || word == "tempo" || word == "swing" ||
            word == "key" || word == "chords" || word == "style") {
            if (rest.empty()) fail(line_no, std::format("'{}' needs a value", word));
            if (word == "title") {
                pattern.title = std::string(rest);
            } else if (word == "bpm" || word == "tempo") {
                pattern.bpm = parse_bpm(rest, line_no);
            } else if (word == "swing") {
                pattern.swing = parse_swing(rest, line_no);
            } else if (word == "style") {
                pattern.style = parse_style(rest);
                if (!pattern.style)
                    fail(line_no, std::format("unknown style '{}' ({})", rest, style_list()));
            } else if (word == "key") {
                const auto key = parse_key(rest);
                if (!key) fail(line_no, std::format("unknown key '{}' (try Am, C#m, F, Bb)", rest));
                pattern.key = key->name();
            } else {
                pattern.chords.clear();
                std::string symbols(rest);
                std::replace(symbols.begin(), symbols.end(), '|', ' ');
                std::replace(symbols.begin(), symbols.end(), '\t', ' ');
                for (const auto& symbol : street::text::split(symbols, ' ', true)) {
                    try {
                        pattern.chords.push_back(parse_chord(symbol));
                    } catch (const std::invalid_argument& e) {
                        fail(line_no, e.what());
                    }
                }
            }
            continue;
        }

        const auto instrument = parse_instrument(word);
        if (!instrument) {
            std::string known;
            for (const auto& n : kNames)
                known += (known.empty() ? "" : ", ") + std::string(n.id);
            fail(line_no,
                 std::format("unknown instrument or directive '{}' (instruments: {}; directives: "
                             "title, bpm, swing, key, style, chords)",
                             word, known));
        }

        auto it = std::find_if(tracks.begin(), tracks.end(),
                               [&](const Track& t) { return t.instrument == *instrument; });
        if (it == tracks.end()) {
            tracks.push_back({*instrument, {}});
            last_line.push_back(line_no);
            it = tracks.end() - 1;
        }
        last_line[static_cast<std::size_t>(it - tracks.begin())] = line_no;

        // Column numbers count code points from the start of the raw line, as editors do.
        const auto prefix = static_cast<std::size_t>(rest.data() - raw.data());
        const std::size_t offset =
            street::text::utf8_length(std::string_view(raw).substr(0, prefix));
        const std::u32string steps = street::text::utf8_decode(rest);
        for (std::size_t i = 0; i < steps.size(); ++i) {
            const char32_t c = steps[i];
            if (c == U' ' || c == U'\t' || c == U'|') continue;
            const auto step = step_from(c, *instrument);
            if (!step) {
                std::string shown;
                street::text::utf8_append(shown, c);
                fail(
                    line_no,
                    std::format(
                        "column {}: unexpected '{}' in the {} track (steps: x o g . r t R T ^ ~ *)",
                        offset + i + 1, shown, instrument_id(*instrument)));
            }
            it->steps.push_back(*step);
        }
    }

    if (tracks.empty())
        throw std::invalid_argument("pattern has no tracks (e.g. 'kick | x...x...x...x... |')");

    int bars = 0;
    for (std::size_t i = 0; i < tracks.size(); ++i) {
        const std::size_t n = tracks[i].steps.size();
        if (n == 0 || n % Pattern::kStepsPerBar != 0)
            fail(last_line[i],
                 std::format("the {} track has {} steps; tracks need whole bars of {} steps",
                             instrument_id(tracks[i].instrument), n, Pattern::kStepsPerBar));
        bars = std::max(bars, static_cast<int>(n / Pattern::kStepsPerBar));
    }
    std::sort(tracks.begin(), tracks.end(),
              [](const Track& a, const Track& b) { return a.instrument < b.instrument; });
    pattern.tracks = std::move(tracks);
    pattern.resize(bars); // loops shorter tracks
    return pattern;
}

double step_time(int step, double bpm, double swing) noexcept {
    const double step_seconds = 60.0 / bpm / 4.0;
    double t = step * step_seconds;
    if (step % 2 != 0) t += (2.0 * swing - 1.0) * step_seconds;
    return t;
}

} // namespace street::beats
