#pragma once
// Signal-processing building blocks, written from first principles.
//
// All processors are tiny value types with their state inline, so a voice can
// create them on the stack per hit. Coefficients and state are double precision
// (recursive filters are sensitive to rounding); audio flows through as float.

#include "street/beats/audio.hpp"

#include <cstddef>
#include <numbers>

namespace street::beats::dsp {

inline constexpr double kTwoPi = 2.0 * std::numbers::pi;

/// Converts a duration to a whole number of samples (never negative).
std::size_t samples(double seconds, int sample_rate) noexcept;

/// Per-sample multiplier that makes a value decay by 1/e every `seconds`.
/// Multiplying an envelope by it each sample is an exact exponential decay,
/// far cheaper than calling std::exp per sample.
double decay_coefficient(double seconds, int sample_rate) noexcept;

/// Sine via a 2048-point wavetable with linear interpolation. Any phase (radians, even
/// negative, as FM produces) is accepted. The worst-case error, (2π/2048)² / 8 ≈ 1.2e-6
/// (-118 dB), is far below 16-bit resolution, and it is several times faster than
/// std::sin in the inner loops of the oscillators.
float sine(double radians) noexcept;

/// RBJ "Audio EQ Cookbook" biquad (Robert Bristow-Johnson), transposed direct form II.
/// Second-order sections give 12 dB/octave slopes with a resonance (Q) control, which
/// is all a drum machine needs: low-pass for warmth, high-pass to thin noise into hats,
/// band-pass to focus a clap.
class Biquad {
public:
    static Biquad lowpass(double cutoff_hz, double q, int sample_rate) noexcept;
    static Biquad highpass(double cutoff_hz, double q, int sample_rate) noexcept;
    /// Constant 0 dB peak gain band-pass centred on `center_hz`.
    static Biquad bandpass(double center_hz, double q, int sample_rate) noexcept;

    float process(float x) noexcept {
        const double y = b0_ * x + z1_;
        z1_ = b1_ * x - a1_ * y + z2_;
        z2_ = b2_ * x - a2_ * y;
        return static_cast<float>(y);
    }
    void reset() noexcept { z1_ = z2_ = 0.0; }

private:
    Biquad(double b0, double b1, double b2, double a0, double a1, double a2) noexcept;
    double b0_, b1_, b2_, a1_, a2_;
    double z1_ = 0.0, z2_ = 0.0;
};

/// Chamberlin state-variable filter whose cutoff may change every sample, which is what a
/// synth filter envelope needs (a biquad would have to recompute its coefficients).
/// f = 2·sin(π·fc/fs); cutoffs are clamped to fs/8, where the structure stays stable.
/// Lower damping means more resonance (2 = none, ~0.5 = a squelchy peak).
class StateVariable {
public:
    explicit StateVariable(int sample_rate) noexcept : sample_rate_(sample_rate) {}
    float lowpass(float x, double cutoff_hz, double damping) noexcept;

private:
    int sample_rate_;
    double low_ = 0.0, band_ = 0.0;
};

/// Band-limited sawtooth (PolyBLEP): a naive ramp with its discontinuity smoothed by a
/// two-sample polynomial correction, which removes most of the aliasing a raw saw would
/// fold back into the audible range. `phase` in [0, 1), `increment` = hz / sample rate.
float blep_saw(double phase, double increment) noexcept;

/// One-pole low-pass (exponential smoother): y += a·(x − y), 6 dB/octave.
class OnePole {
public:
    OnePole(double cutoff_hz, int sample_rate) noexcept;
    float lowpass(float x) noexcept {
        y_ += a_ * (x - y_);
        return static_cast<float>(y_);
    }
    /// Complementary high-pass: whatever the low-pass removes.
    float highpass(float x) noexcept { return x - lowpass(x); }

private:
    double a_;
    double y_ = 0.0;
};

/// Removes DC offset: y[n] = x[n] − x[n−1] + R·y[n−1], corner ≈ 10 Hz at 44.1 kHz.
class DcBlocker {
public:
    float process(float x) noexcept {
        const double y = x - x1_ + 0.9985 * y1_;
        x1_ = x;
        y1_ = y;
        return static_cast<float>(y);
    }

private:
    double x1_ = 0.0, y1_ = 0.0;
};

/// tanh waveshaper normalised so ±1 still maps to ±1: adds odd harmonics (warmth,
/// loudness) the harder it is driven. drive <= 0 is treated as clean.
class Saturator {
public:
    explicit Saturator(float drive) noexcept;
    float operator()(float x) const noexcept;

private:
    float drive_, norm_;
};

/// One-off form of Saturator (recomputes the normalisation on every call).
float saturate(float x, float drive) noexcept;

/// Soft-knee clipper: transparent below `threshold`, then a tanh knee that approaches
/// but never reaches ±1. Continuous in value and slope at the threshold.
float soft_clip(float x, float threshold) noexcept;

/// Sample-rate and bit-depth reduction. The E-mu SP-1200 sampled at 26.04 kHz with
/// 12 bits and no steep anti-aliasing filter; holding each sample and quantising it
/// reproduces the aliasing "grit" that defined boom-bap drums.
class BitCrusher {
public:
    BitCrusher(int bits, double rate_hz, int sample_rate) noexcept;
    float process(float x) noexcept;

private:
    double levels_;
    double step_;
    double phase_; // starts one step short of 1 so the very first sample is latched
    float held_ = 0.0f;
};

/// Constant-power pan law: pan in [-1, 1] maps to an angle θ in [0, π/2] with
/// L = cos θ, R = sin θ, so L² + R² = 1 and a sound keeps its loudness as it moves.
struct PanGains {
    float left, right;
};
PanGains pan_gains(float pan) noexcept;

/// Look-ahead brickwall limiter. Offline, so it can see the future: the gain needed
/// at each sample is min-filtered over the look-ahead window (sliding-window minimum
/// with a monotonic deque, O(n)) and then averaged over the same window, which ramps
/// the gain down smoothly *before* every peak instead of clipping it. Releases
/// exponentially. After processing no sample exceeds `threshold`.
void limit(AudioBuffer& audio, float threshold, double lookahead_s = 0.003,
           double release_s = 0.08);

/// Master stage: removes DC, brings the mix to a consistent loudness (bounded boost
/// into the limiter), limits, then normalises the peak to exactly `ceiling_db` dBFS
/// with short fades at both ends so the file starts and stops without clicks.
void master(AudioBuffer& audio, double ceiling_db = -1.0, double fade_out_s = 0.25);

} // namespace street::beats::dsp
