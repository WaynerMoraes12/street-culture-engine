// Subtractive synth voice: electro stabs and palm-muted / ringing power chords.

#include "street/beats/dsp.hpp"
#include "street/beats/voices.hpp"

#include <algorithm>
#include <cmath>

namespace street::beats {

void render_synth_note(std::span<float> out, double hz, float velocity, std::size_t gate,
                       const SynthParams& p, int sample_rate) {
    const std::size_t length =
        std::min(out.size(), gate + dsp::samples(p.release * 6.9, sample_rate));
    if (length == 0) return;
    const std::size_t attack = std::max<std::size_t>(1, dsp::samples(p.attack, sample_rate));
    const std::size_t fade = std::min(length, dsp::samples(0.004, sample_rate));
    const double decay_k = dsp::decay_coefficient(p.decay, sample_rate);
    const double release_k = dsp::decay_coefficient(p.release, sample_rate);
    const double filter_k = dsp::decay_coefficient(p.filter_decay, sample_rate);

    // Two oscillators detuned in opposite directions beat against each other: the classic
    // thick analog-synth sound. Different start phases avoid a hard, identical first cycle.
    const double spread = std::exp2(p.detune_cents / 1200.0);
    const double inc_a = hz * spread / sample_rate;
    const double inc_b = hz / spread / sample_rate;
    double phase_a = 0.0, phase_b = 0.37;
    dsp::StateVariable filter(sample_rate);

    double decay_env = 1.0, filter_env = 1.0, level = 0.0;
    const double brightness = 0.5 + 0.5 * velocity;
    const auto oscillator = [&](double phase, double inc) {
        const double saw = dsp::blep_saw(phase, inc);
        if (p.square_mix <= 0.0f) return saw;
        // A square is the difference of two saws half a cycle apart.
        double shifted = phase + 0.5;
        if (shifted >= 1.0) shifted -= 1.0;
        const double square = saw - dsp::blep_saw(shifted, inc);
        return (1.0 - p.square_mix) * saw + p.square_mix * 0.5 * square;
    };

    for (std::size_t i = 0; i < length; ++i) {
        if (i < gate) {
            const double ramp =
                i < attack ? static_cast<double>(i) / static_cast<double>(attack) : 1.0;
            level = ramp * (p.sustain + (1.0 - p.sustain) * decay_env);
            decay_env *= decay_k;
        } else {
            level *= release_k;
        }
        const double raw = 0.5 * (oscillator(phase_a, inc_a) + oscillator(phase_b, inc_b));
        phase_a += inc_a;
        phase_b += inc_b;
        if (phase_a >= 1.0) phase_a -= 1.0;
        if (phase_b >= 1.0) phase_b -= 1.0;

        const double cutoff = p.cutoff_hz + p.env_hz * filter_env * brightness;
        filter_env *= filter_k;
        const float filtered = filter.lowpass(static_cast<float>(raw), cutoff, p.damping);
        const std::size_t left = length - i;
        const float declick =
            left >= fade ? 1.0f : static_cast<float>(left) / static_cast<float>(fade);
        out[i] += filtered * static_cast<float>(level) * velocity * declick;
    }
}

} // namespace street::beats
