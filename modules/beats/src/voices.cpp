#include "street/beats/voices.hpp"

#include "street/beats/dsp.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace street::beats {
namespace {

using dsp::decay_coefficient;
using dsp::kTwoPi;
using dsp::samples;

/// Envelope decays are cut once they fall below -60 dB (1/e^6.9).
constexpr double kRingOut = 6.9;

/// Length a one-shot may ring: its natural length, capped by the space it was given.
std::size_t voice_length(std::span<const float> out, double natural_seconds, int sample_rate) {
    return std::min(out.size(), samples(natural_seconds, sample_rate));
}

/// Linear fade across the last few milliseconds of a (possibly truncated) voice.
class Declick {
public:
    Declick(std::size_t length, int sample_rate)
        : length_(length), fade_(std::min(length, samples(0.004, sample_rate))) {}
    float operator()(std::size_t i) const noexcept {
        const std::size_t left = length_ - i;
        return left >= fade_ ? 1.0f : static_cast<float>(left) / static_cast<float>(fade_);
    }

private:
    std::size_t length_, fade_;
};

/// Linear attack ramp over `n` samples; avoids the click of starting at full level.
float ramp(std::size_t i, std::size_t n) noexcept {
    return i >= n ? 1.0f : static_cast<float>(i) / static_cast<float>(n);
}

float white(Rng& rng) noexcept {
    return static_cast<float>(rng.uniform(-1.0, 1.0));
}

void advance(double& phase, double hz, int sample_rate) noexcept {
    phase += kTwoPi * hz / sample_rate;
    if (phase >= kTwoPi) phase -= kTwoPi;
}

} // namespace

void render_kick(std::span<float> out, float velocity, const KickParams& p, int sample_rate) {
    const std::size_t length = voice_length(out, p.amp_decay * kRingOut, sample_rate);
    const Declick declick(length, sample_rate);
    const std::size_t attack = samples(0.0012, sample_rate);
    const double pitch_k = decay_coefficient(p.pitch_decay, sample_rate);
    const double amp_k = decay_coefficient(p.amp_decay, sample_rate);
    const double click_k = decay_coefficient(0.0015, sample_rate);
    const double drive_norm = 1.0 / std::tanh(std::max(0.01, double{p.drive}));

    double phase = 0.0, click_phase = 0.0;
    double sweep = 1.0, amp = 1.0, click = p.click;
    for (std::size_t i = 0; i < length; ++i) {
        // Exponential pitch drop: f(t) = end + (start - end)·e^(-t/τ).
        advance(phase, p.end_hz + (p.start_hz - p.end_hz) * sweep, sample_rate);
        advance(click_phase, 1800.0, sample_rate);
        // Driving the *enveloped* signal means the tail cleans up as it decays.
        const double body = std::tanh(p.drive * amp * dsp::sine(phase)) * drive_norm;
        const double value = body + click * dsp::sine(click_phase);
        out[i] += static_cast<float>(value) * velocity * ramp(i, attack) * declick(i);
        sweep *= pitch_k;
        amp *= amp_k;
        click *= click_k;
    }
}

void render_snare(std::span<float> out, float velocity, const SnareParams& p, int sample_rate,
                  Rng& noise) {
    const std::size_t length =
        voice_length(out, std::max(p.tone_decay, p.noise_decay) * kRingOut, sample_rate);
    const Declick declick(length, sample_rate);
    const std::size_t attack = samples(0.0008, sample_rate);
    auto wires = dsp::Biquad::highpass(p.noise_highpass_hz, 0.8, sample_rate);
    const double tone_k = decay_coefficient(p.tone_decay, sample_rate);
    const double noise_k = decay_coefficient(p.noise_decay, sample_rate);
    const double bend_k = decay_coefficient(0.012, sample_rate);
    const dsp::Saturator crack(1.5f);

    double phase1 = 0.0, phase2 = 0.0, tone_env = 1.0, noise_env = 1.0, bend = 0.5;
    for (std::size_t i = 0; i < length; ++i) {
        const double hz = p.tone_hz * (1.0 + bend); // the head is tighter right after the strike
        advance(phase1, hz, sample_rate);
        advance(phase2, hz * 1.6, sample_rate);
        const double tone = (dsp::sine(phase1) + 0.5 * dsp::sine(phase2)) * tone_env;
        const double snap = wires.process(white(noise)) * noise_env * 1.6;
        const double mix = p.tone_mix * tone + (1.0 - p.tone_mix) * snap;
        out[i] += crack(static_cast<float>(mix)) * velocity * ramp(i, attack) * declick(i);
        tone_env *= tone_k;
        noise_env *= noise_k;
        bend *= bend_k;
    }
}

