#include "street/beats/dsp.hpp"

#include <algorithm>
#include <cmath>
#include <doctest/doctest.h>
#include <vector>

using namespace street::beats;

namespace {

constexpr int kRate = 44100;

/// Steady-state RMS gain of a filter for a sine at `hz` (the first 0.1 s is discarded).
template <class Process> double sine_gain(double hz, Process&& process) {
    double in = 0.0, out = 0.0;
    for (int i = 0; i < kRate / 2; ++i) {
        const auto x = static_cast<float>(std::sin(dsp::kTwoPi * hz * i / kRate));
        const float y = process(x);
        if (i > kRate / 10) {
            in += double{x} * x;
            out += double{y} * y;
        }
    }
    return std::sqrt(out / in);
}

} // namespace

TEST_CASE("wavetable sine matches std::sin to better than 16-bit precision") {
    double worst = 0.0;
    for (int i = -5000; i < 5000; ++i) {
        const double x = i * 0.01237; // includes negative phases, as FM produces
        worst = std::max(worst, std::abs(dsp::sine(x) - std::sin(x)));
    }
    CHECK(worst < 2e-6);
}

TEST_CASE("time helpers") {
    CHECK(dsp::samples(1.0, kRate) == 44100);
    CHECK(dsp::samples(-1.0, kRate) == 0);
    // After `seconds` worth of samples the envelope has decayed by exactly 1/e.
    const double k = dsp::decay_coefficient(0.1, kRate);
    CHECK(std::pow(k, 4410) == doctest::Approx(std::exp(-1.0)).epsilon(1e-6));
}

TEST_CASE("RBJ low-pass passes lows and attenuates highs") {
    auto lp = [] { return dsp::Biquad::lowpass(1000.0, 0.707, kRate); };
    auto a = lp(), b = lp(), c = lp();
    CHECK(sine_gain(100.0, [&](float x) { return a.process(x); }) ==
          doctest::Approx(1.0).epsilon(0.02));
    CHECK(sine_gain(1000.0, [&](float x) { return b.process(x); }) ==
          doctest::Approx(0.707).epsilon(0.03));
    CHECK(sine_gain(10000.0, [&](float x) { return c.process(x); }) < 0.02); // ~-40 dB a decade up
}

TEST_CASE("RBJ high-pass and band-pass") {
    auto hp1 = dsp::Biquad::highpass(2000.0, 0.707, kRate);
    auto hp2 = dsp::Biquad::highpass(2000.0, 0.707, kRate);
    CHECK(sine_gain(100.0, [&](float x) { return hp1.process(x); }) < 0.01);
    CHECK(sine_gain(12000.0, [&](float x) { return hp2.process(x); }) ==
          doctest::Approx(1.0).epsilon(0.03));

    auto bp1 = dsp::Biquad::bandpass(1000.0, 2.0, kRate);
    auto bp2 = dsp::Biquad::bandpass(1000.0, 2.0, kRate);
    CHECK(sine_gain(1000.0, [&](float x) { return bp1.process(x); }) ==
          doctest::Approx(1.0).epsilon(0.02));
    CHECK(sine_gain(8000.0, [&](float x) { return bp2.process(x); }) < 0.1);
}

TEST_CASE("one-pole smoother and DC blocker") {
    dsp::OnePole smooth(10.0, kRate);
    float y = 0.0f;
    for (int i = 0; i < kRate; ++i)
        y = smooth.lowpass(1.0f);
    CHECK(y == doctest::Approx(1.0).epsilon(1e-3));

    dsp::DcBlocker dc;
    float last = 1.0f;
    for (int i = 0; i < kRate; ++i)
        last = dc.process(0.5f); // pure offset decays away
    CHECK(std::abs(last) < 1e-3f);
}

