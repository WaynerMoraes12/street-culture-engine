#include "kit.hpp"

#include <initializer_list>
#include <utility>

namespace street::beats::detail {
namespace {

using I = Instrument;

/// Channel strip per instrument; anything not listed keeps the default (0.5, centre).
/// Hats and shakers sit to opposite sides for width; kick, snare and bass stay centred.
std::array<Channel, kInstrumentCount>
make_mix(std::initializer_list<std::pair<Instrument, Channel>> strips) {
    std::array<Channel, kInstrumentCount> mix{};
    for (const auto& [instrument, channel] : strips)
        mix[static_cast<std::size_t>(instrument)] = channel;
    return mix;
}

Kit boombap() {
    Kit k;
    k.kick = {.start_hz = 160,
              .end_hz = 52,
              .pitch_decay = 0.03,
              .amp_decay = 0.22,
              .drive = 2.6f,
              .click = 0.35f};
    k.snare = {.tone_hz = 185,
               .tone_decay = 0.06,
               .noise_decay = 0.16,
               .noise_highpass_hz = 1500,
               .tone_mix = 0.4f};
    k.clap = {};
    k.hat = {.closed_decay = 0.022,
             .open_decay = 0.18,
             .cutoff_hz = 6800,
             .tone = 0.95,
             .noise_mix = 0.25f};
    k.bass = {.decay = 0.6, .release = 0.04, .glide = 0.04, .knock = 0.3, .drive = 1.3f};
    k.keys = {.ratio = 1.0,
              .index = 2.4,
              .index_floor = 0.3,
              .index_decay = 0.25,
              .attack = 0.003,
              .amp_decay = 1.4,
              .release = 0.12};
    k.mix = make_mix({
        {I::Kick, {0.95f, 0.0f}},
        {I::Snare, {0.75f, 0.02f}},
        {I::Clap, {0.5f, -0.1f}},
        {I::Rim, {0.4f, -0.2f}},
        {I::HatClosed, {0.28f, 0.3f}},
        {I::HatOpen, {0.2f, 0.3f}},
        {I::Shaker, {0.2f, -0.35f}},
        {I::Tom, {0.5f, -0.15f}},
        {I::Bass, {0.5f, 0.0f}},
        {I::Keys, {0.2f, 0.0f}},
    });
    k.groove_ms = 4.0;
    k.jitter_ms = 2.0;
    k.backbeat_lag_ms = 4.0;
    k.velocity_spread = 0.07;
    k.strum_ms = 6.0;
    k.bass_max_steps = 6;
    k.crush_bits = 12;
    k.crush_rate_hz = 26040.0; // SP-1200
    k.drum_drive = 1.4f;
    k.keys_lowpass_hz = 5200.0;
    k.vinyl = 0.25f;
    k.keys_autopan = 0.25f;
    k.keys_autopan_hz = 0.25;
    return k;
}

Kit trap() {
    Kit k;
    k.kick = {.start_hz = 180,
              .end_hz = 55,
              .pitch_decay = 0.025,
              .amp_decay = 0.16,
              .drive = 3.0f,
              .click = 0.45f};
    k.snare = {.tone_hz = 200,
               .tone_decay = 0.05,
               .noise_decay = 0.12,
               .noise_highpass_hz = 2200,
               .tone_mix = 0.35f};
    k.clap = {.bursts = 4,
              .spacing = 0.009,
              .burst_decay = 0.0035,
              .tail_decay = 0.13,
              .center_hz = 1300,
              .q = 1.2};
    k.hat = {.closed_decay = 0.014,
             .open_decay = 0.22,
             .cutoff_hz = 8500,
             .tone = 1.1,
             .noise_mix = 0.2f};
    k.bass = {.decay = 1.6, .release = 0.05, .glide = 0.05, .knock = 0.8, .drive = 2.6f};
    k.keys = {.ratio = 3.5,
              .index = 1.6,
              .index_floor = 0.15,
              .index_decay = 0.5,
              .attack = 0.002,
              .amp_decay = 1.1,
              .release = 0.3}; // glassy FM bell
    k.mix = make_mix({
        {I::Kick, {0.8f, 0.0f}},
        {I::Snare, {0.5f, 0.0f}},
        {I::Clap, {0.6f, 0.0f}},
        {I::Rim, {0.35f, -0.25f}},
        {I::HatClosed, {0.24f, 0.35f}},
        {I::HatOpen, {0.2f, -0.3f}},
        {I::Shaker, {0.2f, -0.4f}},
        {I::Tom, {0.45f, 0.15f}},
        {I::Bass, {0.8f, 0.0f}},
        {I::Keys, {0.1f, 0.0f}},
    });
    k.groove_ms = 1.0;
    k.jitter_ms = 0.8;
    k.velocity_spread = 0.05;
    k.bass_max_steps = 16;
    k.keys_lowpass_hz = 6000.0;
    k.keys_autopan = 0.4f;
    k.keys_autopan_hz = 0.15;
    return k;
}

Kit lofi() {
    Kit k;
    k.kick = {.start_hz = 120,
              .end_hz = 48,
              .pitch_decay = 0.04,
              .amp_decay = 0.25,
              .drive = 1.4f,
              .click = 0.1f};
    k.snare = {.tone_hz = 175,
               .tone_decay = 0.08,
               .noise_decay = 0.12,
               .noise_highpass_hz = 1200,
               .tone_mix = 0.5f};
    k.hat = {.closed_decay = 0.02,
             .open_decay = 0.16,
             .cutoff_hz = 6000,
             .tone = 0.85,
             .noise_mix = 0.3f};
    k.rim = {.decay = 0.014};
    k.shaker = {.attack = 0.015, .decay = 0.05, .highpass_hz = 4500};
    k.bass = {.decay = 0.8, .release = 0.06, .glide = 0.05, .knock = 0.15, .drive = 1.0f};
    k.keys = {.ratio = 1.0,
              .index = 1.8,
              .index_floor = 0.2,
              .index_decay = 0.4,
              .attack = 0.006,
              .amp_decay = 2.2,
              .release = 0.25};
    k.mix = make_mix({
        {I::Kick, {0.75f, 0.0f}},
        {I::Snare, {0.5f, 0.05f}},
        {I::Clap, {0.4f, -0.1f}},
        {I::Rim, {0.45f, -0.15f}},
        {I::HatClosed, {0.2f, 0.3f}},
        {I::HatOpen, {0.16f, 0.3f}},
        {I::Shaker, {0.16f, -0.35f}},
        {I::Tom, {0.4f, -0.1f}},
        {I::Bass, {0.45f, 0.0f}},
        {I::Keys, {0.26f, 0.0f}},
    });
    k.groove_ms = 6.0;
    k.jitter_ms = 3.0;
    k.backbeat_lag_ms = 9.0; // the classic "late snare"
    k.velocity_spread = 0.1;
    k.strum_ms = 14.0;
    k.bass_max_steps = 8;
    k.master_lowpass_hz = 4200.0;
    k.vinyl = 0.7f;
    k.wow = {.depth = 0.0035, .hz = 0.55, .origin = 0};
    k.keys_autopan = 0.35f;
    k.keys_autopan_hz = 0.2;
    return k;
}

Kit drill() {
    Kit k;
    k.kick = {.start_hz = 170,
              .end_hz = 50,
              .pitch_decay = 0.03,
              .amp_decay = 0.18,
              .drive = 2.8f,
              .click = 0.4f};
    k.snare = {.tone_hz = 210,
               .tone_decay = 0.05,
               .noise_decay = 0.14,
               .noise_highpass_hz = 2000,
               .tone_mix = 0.3f};
    k.clap = {.bursts = 3,
              .spacing = 0.011,
              .burst_decay = 0.004,
              .tail_decay = 0.1,
              .center_hz = 1400,
              .q = 1.4};
    k.hat = {.closed_decay = 0.012,
             .open_decay = 0.2,
             .cutoff_hz = 9000,
             .tone = 1.15,
             .noise_mix = 0.18f};
    k.bass = {.decay = 1.8, .release = 0.05, .glide = 0.07, .knock = 0.7, .drive = 3.0f};
    k.keys = {.ratio = 3.5,
              .index = 1.3,
              .index_floor = 0.1,
              .index_decay = 0.6,
              .attack = 0.003,
              .amp_decay = 1.3,
              .release = 0.35};
    k.mix = make_mix({
        {I::Kick, {0.7f, 0.0f}},
        {I::Snare, {0.6f, 0.0f}},
        {I::Clap, {0.35f, 0.08f}},
        {I::Rim, {0.35f, -0.25f}},
        {I::HatClosed, {0.24f, 0.35f}},
        {I::HatOpen, {0.18f, -0.3f}},
        {I::Shaker, {0.18f, -0.4f}},
        {I::Tom, {0.4f, 0.1f}},
        {I::Bass, {0.85f, 0.0f}},
        {I::Keys, {0.1f, 0.0f}},
    });
    k.groove_ms = 1.2;
    k.jitter_ms = 0.8;
    k.velocity_spread = 0.05;
    k.bass_max_steps = 12;
    k.keys_lowpass_hz = 4500.0;
    k.keys_autopan = 0.45f;
    k.keys_autopan_hz = 0.12;
    return k;
}

Kit funk() {
    Kit k;
    k.kick = {.start_hz = 170,
              .end_hz = 46,
              .pitch_decay = 0.045,
              .amp_decay = 0.3,
              .drive = 3.2f,
              .click = 0.3f};
    k.clap = {.bursts = 4,
              .spacing = 0.01,
              .burst_decay = 0.004,
              .tail_decay = 0.14,
              .center_hz = 1150,
              .q = 1.1};
    k.hat = {.closed_decay = 0.016,
             .open_decay = 0.18,
             .cutoff_hz = 8000,
             .tone = 1.0,
             .noise_mix = 0.2f};
    k.rim = {.decay = 0.01};
    k.shaker = {.attack = 0.008, .decay = 0.04, .highpass_hz = 6000};
    k.tom = {.hz = 88.0, .decay = 0.28, .bend = 0.7};
    k.bass = {.decay = 1.2, .release = 0.05, .glide = 0.05, .knock = 0.9, .drive = 3.4f};
    k.keys = {.ratio = 3.5,
              .index = 1.5,
              .index_floor = 0.15,
              .index_decay = 0.3,
              .attack = 0.002,
              .amp_decay = 0.6,
              .release = 0.12};
    k.mix = make_mix({
        {I::Kick, {0.85f, 0.0f}},
        {I::Snare, {0.5f, 0.0f}},
        {I::Clap, {0.6f, 0.0f}},
        {I::Rim, {0.35f, 0.25f}},
        {I::HatClosed, {0.2f, 0.35f}},
        {I::HatOpen, {0.18f, 0.3f}},
        {I::Shaker, {0.3f, -0.35f}},
        {I::Tom, {0.7f, -0.12f}},
        {I::Bass, {0.75f, 0.0f}},
        {I::Keys, {0.1f, 0.0f}},
    });
    k.groove_ms = 2.5;
    k.jitter_ms = 1.5;
    k.velocity_spread = 0.07;
    k.bass_max_steps = 6;
    k.drum_drive = 1.6f;
    k.keys_autopan = 0.3f;
    k.keys_autopan_hz = 0.3;
    return k;
}

// Early-80s electro: a boomy, long 808 kick, snappy snare doubled by the 808 clap, the
// cowbell, bright 16th hats, a resonant sequenced saw bass and saw/square chord stabs.
Kit electro() {
    Kit k;
    k.kick = {.start_hz = 130,
              .end_hz = 47,
              .pitch_decay = 0.05,
              .amp_decay = 0.42,
              .drive = 1.6f,
              .click = 0.15f};
    k.snare = {.tone_hz = 235,
               .tone_decay = 0.04,
               .noise_decay = 0.1,
               .noise_highpass_hz = 2600,
               .tone_mix = 0.35f};
    k.clap = {.bursts = 4,
              .spacing = 0.009,
              .burst_decay = 0.0035,
              .tail_decay = 0.15,
              .center_hz = 1100,
              .q = 1.0};
    k.hat = {.closed_decay = 0.02,
             .open_decay = 0.24,
             .cutoff_hz = 8200,
             .tone = 1.0,
             .noise_mix = 0.12f};
    k.cowbell = {.decay = 0.05, .tail = 0.28, .tone = 1.0};
    k.tom = {.hz = 125.0, .decay = 0.3, .bend = 0.45};
    k.bass = {.shape = BassShape::Saw,
              .decay = 0.35,
              .release = 0.03,
              .glide = 0.02,
              .knock = 0.0,
              .drive = 1.4f,
              .cutoff_hz = 260.0,
              .env_hz = 2200.0,
              .filter_decay = 0.09,
              .damping = 0.55};
    k.synth_keys = true;
    k.stab = {.detune_cents = 9.0,
              .square_mix = 0.35f,
              .cutoff_hz = 1400.0,
              .env_hz = 4200.0,
              .filter_decay = 0.12,
              .damping = 0.8,
              .attack = 0.002,
              .decay = 0.14,
              .sustain = 0.25f,
              .release = 0.07};
    k.mix = make_mix({
        {I::Kick, {0.9f, 0.0f}},
        {I::Snare, {0.55f, 0.0f}},
        {I::Clap, {0.55f, 0.0f}},
        {I::Cowbell, {0.3f, 0.3f}},
        {I::HatClosed, {0.2f, 0.25f}},
        {I::HatOpen, {0.17f, -0.25f}},
        {I::Tom, {0.45f, -0.15f}},
        {I::Bass, {0.5f, 0.0f}},
        {I::Keys, {0.2f, 0.0f}},
    });
    k.groove_ms = 0.8; // machine-tight
    k.jitter_ms = 0.4;
    k.velocity_spread = 0.04;
    k.bass_max_steps = 2;
    k.drum_drive = 1.1f;
    k.keys_autopan = 0.35f;
    k.keys_autopan_hz = 0.25;
    return k;
}

// NWOBHM gallop: tight clicky double-kick, a big snare, ride and crash, a gritty saw bass,
// and power chords from detuned saw/square pairs driven hard into a cabinet low-pass.
Kit gallop() {
    Kit k;
    k.kick = {.start_hz = 120,
              .end_hz = 58,
              .pitch_decay = 0.018,
              .amp_decay = 0.12,
              .drive = 2.4f,
              .click = 0.7f};
    k.snare = {.tone_hz = 195,
               .tone_decay = 0.09,
               .noise_decay = 0.2,
               .noise_highpass_hz = 1400,
               .tone_mix = 0.45f};
    k.hat = {.closed_decay = 0.025,
             .open_decay = 0.3,
             .cutoff_hz = 7000,
             .tone = 1.2,
             .noise_mix = 0.3f};
    k.tom = {.hz = 105.0, .decay = 0.3, .bend = 0.35};
    k.bass = {.shape = BassShape::Saw,
              .decay = 0.4,
              .release = 0.02,
              .glide = 0.02,
              .knock = 0.0,
              .drive = 2.6f,
              .cutoff_hz = 650.0,
              .env_hz = 900.0,
              .filter_decay = 0.08,
              .damping = 1.2};
    k.guitar_mute = {.detune_cents = 14.0,
                     .square_mix = 0.5f,
                     .cutoff_hz = 700.0,
                     .env_hz = 1300.0,
                     .filter_decay = 0.04,
                     .damping = 1.3,
                     .attack = 0.0015,
                     .decay = 0.06,
                     .sustain = 0.0f,
                     .release = 0.025};
    k.guitar_ring = {.detune_cents = 14.0,
                     .square_mix = 0.5f,
                     .cutoff_hz = 1800.0,
                     .env_hz = 2500.0,
                     .filter_decay = 0.35,
                     .damping = 1.1,
                     .attack = 0.002,
                     .decay = 1.0,
                     .sustain = 0.45f,
                     .release = 0.25};
    k.mix = make_mix({
        {I::Kick, {0.8f, 0.0f}},
        {I::Snare, {0.72f, 0.0f}},
        {I::HatClosed, {0.18f, 0.3f}},
        {I::Ride, {0.2f, 0.35f}},
        {I::Crash, {0.26f, -0.3f}},
        {I::Tom, {0.5f, 0.1f}},
        {I::Bass, {0.42f, 0.0f}},
        {I::Guitar, {0.45f, 0.0f}},
    });
    k.groove_ms = 1.5;
    k.jitter_ms = 0.8;
    k.velocity_spread = 0.05;
    k.bass_max_steps = 2;
    k.guitar_drive = 9.0f;
    k.guitar_lowpass_hz = 4200.0;
    k.guitar_double_ms = 14.0;
    return k;
}

} // namespace

const Kit& kit_for(Style style) {
    static const Kit kElectro = electro();
    static const Kit kGallop = gallop();
    static const Kit kBoomBap = boombap();
    static const Kit kTrap = trap();
    static const Kit kLoFi = lofi();
    static const Kit kDrill = drill();
    static const Kit kFunk = funk();
    switch (style) {
    case Style::Trap:
        return kTrap;
    case Style::LoFi:
        return kLoFi;
    case Style::Drill:
        return kDrill;
    case Style::Funk:
        return kFunk;
    case Style::Electro:
        return kElectro;
    case Style::Gallop:
        return kGallop;
    case Style::BoomBap:
        break;
    }
    return kBoomBap;
}

} // namespace street::beats::detail