void render_clap(std::span<float> out, float velocity, const ClapParams& p, int sample_rate,
                 Rng& noise) {
    const int bursts = std::clamp(p.bursts, 1, 8);
    const double tail_start = p.spacing * (bursts - 1);
    const std::size_t length = voice_length(out, tail_start + p.tail_decay * kRingOut, sample_rate);
    const Declick declick(length, sample_rate);
    auto body = dsp::Biquad::bandpass(p.center_hz, p.q, sample_rate);
    const dsp::Saturator grit(1.2f);
    const double dt = 1.0 / sample_rate;

    for (std::size_t i = 0; i < length; ++i) {
        const double t = static_cast<double>(i) * dt;
        // Sawtooth-shaped envelope: each burst re-strikes, the last one rings into the tail.
        const int burst = std::min(static_cast<int>(t / p.spacing), bursts - 1);
        const double local = t - burst * p.spacing;
        const double env =
            burst < bursts - 1 ? std::exp(-local / p.burst_decay) : std::exp(-local / p.tail_decay);
        const float shaped = body.process(white(noise)) * static_cast<float>(env * 3.2);
        out[i] += grit(shaped) * velocity * declick(i);
    }
}

void render_hat(std::span<float> out, float velocity, bool open, const HatParams& p,
                int sample_rate, Rng& noise) {
    static constexpr std::array<double, 6> kRatios = {205.3, 304.4, 369.6, 522.7, 540.0, 800.0};
    const double decay = open ? p.open_decay : p.closed_decay;
    const std::size_t length = voice_length(out, decay * kRingOut, sample_rate);
    const Declick declick(length, sample_rate);
    const std::size_t attack = samples(0.0004, sample_rate);
    auto band = dsp::Biquad::bandpass(p.cutoff_hz, 0.9, sample_rate);
    auto air = dsp::Biquad::highpass(p.cutoff_hz * 0.7, 0.7, sample_rate);
    const double env_k = decay_coefficient(decay, sample_rate);

    // The 808's oscillators free-run, so every hit catches them at a different phase.
    std::array<double, 6> phase{};
    std::array<double, 6> step{};
    for (std::size_t k = 0; k < phase.size(); ++k) {
        phase[k] = noise.uniform();
        step[k] = kRatios[k] * p.tone / sample_rate;
    }

    double env = 1.0;
    for (std::size_t i = 0; i < length; ++i) {
        double metal = 0.0;
        for (std::size_t k = 0; k < phase.size(); ++k) {
            phase[k] += step[k];
            if (phase[k] >= 1.0) phase[k] -= 1.0;
            metal += phase[k] < 0.5 ? 1.0 : -1.0; // naive square: its aliasing only adds sizzle
        }
        const double source = metal / 6.0 + p.noise_mix * white(noise);
        const float shimmer = air.process(band.process(static_cast<float>(source)));
        out[i] += shimmer * static_cast<float>(env * 2.6) * velocity * ramp(i, attack) * declick(i);
        env *= env_k;
    }
}

