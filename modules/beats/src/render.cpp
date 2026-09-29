// The sequencer: turns a Pattern into audio.
//
//   schedule   steps → timed events (swing, rolls, groove drift, jitter, velocity)
//   voices     events → one mono track at a time, added into a scratch buffer
//   mix        track → drum or music bus with gain and constant-power pan
//   buses      style effects (SP-1200 crush, drive, low-pass, tape wow, auto-pan)
//   master     sum + vinyl → DC block, loudness, look-ahead limiter, -1 dBFS

#include "kit.hpp"
#include "streams.hpp"
#include "street/beats/beats.hpp"
#include "street/beats/dsp.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace street::beats {
namespace {

using detail::Kit;
using detail::Stream;
using detail::stream;

struct Event {
    std::size_t onset = 0; ///< frame
    float velocity = 0.0f;
    int note = 0;
    bool slide = false;
    int bar = 0;
    bool ring = false;
};

struct Timing {
    double bpm = 90.0;
    double swing = 0.5;
    int sample_rate = kSampleRate;
    double step_frames() const noexcept { return sample_rate * 60.0 / bpm / 4.0; }
};

bool is_backbeat(Instrument i) noexcept {
    return i == Instrument::Snare || i == Instrument::Clap || i == Instrument::Rim;
}

bool locked_to_grid(Instrument i) noexcept {
    // Kick and bass share the groove drift but no private jitter, so they never flam.
    return i == Instrument::Kick || i == Instrument::Bass || i == Instrument::Keys;
}

std::vector<Event> schedule(const Track& track, int bars, const Timing& timing, const Kit& kit,
                            const std::vector<double>& drift_ms, bool humanize,
                            std::uint64_t seed) {
    std::vector<Event> events;
    if (track.steps.empty()) return events;
    Rng human(mix64(seed) + static_cast<std::uint64_t>(track.instrument), stream(Stream::Human));
    const double step = timing.step_frames();
    const double ms = timing.sample_rate / 1000.0;
    const int total = bars * Pattern::kStepsPerBar;

    for (int g = 0; g < total; ++g) {
        const Step& s = track.steps[static_cast<std::size_t>(g) % track.steps.size()];
        if (!s.active()) continue;
        double t = step_time(g, timing.bpm, timing.swing) * timing.sample_rate;
        double v = s.velocity;
        if (humanize) {
            t += drift_ms[static_cast<std::size_t>(g)] * ms;
            if (!locked_to_grid(track.instrument))
                t += std::clamp(human.normal(0.0, kit.jitter_ms), -2.5 * kit.jitter_ms,
                                2.5 * kit.jitter_ms) *
                     ms;
            if (is_backbeat(track.instrument)) t += kit.backbeat_lag_ms * ms;
            v *= 1.0 + std::clamp(human.normal(0.0, kit.velocity_spread), -0.25, 0.25);
        }
        const auto velocity = static_cast<float>(std::clamp(v, 0.05, 1.0));
        const int bar = g / Pattern::kStepsPerBar;
        if (s.roll >= 2) {
            // Rolls sit on a straight grid inside the step and crescendo into the next hit.
            for (int k = 0; k < s.roll; ++k) {
                const double at = t + k * step / s.roll;
                const float ramp =
                    0.55f + 0.45f * static_cast<float>(k + 1) / static_cast<float>(s.roll);
                events.push_back({static_cast<std::size_t>(std::max(0.0, at)), velocity * ramp,
                                  s.note, s.slide, bar, s.ring});
            }
        } else {
            events.push_back({static_cast<std::size_t>(std::max(0.0, t)), velocity, s.note, s.slide,
                              bar, s.ring});
        }
    }
    std::stable_sort(events.begin(), events.end(),
                     [](const Event& a, const Event& b) { return a.onset < b.onset; });
    return events;
}

/// Span from `onset` to `limit` (exclusive), clipped to the buffer.
std::span<float> from(std::vector<float>& buffer, std::size_t onset, std::size_t limit) {
    limit = std::min(limit, buffer.size());
    if (onset >= limit) return {};
    return std::span<float>(buffer).subspan(onset, limit - onset);
}

/// Onset of the first event strictly after `onset` in a sorted list, or `fallback`.
std::size_t next_onset(const std::vector<Event>& events, std::size_t onset, std::size_t fallback) {
    const auto it = std::upper_bound(events.begin(), events.end(), onset,
                                     [](std::size_t t, const Event& e) { return t < e.onset; });
    return it == events.end() ? fallback : it->onset;
}

struct Harmony {
    std::vector<Chord> chords;
    const Chord& at(int bar) const { return chords[static_cast<std::size_t>(bar) % chords.size()]; }
};

void render_track(std::vector<float>& out, Instrument instrument, const std::vector<Event>& events,
                  const std::vector<Event>& closed_hats, const Harmony& harmony, const Kit& kit,
                  const Timing& timing, Rng& noise) {
    const int sr = timing.sample_rate;
    const std::size_t end = out.size();
    switch (instrument) {
    case Instrument::Kick:
        for (const Event& e : events)
            render_kick(from(out, e.onset, end), e.velocity, kit.kick, sr);
        break;
    case Instrument::Snare:
        for (const Event& e : events)
            render_snare(from(out, e.onset, end), e.velocity, kit.snare, sr, noise);
        break;
    case Instrument::Clap:
        for (const Event& e : events)
            render_clap(from(out, e.onset, end), e.velocity, kit.clap, sr, noise);
        break;
    case Instrument::Rim:
        for (const Event& e : events)
            render_rim(from(out, e.onset, end), e.velocity, kit.rim, sr);
        break;
    case Instrument::HatClosed:
        for (const Event& e : events)
            render_hat(from(out, e.onset, end), e.velocity, false, kit.hat, sr, noise);
        break;
    case Instrument::HatOpen:
        // Like the 808's hat circuit, an open hat is choked by the next hat of either kind.
        for (const Event& e : events) {
            const std::size_t choke =
                std::min(next_onset(closed_hats, e.onset, end), next_onset(events, e.onset, end));
            render_hat(from(out, e.onset, choke), e.velocity, true, kit.hat, sr, noise);
        }
        break;
    case Instrument::Shaker:
        for (const Event& e : events)
            render_shaker(from(out, e.onset, end), e.velocity, kit.shaker, sr, noise);
        break;
    case Instrument::Tom:
        for (const Event& e : events)
            render_tom(from(out, e.onset, end), e.velocity, kit.tom.hz * std::exp2(e.note / 12.0),
                       kit.tom, sr);
        break;
    case Instrument::Bass: {
        const auto max_len = static_cast<std::size_t>(kit.bass_max_steps * timing.step_frames());
        std::vector<BassNote> notes;
        for (std::size_t i = 0; i < events.size(); ++i) {
            const Event& e = events[i];
            const std::size_t next = i + 1 < events.size() ? events[i + 1].onset : end;
            notes.push_back({e.onset, std::min(next - e.onset, max_len),
                             midi_to_hz(harmony.at(e.bar).bass_note() + e.note), e.velocity,
                             e.slide});
        }
        render_bass_line(out, notes, kit.bass, sr);
        break;
    }
    case Instrument::Cowbell:
        for (const Event& e : events)
            render_cowbell(from(out, e.onset, end), e.velocity, kit.cowbell, sr);
        break;
    case Instrument::Ride:
        for (const Event& e : events)
            render_cymbal(from(out, e.onset, end), e.velocity, kit.ride, sr, noise);
        break;
    case Instrument::Crash:
        for (const Event& e : events)
            render_cymbal(from(out, e.onset, end), e.velocity, kit.crash, sr, noise);
        break;
    case Instrument::Guitar: {
        // Power chords (root, fifth, octave) from E2 up; chugs are gated to one step, open
        // chords ring until the next hit. Distortion is applied to the summed track later,
        // because the intermodulation between the chord tones is what makes it crunch.
        const auto step = static_cast<std::size_t>(timing.step_frames());
        const auto max_ring = static_cast<std::size_t>(16 * timing.step_frames());
        for (std::size_t i = 0; i < events.size(); ++i) {
            const Event& e = events[i];
            const std::size_t next = i + 1 < events.size() ? events[i + 1].onset : end;
            const std::size_t gate = std::min(next - e.onset, e.ring ? max_ring : step);
            const int root = 40 + ((harmony.at(e.bar).root + e.note - 4) % 12 + 12) % 12;
            for (int interval : {0, 7, 12})
                render_synth_note(from(out, e.onset, end), midi_to_hz(root + interval), e.velocity,
                                  gate, e.ring ? kit.guitar_ring : kit.guitar_mute, sr);
        }
        break;
    }
    case Instrument::Keys: {
        const auto max_len = static_cast<std::size_t>(16 * timing.step_frames());
        const auto strum = static_cast<std::size_t>(kit.strum_ms * sr / 1000.0);
        for (std::size_t i = 0; i < events.size(); ++i) {
            const Event& e = events[i];
            const std::size_t next = i + 1 < events.size() ? events[i + 1].onset : end;
            // Synth stabs are short and punchy; the piano rings until the next chord.
            const std::size_t limit =
                kit.synth_keys ? static_cast<std::size_t>(2 * timing.step_frames()) : max_len;
            const std::size_t gate = std::min(next - e.onset, limit);
            const std::vector<int> notes = harmony.at(e.bar).voicing();
            for (std::size_t n = 0; n < notes.size(); ++n) {
                // Strummed upwards; inner voices a touch softer so the top note sings.
                const std::size_t onset = e.onset + n * strum;
                const float v = e.velocity * (n + 1 == notes.size() ? 1.0f : 0.8f);
                Wow wow = kit.wow;
                wow.origin = onset;
                const std::size_t held = gate > n * strum ? gate - n * strum : 0;
                if (kit.synth_keys)
                    render_synth_note(from(out, onset, end), midi_to_hz(notes[n]), v, held,
                                      kit.stab, sr);
                else
                    render_keys_note(from(out, onset, end), midi_to_hz(notes[n]), v, held, kit.keys,
                                     sr, wow);
            }
        }
        break;
    }
    }
}

/// Adds a mono track to a stereo bus. Keys get a slow constant-power auto-pan.
void mix_into(AudioBuffer& bus, const std::vector<float>& track, Instrument instrument,
              const Kit& kit) {
    const detail::Channel& ch = kit.channel(instrument);
    if (instrument == Instrument::Guitar && kit.guitar_double_ms > 0.0) {
        // Fake double-tracking: the dry take hard left, a copy a few ms later hard right.
        // The Haas effect turns the delay into width rather than an audible echo.
        const auto delay =
            static_cast<std::size_t>(kit.guitar_double_ms * bus.sample_rate / 1000.0);
        for (std::size_t i = 0; i < track.size(); ++i) {
            bus.left[i] += track[i] * ch.gain * 0.8f;
            if (i >= delay) bus.right[i] += track[i - delay] * ch.gain * 0.8f;
        }
        return;
    }
    if (instrument == Instrument::Keys && kit.keys_autopan > 0.0f) {
        // The LFO moves a fraction of a degree per millisecond, so the pan law is only
        // re-evaluated every 64 samples (~1.5 ms).
        constexpr std::size_t kBlock = 64;
        const double w = dsp::kTwoPi * kit.keys_autopan_hz / bus.sample_rate;
        for (std::size_t start = 0; start < track.size(); start += kBlock) {
            const float pan = ch.pan + kit.keys_autopan * dsp::sine(w * static_cast<double>(start));
            const dsp::PanGains g = dsp::pan_gains(pan);
            for (std::size_t i = start; i < std::min(track.size(), start + kBlock); ++i) {
                bus.left[i] += track[i] * ch.gain * g.left;
                bus.right[i] += track[i] * ch.gain * g.right;
            }
        }
        return;
    }
    const dsp::PanGains g = dsp::pan_gains(ch.pan);
    for (std::size_t i = 0; i < track.size(); ++i) {
        bus.left[i] += track[i] * ch.gain * g.left;
        bus.right[i] += track[i] * ch.gain * g.right;
    }
}

/// Guitar amp: tighten the lows, clip hard, then roll off like a 4x12 speaker cabinet.
void amp_and_cabinet(std::vector<float>& track, const Kit& kit) {
    auto tighten = dsp::Biquad::highpass(90.0, 0.7, kSampleRate);
    auto cabinet = dsp::Biquad::lowpass(kit.guitar_lowpass_hz, 0.9, kSampleRate);
    auto fizz = dsp::Biquad::lowpass(kit.guitar_lowpass_hz * 1.6, 0.707, kSampleRate);
    const dsp::Saturator amp(kit.guitar_drive);
    for (float& s : track)
        s = fizz.process(cabinet.process(amp(tighten.process(s))));
}

void lowpass(AudioBuffer& bus, double hz) {
    auto l = dsp::Biquad::lowpass(hz, 0.707, bus.sample_rate);
    auto r = dsp::Biquad::lowpass(hz, 0.707, bus.sample_rate);
    for (std::size_t i = 0; i < bus.frames(); ++i) {
        bus.left[i] = l.process(bus.left[i]);
        bus.right[i] = r.process(bus.right[i]);
    }
}

void crush(AudioBuffer& bus, int bits, double rate_hz) {
    dsp::BitCrusher l(bits, rate_hz, bus.sample_rate), r(bits, rate_hz, bus.sample_rate);
    for (std::size_t i = 0; i < bus.frames(); ++i) {
        bus.left[i] = l.process(bus.left[i]);
        bus.right[i] = r.process(bus.right[i]);
    }
}

void drive(AudioBuffer& bus, float amount) {
    const dsp::Saturator shape(amount);
    for (std::size_t i = 0; i < bus.frames(); ++i) {
        bus.left[i] = shape(bus.left[i]);
        bus.right[i] = shape(bus.right[i]);
    }
}

} // namespace

