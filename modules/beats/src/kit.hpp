#pragma once
// Internal: each style's sound palette, mix and "feel" constants.
//
// Kept apart from the composer on purpose: the composer decides *what* is played,
// the kit decides *how it sounds*. Rendering a hand-written pattern with
// --style lofi runs it through the lo-fi kit.

#include "street/beats/pattern.hpp"
#include "street/beats/style.hpp"
#include "street/beats/voices.hpp"

#include <array>

namespace street::beats::detail {

struct Channel {
    float gain = 0.5f;
    float pan = 0.0f; ///< -1 left … +1 right
};

struct Kit {
    KickParams kick;
    SnareParams snare;
    ClapParams clap;
    HatParams hat;
    RimParams rim;
    ShakerParams shaker;
    TomParams tom;
    CowbellParams cowbell;
    CymbalParams crash{
        .decay = 1.3, .cutoff_hz = 6000.0, .tone = 1.6, .noise_mix = 0.75f, .bell = 0.0f};
    CymbalParams ride{
        .decay = 0.38, .cutoff_hz = 5200.0, .tone = 1.3, .noise_mix = 0.25f, .bell = 0.6f};
    BassParams bass;
    KeysParams keys;
    bool synth_keys = false; ///< keys play `stab` (subtractive synth) instead of the FM piano
    SynthParams stab;
    SynthParams guitar_mute; ///< palm-muted chug
    SynthParams guitar_ring; ///< open, ringing chord
    std::array<Channel, kInstrumentCount> mix{};

    // Feel.
    double groove_ms = 3.0;     ///< shared per-step timing drift (the whole band pushes/pulls)
    double jitter_ms = 1.0;     ///< extra per-voice jitter for hats and percussion
    double backbeat_lag_ms = 0; ///< snares/claps laid back behind the grid
    double velocity_spread = 0.06;
    double strum_ms = 0;    ///< delay between successive chord notes
    int bass_max_steps = 8; ///< longest an 808 note may be held

    // Effects.
    int crush_bits = 0; ///< 0 = off; applied to drums + keys
    double crush_rate_hz = 0;
    float drum_drive = 0.0f;      ///< tanh drive on the drum bus
    double master_lowpass_hz = 0; ///< 0 = off
    double keys_lowpass_hz = 0;
    float vinyl = 0.0f;
    Wow wow;                   ///< tape wobble on the keys
    float keys_autopan = 0.0f; ///< depth of the suitcase-piano auto-pan
    double keys_autopan_hz = 0.0;
    float guitar_drive = 8.0f;         ///< amp distortion on the summed power chord
    double guitar_lowpass_hz = 4500.0; ///< speaker-cabinet roll-off
    double guitar_double_ms = 0.0;     ///< > 0: fake double-tracking, hard left/right

    const Channel& channel(Instrument i) const noexcept { return mix[static_cast<std::size_t>(i)]; }
};

const Kit& kit_for(Style style);

} // namespace street::beats::detail