void render_rim(std::span<float> out, float velocity, const RimParams& p, int sample_rate) {
    const std::size_t length = voice_length(out, p.decay * kRingOut, sample_rate);
    const Declick declick(length, sample_rate);
    auto thin = dsp::Biquad::highpass(250.0, 0.7, sample_rate);
    const dsp::Saturator knock(2.0f);
    const double env_k = decay_coefficient(p.decay, sample_rate);
    double low = 0.0, high = 0.0, env = 1.0;
    for (std::size_t i = 0; i < length; ++i) {
        advance(low, 480.0, sample_rate);
        advance(high, 1700.0, sample_rate);
        const auto tone = static_cast<float>((0.4 * dsp::sine(low) + 0.6 * dsp::sine(high)) * env);
        out[i] += knock(thin.process(tone)) * velocity * declick(i);
        env *= env_k;
    }
}

void render_shaker(std::span<float> out, float velocity, const ShakerParams& p, int sample_rate,
                   Rng& noise) {
    const std::size_t length = voice_length(out, p.attack + p.decay * kRingOut, sample_rate);
    const Declick declick(length, sample_rate);
    const std::size_t attack = std::max<std::size_t>(1, samples(p.attack, sample_rate));
    auto beads = dsp::Biquad::highpass(p.highpass_hz, 0.9, sample_rate);
    const double env_k = decay_coefficient(p.decay, sample_rate);
    double env = 1.0;
    for (std::size_t i = 0; i < length; ++i) {
        const float value = beads.process(white(noise)) * ramp(i, attack) * static_cast<float>(env);
        out[i] += value * 0.9f * velocity * declick(i);
        if (i >= attack) env *= env_k;
    }
}

void render_tom(std::span<float> out, float velocity, double hz, const TomParams& p,
                int sample_rate) {
    const std::size_t length = voice_length(out, p.decay * kRingOut, sample_rate);
    const Declick declick(length, sample_rate);
    const std::size_t attack = samples(0.001, sample_rate);
    const double amp_k = decay_coefficient(p.decay, sample_rate);
    const double bend_k = decay_coefficient(0.035, sample_rate);
    const dsp::Saturator skin(1.4f);
    double phase = 0.0, amp = 1.0, bend = p.bend;
    for (std::size_t i = 0; i < length; ++i) {
        advance(phase, hz * (1.0 + bend), sample_rate);
        const auto value = static_cast<float>(dsp::sine(phase) * amp);
        out[i] += skin(value) * velocity * ramp(i, attack) * declick(i);
        amp *= amp_k;
        bend *= bend_k;
    }
}

void render_cowbell(std::span<float> out, float velocity, const CowbellParams& p, int sample_rate) {
    const std::size_t length = voice_length(out, p.tail * kRingOut, sample_rate);
    const Declick declick(length, sample_rate);
    const std::size_t attack = samples(0.0005, sample_rate);
    auto body = dsp::Biquad::bandpass(1700.0 * p.tone, 0.9, sample_rate);
    const double fast_k = decay_coefficient(p.decay, sample_rate);
    const double tail_k = decay_coefficient(p.tail, sample_rate);
    const double step_low = 540.0 * p.tone / sample_rate, step_high = 800.0 * p.tone / sample_rate;
    double low = 0.0, high = 0.0, fast = 1.0, tail = 1.0;
    for (std::size_t i = 0; i < length; ++i) {
        low += step_low;
        high += step_high;
        if (low >= 1.0) low -= 1.0;
        if (high >= 1.0) high -= 1.0;
        const double squares = (low < 0.5 ? 0.5 : -0.5) + (high < 0.5 ? 0.5 : -0.5);
        // Two-stage envelope: the "clonk" dies fast, a quieter ring hangs on.
        const double env = 0.75 * fast + 0.25 * tail;
        const float value = body.process(static_cast<float>(squares * env)) * 2.2f;
        out[i] += value * velocity * ramp(i, attack) * declick(i);
        fast *= fast_k;
        tail *= tail_k;
    }
}

