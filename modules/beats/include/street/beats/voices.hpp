#pragma once
// Synthesised instruments: every sound in a beat is built here from oscillators,
// noise, envelopes and filters. No samples.
//
// Drum voices are one-shots that *add* into a span starting at the hit's onset.
// The span's length is the maximum the voice may ring (end of song, or a choke:
// a closed hat cutting an open one); when truncated, the voice fades out over a
// few milliseconds instead of stopping dead, which would click.
//
// Voices that need noise take an street::Rng, so the exact noise (and therefore the
// whole render) is reproducible from the beat's seed.

#include "street/core/rng.hpp"

#include <cstddef>
#include <span>

namespace street::beats {

/// TR-808 style kick: a sine whose pitch falls exponentially from `start_hz` to
/// `end_hz` (the "boom" is the sweep, not a static tone), an exponential amplitude
/// decay, tanh drive for harmonics that small speakers can reproduce, and a short
/// high tone for the beater click.
struct KickParams {
    double start_hz = 150.0, end_hz = 45.0;
    double pitch_decay = 0.04; ///< seconds for the pitch sweep to fall by 1/e
    double amp_decay = 0.32;
    float drive = 2.0f;
    float click = 0.25f;
};

/// Snare: two sine "drum head" modes (fundamental + 1.6×) plus high-passed white noise
/// for the snare wires.
struct SnareParams {
    double tone_hz = 190.0, tone_decay = 0.07;
    double noise_decay = 0.13, noise_highpass_hz = 1800.0;
    float tone_mix = 0.45f;
};

/// Clap: several hands that never hit at exactly the same time, modelled as 3–4 noise
/// bursts ~10 ms apart followed by a longer tail, all band-passed around 1 kHz.
struct ClapParams {
    int bursts = 4;
    double spacing = 0.010, burst_decay = 0.004, tail_decay = 0.11;
    double center_hz = 1200.0, q = 1.3;
};

/// Hi-hats: six square-wave oscillators at the TR-808's inharmonic frequencies
/// (205.3, 304.4, 369.6, 522.7, 540 and 800 Hz). Their sum is a dense metallic cluster
/// with no clear pitch; band- and high-passing it leaves the shimmer of a cymbal.
struct HatParams {
    double closed_decay = 0.018, open_decay = 0.20;
    double cutoff_hz = 7500.0;
    double tone = 1.0;       ///< scales every oscillator frequency (darker < 1 < brighter)
    float noise_mix = 0.15f; ///< a little white noise for air
};

/// Rimshot / side-stick: two short resonant tones (~480 Hz and 1.7 kHz), high-passed.
struct RimParams {
    double decay = 0.012;
};

/// Shaker: high-passed noise with a slow-ish attack (the beads swing before they hit).
struct ShakerParams {
    double attack = 0.012, decay = 0.045, highpass_hz = 5500.0;
};

/// Tom / atabaque-style low drum: sine with a quick downward pitch bend.
struct TomParams {
    double hz = 95.0, decay = 0.26;
    double bend = 0.6; ///< start pitch = hz · (1 + bend)
};

/// 808 bass: a monophonic sine line with tanh drive, a short pitch "knock" on each
/// attack and portamento (glide) into notes flagged as slides.
/// Oscillator of the bass line: a driven sine (808) or a filtered PolyBLEP sawtooth
/// (80s sequencer bass, or a gritty bass guitar under metal).
enum class BassShape { Sine808, Saw };

struct BassParams {
    BassShape shape = BassShape::Sine808;
    double decay = 1.2; ///< amplitude decay while the note is held
    double release = 0.03;
    double glide = 0.045; ///< portamento time constant
    double knock = 0.7;   ///< attack pitch overshoot (fraction of the note frequency)
    float drive = 2.2f;
    // Saw only: low-pass cutoff plus an envelope that opens it on every attack.
    double cutoff_hz = 350.0, env_hz = 1500.0, filter_decay = 0.12, damping = 0.9;
};

/// TR-808 cowbell: two square waves at 540 and 800 Hz through a band-pass, with a fast
/// initial decay into a longer, quieter tail.
struct CowbellParams {
    double decay = 0.06, tail = 0.3;
    double tone = 1.0; ///< scales both oscillators
};

/// Crash and ride cymbals: the 808 metal cluster pitched up, mixed with noise, band- and
/// high-passed, with a long decay. `bell` adds the ping of a ride's bell.
struct CymbalParams {
    double decay = 1.2;
    double cutoff_hz = 6500.0;
    double tone = 1.5;
    float noise_mix = 0.6f;
    float bell = 0.0f;
};

/// Subtractive synth voice for stabs and power chords: two detuned PolyBLEP oscillators
/// (saw, square, or a blend), a resonant low-pass whose envelope opens on the attack, and
/// an attack-decay-sustain-release amplifier.
struct SynthParams {
    double detune_cents = 12.0;
    float square_mix = 0.0f; ///< 0 = saws, 1 = squares
    double cutoff_hz = 1200.0, env_hz = 3000.0, filter_decay = 0.15, damping = 1.0;
    double attack = 0.003, decay = 0.25;
    float sustain = 0.2f;
    double release = 0.08;
};

/// Two-operator FM electric piano (Chowning FM): a sine carrier phase-modulated by a
/// sine at `ratio` × the carrier frequency. The modulation index decays faster than
/// the amplitude, so each note starts bright and bell-like and mellows into a round
/// tone, like a tine piano. Harder notes get a higher index (brighter), as on the
/// real instrument.
struct KeysParams {
    double ratio = 1.0;
    double index = 2.0, index_floor = 0.25, index_decay = 0.30;
    double attack = 0.004, amp_decay = 1.6, release = 0.15;
};

void render_kick(std::span<float> out, float velocity, const KickParams& p, int sample_rate);
void render_snare(std::span<float> out, float velocity, const SnareParams& p, int sample_rate,
                  Rng& noise);
void render_clap(std::span<float> out, float velocity, const ClapParams& p, int sample_rate,
                 Rng& noise);
void render_hat(std::span<float> out, float velocity, bool open, const HatParams& p,
                int sample_rate, Rng& noise);
void render_rim(std::span<float> out, float velocity, const RimParams& p, int sample_rate);
void render_shaker(std::span<float> out, float velocity, const ShakerParams& p, int sample_rate,
                   Rng& noise);
void render_tom(std::span<float> out, float velocity, double hz, const TomParams& p,
                int sample_rate);
void render_cowbell(std::span<float> out, float velocity, const CowbellParams& p, int sample_rate);
void render_cymbal(std::span<float> out, float velocity, const CymbalParams& p, int sample_rate,
                   Rng& noise);
/// One synth note held for `gate` samples, then released.
void render_synth_note(std::span<float> out, double hz, float velocity, std::size_t gate,
                       const SynthParams& p, int sample_rate);

/// One note of the bass line, in samples relative to the start of the output span.
struct BassNote {
    std::size_t start = 0;
    std::size_t length = 0; ///< gate length; the note then releases
    double hz = 55.0;
    float velocity = 1.0f;
    bool glide = false; ///< slide into this note from the previous one without re-attacking
};

/// Renders a whole monophonic bass line (notes sorted by start). Rendering the line in
/// one pass keeps the oscillator phase continuous across notes, which is what makes
/// glides smooth and retriggers click-free.
void render_bass_line(std::span<float> out, std::span<const BassNote> notes, const BassParams& p,
                      int sample_rate);

/// Tape "wow": slow pitch wobble shared by every note, as if the whole take were
/// played from a slightly warped cassette.
struct Wow {
    double depth = 0.0; ///< fractional pitch deviation, e.g. 0.003 ≈ ±5 cents
    double hz = 0.55;
    std::size_t origin = 0; ///< absolute frame of out[0], so all notes share one LFO
};

/// One FM electric-piano note held for `gate` samples, then released.
void render_keys_note(std::span<float> out, double hz, float velocity, std::size_t gate,
                      const KeysParams& p, int sample_rate, const Wow& wow = {});

/// Vinyl surface noise: random crackle pops (mostly tiny, occasionally loud) and a
/// soft hiss, independently on each channel. `amount` in [0, 1]; at 1 the loudest
/// pops peak around -25 dBFS, sitting under a full-scale mix.
void render_vinyl(std::span<float> left, std::span<float> right, float amount, int sample_rate,
                  Rng& rng);

} // namespace street::beats