AudioBuffer render(const Pattern& pattern, const BeatSpec& spec) {
    if (pattern.bars() <= 0 || pattern.tracks.empty())
        throw std::invalid_argument("render: the pattern is empty");

    // Explicit spec values win, then the pattern's directives, then the style (seeded).
    BeatSpec wanted = spec;
    if (wanted.bpm == 0) wanted.bpm = pattern.bpm;
    if (wanted.swing < 0.0) wanted.swing = pattern.swing;
    if (wanted.key.empty()) wanted.key = pattern.key;
    if (wanted.bars <= 0) wanted.bars = pattern.bars();
    const BeatSpec r = resolve(wanted);

    const Kit& kit = detail::kit_for(r.style);
    const Timing timing{static_cast<double>(r.bpm), r.swing, kSampleRate};
    Harmony harmony{pattern.chords};
    if (harmony.chords.empty()) harmony.chords = progression(r.style, *parse_key(r.key), r.seed);

    // One extra beat of tail lets the last hits ring out under the fade.
    const int steps = r.bars * Pattern::kStepsPerBar;
    const auto frames = static_cast<std::size_t>(std::ceil((steps + 4) * timing.step_frames()));
    AudioBuffer drums(frames), music(frames);

    // Groove drift is shared by every track at a given step: the band leans together.
    std::vector<double> drift(static_cast<std::size_t>(steps), 0.0);
    Rng timing_rng(r.seed, stream(Stream::Timing));
    for (double& d : drift)
        d = std::clamp(timing_rng.normal(0.0, kit.groove_ms), -2.5 * kit.groove_ms,
                       2.5 * kit.groove_ms);

    std::array<std::vector<Event>, kInstrumentCount> events;
    for (const Track& t : pattern.tracks)
        events[static_cast<std::size_t>(t.instrument)] =
            schedule(t, r.bars, timing, kit, drift, r.humanize, r.seed);

    Rng noise(r.seed, stream(Stream::Noise));
    std::vector<float> scratch(frames);
    for (Instrument instrument : kAllInstruments) {
        const auto& list = events[static_cast<std::size_t>(instrument)];
        if (list.empty()) continue;
        std::fill(scratch.begin(), scratch.end(), 0.0f);
        render_track(scratch, instrument, list,
                     events[static_cast<std::size_t>(Instrument::HatClosed)], harmony, kit, timing,
                     noise);
        if (instrument == Instrument::Keys && kit.keys_lowpass_hz > 0.0) {
            auto lp = dsp::Biquad::lowpass(kit.keys_lowpass_hz, 0.707, kSampleRate);
            for (float& s : scratch)
                s = lp.process(s);
        }
        if (instrument == Instrument::Guitar) amp_and_cabinet(scratch, kit);
        // Keys ride with the drums through the crusher (they're "the sample"); bass stays clean.
        const bool melodic = instrument == Instrument::Bass || instrument == Instrument::Guitar;
        mix_into(melodic ? music : drums, scratch, instrument, kit);
    }

    if (kit.drum_drive > 0.0f) drive(drums, kit.drum_drive);
    if (kit.crush_bits > 0) crush(drums, kit.crush_bits, kit.crush_rate_hz);

    AudioBuffer out(frames);
    for (std::size_t i = 0; i < frames; ++i) {
        out.left[i] = drums.left[i] + music.left[i];
        out.right[i] = drums.right[i] + music.right[i];
    }
    Rng vinyl(r.seed, stream(Stream::Vinyl));
    render_vinyl(out.left, out.right, kit.vinyl, kSampleRate, vinyl);
    if (kit.master_lowpass_hz > 0.0) lowpass(out, kit.master_lowpass_hz);

    dsp::master(out, -1.0, 4 * timing.step_frames() / kSampleRate);
    return out;
}

} // namespace street::beats