void render_cymbal(std::span<float> out, float velocity, const CymbalParams& p, int sample_rate,
                   Rng& noise) {
    static constexpr std::array<double, 6> kMetal = {205.3, 304.4, 369.6, 522.7, 540.0, 800.0};
    const std::size_t length =
        voice_length(out, p.decay * 4.6, sample_rate); // -40 dB: under a mix, enough
    const Declick declick(length, sample_rate);
    const std::size_t attack = samples(0.001, sample_rate);
    auto band = dsp::Biquad::bandpass(p.cutoff_hz, 0.6, sample_rate);
    auto air = dsp::Biquad::highpass(p.cutoff_hz * 0.5, 0.7, sample_rate);
    const double env_k = decay_coefficient(p.decay, sample_rate);
    const double bell_k = decay_coefficient(0.25, sample_rate);
    std::array<double, 6> phase{};
    for (double& ph : phase)
        ph = noise.uniform();
    double env = 1.0, bell = p.bell, ping_low = 0.0, ping_high = 0.0;
    for (std::size_t i = 0; i < length; ++i) {
        double metal = 0.0;
        for (std::size_t k = 0; k < phase.size(); ++k) {
            phase[k] += kMetal[k] * p.tone / sample_rate;
            if (phase[k] >= 1.0) phase[k] -= 1.0;
            metal += phase[k] < 0.5 ? 1.0 : -1.0;
        }
        const double source = (1.0 - p.noise_mix) * metal / 6.0 + p.noise_mix * white(noise);
        const float wash = air.process(band.process(static_cast<float>(source)));
        advance(ping_low, 1720.0, sample_rate);
        advance(ping_high, 2610.0, sample_rate);
        const double ping =
            bell > 1e-4 ? bell * (dsp::sine(ping_low) + 0.6 * dsp::sine(ping_high)) * 0.35 : 0.0;
        const auto value = static_cast<float>(wash * 2.4 * env + ping);
        out[i] += value * velocity * ramp(i, attack) * declick(i);
        env *= env_k;
        bell *= bell_k;
    }
}

void render_bass_line(std::span<float> out, std::span<const BassNote> notes, const BassParams& p,
                      int sample_rate) {
    if (notes.empty()) return;
    const double decay_k = decay_coefficient(p.decay, sample_rate);
    const double release_k = decay_coefficient(p.release, sample_rate);
    const double knock_k = decay_coefficient(0.008, sample_rate);
    // Portamento in the log-frequency domain: equal time per octave, like a real glide circuit.
    const double glide_step = 1.0 - decay_coefficient(p.glide, sample_rate);
    const std::size_t attack = std::max<std::size_t>(1, samples(0.004, sample_rate));
    const double drive_norm = 1.0 / std::tanh(std::max(0.01, double{p.drive}));

    std::size_t end = 0;
    for (const auto& n : notes)
        end = std::max(end, n.start + n.length);
    end = std::min(out.size(), end + samples(p.release * kRingOut, sample_rate));

    const bool saw = p.shape == BassShape::Saw;
    const double filter_k = decay_coefficient(p.filter_decay, sample_rate);
    dsp::StateVariable filter(sample_rate);

    double phase = 0.0, log_hz = std::log(notes.front().hz), log_target = log_hz;
    double level = 0.0, attack_from = 0.0, knock = 0.0, filter_env = 0.0;
    float velocity = 0.0f;
    std::size_t next = 0, gate_end = 0, since_attack = attack;
    for (std::size_t i = notes.front().start; i < end; ++i) {
        while (next < notes.size() && notes[next].start <= i) {
            const BassNote& note = notes[next++];
            const bool legato =
                note.glide && i < gate_end + samples(0.01, sample_rate) && level > 0.02;
            log_target = std::log(std::max(note.hz, 1.0));
            gate_end = note.start + note.length;
            if (!legato) {
                // Re-attack from the current level (never from zero) so retriggers don't click.
                log_hz = log_target;
                attack_from = level;
                since_attack = 0;
                knock = p.knock;
                filter_env = 1.0;
                velocity = note.velocity;
            }
        }
        log_hz += (log_target - log_hz) * glide_step;

        if (since_attack < attack) {
            level = attack_from + (1.0 - attack_from) * (static_cast<double>(since_attack) /
                                                         static_cast<double>(attack));
            ++since_attack;
        } else {
            level *= i < gate_end ? decay_k : release_k;
        }
        const double hz = std::exp(log_hz) * (1.0 + knock);
        knock *= knock_k;
        double source = 0.0;
        if (saw) {
            phase += hz / sample_rate; // saw phase runs in cycles, not radians
            if (phase >= 1.0) phase -= 1.0;
            const double cutoff = p.cutoff_hz + p.env_hz * filter_env * (0.5 + 0.5 * velocity);
            source = filter.lowpass(dsp::blep_saw(phase, hz / sample_rate), cutoff, p.damping);
            filter_env *= filter_k;
        } else {
            advance(phase, hz, sample_rate);
            source = dsp::sine(phase);
        }
        const double value = std::tanh(p.drive * level * source) * drive_norm;
        out[i] += static_cast<float>(value) * velocity;
    }
}