TEST_CASE("saturation and soft clipping are bounded, odd and transparent where promised") {
    const dsp::Saturator drive(3.0f);
    CHECK(drive(1.0f) == doctest::Approx(1.0));
    CHECK(drive(-0.4f) == doctest::Approx(-drive(0.4f)));
    CHECK(std::abs(drive(50.0f)) <= 1.0f / std::tanh(3.0f) + 1e-6f);
    CHECK(dsp::saturate(0.3f, 0.0f) == 0.3f); // zero drive is clean

    CHECK(dsp::soft_clip(0.5f, 0.8f) == 0.5f);
    CHECK(dsp::soft_clip(0.9f, 0.8f) < 0.9f);
    CHECK(dsp::soft_clip(1.2f, 0.8f) < 1.0f);
    CHECK(dsp::soft_clip(10.0f, 0.8f) <= 1.0f); // float tanh saturates to exactly 1 far out
    CHECK(dsp::soft_clip(-10.0f, 0.8f) >= -1.0f);
    // Continuous at the knee.
    CHECK(dsp::soft_clip(0.8001f, 0.8f) == doctest::Approx(0.8001).epsilon(1e-4));
}

TEST_CASE("bit crusher quantises and holds samples") {
    dsp::BitCrusher crush(4, kRate / 4.0, kRate); // 8 levels per polarity, hold 4 samples
    std::vector<float> out;
    for (int i = 0; i < 16; ++i)
        out.push_back(crush.process(static_cast<float>(i) / 16.0f));
    for (float v : out)
        CHECK(v * 8.0f == doctest::Approx(std::round(v * 8.0f)));
    CHECK(out[0] == out[1]);
    CHECK(out[1] == out[3]);
    CHECK(out[4] != out[3]);
}

TEST_CASE("constant-power pan law") {
    const auto centre = dsp::pan_gains(0.0f);
    CHECK(centre.left == doctest::Approx(std::sqrt(0.5)));
    CHECK(centre.right == doctest::Approx(std::sqrt(0.5)));
    const auto left = dsp::pan_gains(-1.0f);
    CHECK(left.left == doctest::Approx(1.0));
    CHECK(left.right == doctest::Approx(0.0).epsilon(1e-6));
    for (float p = -1.0f; p <= 1.0f; p += 0.125f) {
        const auto g = dsp::pan_gains(p);
        CHECK(g.left * g.left + g.right * g.right == doctest::Approx(1.0));
    }
}

TEST_CASE("look-ahead limiter catches every peak and leaves quiet material alone") {
    AudioBuffer audio(kRate / 2);
    for (std::size_t i = 0; i < audio.frames(); ++i) {
        const double quiet = 0.2 * std::sin(0.01 * static_cast<double>(i));
        audio.left[i] = audio.right[i] = static_cast<float>(quiet);
    }
    audio.left[15000] = 1.8f; // a transient spike
    audio.right[15010] = -1.5f;
    dsp::limit(audio, 0.9f);
    CHECK(peak(audio) <= 0.9f + 1e-6f);
    CHECK(audio.left[100] ==
          doctest::Approx(0.2 * std::sin(1.0)).epsilon(1e-6)); // untouched far away
}

TEST_CASE("master stage normalises to the ceiling with clean edges") {
    AudioBuffer audio(kRate);
    for (std::size_t i = 0; i < audio.frames(); ++i) {
        const auto s = static_cast<float>(0.3 * std::sin(0.05 * static_cast<double>(i)) +
                                          (i % 5000 == 0 ? 0.9 : 0.0));
        audio.left[i] = s;
        audio.right[i] = -s;
    }
    dsp::master(audio, -1.0, 0.1);
    CHECK(peak_dbfs(audio) == doctest::Approx(-1.0).epsilon(1e-3));
    CHECK(audio.left.front() == 0.0f);
    CHECK(audio.left.back() == doctest::Approx(0.0).epsilon(1e-4));

    AudioBuffer silence(1000);
    dsp::master(silence);
    CHECK(peak(silence) == 0.0f);
}
