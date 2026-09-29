#include "street/beats/dsp.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <vector>

namespace street::beats::dsp {
namespace {

struct Rbj {
    double cos_w, alpha;
};

// Shared RBJ intermediate terms. The cutoff is kept below Nyquist so the filter stays stable
// even if a caller asks for something silly like 30 kHz at 44.1 kHz.
Rbj rbj(double hz, double q, int sample_rate) noexcept {
    const double nyquist_safe = 0.45 * sample_rate;
    const double w0 = kTwoPi * std::clamp(hz, 10.0, nyquist_safe) / sample_rate;
    return {std::cos(w0), std::sin(w0) / (2.0 * std::max(q, 0.05))};
}

} // namespace

std::size_t samples(double seconds, int sample_rate) noexcept {
    return seconds > 0.0 ? static_cast<std::size_t>(seconds * sample_rate + 0.5) : 0;
}

float sine(double radians) noexcept {
    constexpr int kSize = 2048;
    static const std::array<float, kSize + 1> table = [] {
        std::array<float, kSize + 1> t{};
        for (int i = 0; i <= kSize; ++i)
            t[static_cast<std::size_t>(i)] = static_cast<float>(std::sin(kTwoPi * i / kSize));
        return t;
    }();
    const double position = radians * (kSize / kTwoPi);
    const double floor = std::floor(position);
    const auto index = static_cast<std::size_t>(static_cast<std::int64_t>(floor) & (kSize - 1));
    const auto frac = static_cast<float>(position - floor);
    return table[index] + (table[index + 1] - table[index]) * frac;
}

double decay_coefficient(double seconds, int sample_rate) noexcept {
    if (seconds <= 0.0) return 0.0;
    return std::exp(-1.0 / (seconds * sample_rate));
}

Biquad::Biquad(double b0, double b1, double b2, double a0, double a1, double a2) noexcept
    : b0_(b0 / a0), b1_(b1 / a0), b2_(b2 / a0), a1_(a1 / a0), a2_(a2 / a0) {}

Biquad Biquad::lowpass(double cutoff_hz, double q, int sample_rate) noexcept {
    const auto [c, alpha] = rbj(cutoff_hz, q, sample_rate);
    return {(1.0 - c) / 2.0, 1.0 - c, (1.0 - c) / 2.0, 1.0 + alpha, -2.0 * c, 1.0 - alpha};
}

Biquad Biquad::highpass(double cutoff_hz, double q, int sample_rate) noexcept {
    const auto [c, alpha] = rbj(cutoff_hz, q, sample_rate);
    return {(1.0 + c) / 2.0, -(1.0 + c), (1.0 + c) / 2.0, 1.0 + alpha, -2.0 * c, 1.0 - alpha};
}

Biquad Biquad::bandpass(double center_hz, double q, int sample_rate) noexcept {
    const auto [c, alpha] = rbj(center_hz, q, sample_rate);
    return {alpha, 0.0, -alpha, 1.0 + alpha, -2.0 * c, 1.0 - alpha};
}

float StateVariable::lowpass(float x, double cutoff_hz, double damping) noexcept {
    const double hz = std::clamp(cutoff_hz, 20.0, sample_rate_ / 8.0);
    const double f = 2.0 * sine(std::numbers::pi * hz / sample_rate_);
    const double q = std::clamp(damping, 0.2, 2.0);
    low_ += f * band_;
    const double high = x - low_ - q * band_;
    band_ += f * high;
    return static_cast<float>(low_);
}

float blep_saw(double phase, double increment) noexcept {
    double value = 2.0 * phase - 1.0;
    const double dt = std::clamp(increment, 1e-9, 0.5);
    if (phase < dt) { // just after the wrap
        const double t = phase / dt;
        value -= t + t - t * t - 1.0;
    } else if (phase > 1.0 - dt) { // just before it
        const double t = (phase - 1.0) / dt;
        value -= t * t + t + t + 1.0;
    }
    return static_cast<float>(value);
}

OnePole::OnePole(double cutoff_hz, int sample_rate) noexcept
    : a_(1.0 - std::exp(-kTwoPi * std::max(cutoff_hz, 1.0) / sample_rate)) {}

Saturator::Saturator(float drive) noexcept
    : drive_(drive), norm_(drive > 1e-3f ? 1.0f / std::tanh(drive) : 1.0f) {}

float Saturator::operator()(float x) const noexcept {
    return drive_ > 1e-3f ? std::tanh(drive_ * x) * norm_ : x;
}

float saturate(float x, float drive) noexcept {
    return Saturator(drive)(x);
}

float soft_clip(float x, float threshold) noexcept {
    const float t = std::clamp(threshold, 0.05f, 0.999f);
    const float mag = std::abs(x);
    if (mag <= t) return x;
    const float room = 1.0f - t;
    const float shaped = t + room * std::tanh((mag - t) / room); // slope 1 at the knee
    return x < 0.0f ? -shaped : shaped;
}

BitCrusher::BitCrusher(int bits, double rate_hz, int sample_rate) noexcept
    : levels_(std::ldexp(1.0, std::clamp(bits, 2, 24) - 1)),
      step_(std::clamp(rate_hz / sample_rate, 1e-4, 1.0)), phase_(1.0 - step_) {}

float BitCrusher::process(float x) noexcept {
    phase_ += step_;
    if (phase_ >= 1.0) {
        phase_ -= std::floor(phase_);
        held_ = static_cast<float>(std::round(x * levels_) / levels_);
    }
    return held_;
}

PanGains pan_gains(float pan) noexcept {
    const double theta =
        (std::clamp(static_cast<double>(pan), -1.0, 1.0) + 1.0) * std::numbers::pi / 4.0;
    return {static_cast<float>(std::cos(theta)), static_cast<float>(std::sin(theta))};
}

void limit(AudioBuffer& audio, float threshold, double lookahead_s, double release_s) {
    const std::size_t n = audio.frames();
    if (n == 0) return;
    const std::size_t window = std::max<std::size_t>(1, samples(lookahead_s, audio.sample_rate));

    // Gain each sample needs on its own to stay under the threshold.
    std::vector<double> need(n);
    for (std::size_t i = 0; i < n; ++i) {
        const float level = std::max(std::abs(audio.left[i]), std::abs(audio.right[i]));
        need[i] = level > threshold ? threshold / level : 1.0;
    }

    // ahead[i] = min(need[i .. i+window-1]): sliding minimum, scanning backwards with a deque
    // of indices whose values increase from front to back.
    std::vector<double> ahead(n);
    std::deque<std::size_t> window_min;
    for (std::size_t k = n; k-- > 0;) {
        while (!window_min.empty() && need[window_min.back()] >= need[k])
            window_min.pop_back();
        window_min.push_back(k);
        while (window_min.front() >= k + window)
            window_min.pop_front();
        ahead[k] = need[window_min.front()];
    }

    // Averaging ahead[] over the trailing window makes the gain glide down across the
    // look-ahead span. Every value averaged at a peak already includes that peak in its
    // own minimum, so the average never exceeds the gain the peak needs.
    const double release = decay_coefficient(release_s, audio.sample_rate);
    double sum = 0.0;
    double gain = 1.0;
    for (std::size_t i = 0; i < n; ++i) {
        sum += ahead[i];
        if (i >= window) sum -= ahead[i - window];
        const double target = sum / static_cast<double>(std::min(i + 1, window));
        gain = target < gain ? target : target + (gain - target) * release;
        audio.left[i] = static_cast<float>(audio.left[i] * gain);
        audio.right[i] = static_cast<float>(audio.right[i] * gain);
    }
}

void master(AudioBuffer& audio, double ceiling_db, double fade_out_s) {
    const std::size_t n = audio.frames();
    if (n == 0) return;

    DcBlocker dc_left, dc_right;
    for (std::size_t i = 0; i < n; ++i) {
        audio.left[i] = dc_left.process(audio.left[i]);
        audio.right[i] = dc_right.process(audio.right[i]);
    }

    const float current_peak = peak(audio);
    if (current_peak < 1e-6f) return; // silence stays silence

    // Loudness: aim for about -13 dBFS RMS, but never push peaks more than 6 dB into the
    // limiter. Transients survive; quiet styles (lo-fi) get lifted to a usable level.
    constexpr double kTargetRmsDb = -13.0;
    constexpr double kMaxPeakDrive = 2.0;
    const double boost =
        std::min(db_to_gain(kTargetRmsDb - rms_dbfs(audio)), kMaxPeakDrive / current_peak);
    for (std::size_t i = 0; i < n; ++i) {
        audio.left[i] = static_cast<float>(audio.left[i] * boost);
        audio.right[i] = static_cast<float>(audio.right[i] * boost);
    }
    limit(audio, 0.95f);

    // Fades: 2 ms in (a hit on the very first sample must not click), a gentle tail out.
    const std::size_t fade_in = std::min(n, samples(0.002, audio.sample_rate));
    const std::size_t fade_out = std::min(n, samples(fade_out_s, audio.sample_rate));
    for (std::size_t i = 0; i < fade_in; ++i) {
        const auto g = static_cast<float>(i) / static_cast<float>(fade_in);
        audio.left[i] *= g;
        audio.right[i] *= g;
    }
    for (std::size_t i = 0; i < fade_out; ++i) {
        const double x = static_cast<double>(i) / static_cast<double>(fade_out);
        const auto g =
            static_cast<float>(0.5 + 0.5 * std::cos(std::numbers::pi * x)); // raised cosine
        audio.left[n - fade_out + i] *= g;
        audio.right[n - fade_out + i] *= g;
    }

    const double scale = db_to_gain(ceiling_db) / std::max(peak(audio), 1e-6f);
    for (std::size_t i = 0; i < n; ++i) {
        audio.left[i] = static_cast<float>(audio.left[i] * scale);
        audio.right[i] = static_cast<float>(audio.right[i] * scale);
    }
}

} // namespace street::beats::dsp