void render_keys_note(std::span<float> out, double hz, float velocity, std::size_t gate,
                      const KeysParams& p, int sample_rate, const Wow& wow) {
    const std::size_t length =
        std::min(out.size(), gate + samples(p.release * kRingOut, sample_rate));
    const Declick declick(length, sample_rate);
    const std::size_t attack = std::max<std::size_t>(1, samples(p.attack, sample_rate));
    const double amp_k = decay_coefficient(p.amp_decay, sample_rate);
    const double release_k = decay_coefficient(p.release, sample_rate);
    const double index_k = decay_coefficient(p.index_decay, sample_rate);
    const double wow_step = kTwoPi * wow.hz / sample_rate;

    double carrier = 0.0, modulator = 0.0, amp = 1.0;
    double index = p.index * (0.45 + 0.55 * velocity); // velocity → brightness
    for (std::size_t i = 0; i < length; ++i) {
        const double pitch =
            wow.depth > 0.0
                ? hz * (1.0 + wow.depth * dsp::sine(wow_step * static_cast<double>(wow.origin + i)))
                : hz;
        advance(carrier, pitch, sample_rate);
        advance(modulator, pitch * p.ratio, sample_rate);
        // Phase modulation: the modulator bends the carrier's phase; index sets the sideband
        // spread.
        const double value = dsp::sine(carrier + (index + p.index_floor) * dsp::sine(modulator));
        out[i] += static_cast<float>(value * amp) * velocity * ramp(i, attack) * declick(i);
        amp *= i < gate ? amp_k : release_k;
        index *= index_k;
    }
}

void render_vinyl(std::span<float> left, std::span<float> right, float amount, int sample_rate,
                  Rng& rng) {
    if (amount <= 0.0f) return;
    const std::size_t n = std::min(left.size(), right.size());
    const double pops_per_second = 4.0 + 22.0 * amount;
    const double pop_chance = pops_per_second / sample_rate;
    const double pop_k = decay_coefficient(0.0004, sample_rate);
    auto tick_l = dsp::Biquad::highpass(900.0, 0.7, sample_rate);
    auto tick_r = dsp::Biquad::highpass(900.0, 0.7, sample_rate);
    dsp::OnePole hiss_l(3500.0, sample_rate), hiss_r(3500.0, sample_rate);

    double pop_l = 0.0, pop_r = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        if (rng.chance(pop_chance)) {
            // Cubing a uniform value skews toward tiny ticks with the odd loud pop.
            const double u = rng.uniform();
            const double size = 0.06 * amount * (0.08 + 0.92 * u * u * u);
            const double pan = rng.uniform();
            pop_l += size * (1.0 - pan);
            pop_r += size * pan;
        }
        const float crack_l = tick_l.process(static_cast<float>(pop_l) * white(rng));
        const float crack_r = tick_r.process(static_cast<float>(pop_r) * white(rng));
        const float hiss = 0.004f * amount;
        left[i] += 0.5f * crack_l + hiss * hiss_l.lowpass(white(rng));
        right[i] += 0.5f * crack_r + hiss * hiss_r.lowpass(white(rng));
        pop_l *= pop_k;
        pop_r *= pop_k;
    }
}

} // namespace street::beats
