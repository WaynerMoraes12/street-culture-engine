// Era grooves: early-80s Bronx electro-funk and the NWOBHM gallop.

#include "groove_tools.hpp"
#include "grooves.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace street::beats::detail {

using namespace groove;

// ── Electro (1982) ──────────────────────────────────────────────────────────
// Everything on the TR-808 grid, dead straight: a syncopated boom kick, snare and
// handclap together on 2 and 4, machine-accented 16th hats, a cowbell riff, and a
// sequenced synth bass bouncing between root, octave and fifth under off-beat stabs.

namespace {
constexpr Spot kElectroKicks[] = {{3, .3}, {6, .6}, {7, .2}, {10, .55}, {11, .3}, {14, .25}};
constexpr Spot kElectroBells[] = {{0, .5},  {3, .6},  {6, .6},  {8, .3},
                                  {10, .5}, {11, .4}, {13, .4}, {14, .5}};
constexpr Spot kElectroStabs[] = {{0, .4}, {3, .45}, {6, .5}, {10, .45}, {11, .3}, {14, .35}};

/// One step of a sequencer bass line (step < 0 = unused slot).
struct SeqNote {
    int step, note;
};
constexpr std::array<std::array<SeqNote, 8>, 4> kElectroBasslines = {{
    {{{0, 0}, {2, 12}, {4, 0}, {6, 12}, {8, 0}, {10, 12}, {12, 7}, {14, 10}}}, // octave bounce
    {{{0, 0}, {3, 0}, {6, 12}, {8, 0}, {11, 0}, {14, 12}, {-1, 0}, {-1, 0}}},  // syncopated
    {{{0, 0}, {2, 0}, {3, 12}, {6, 7}, {8, 0}, {10, 0}, {11, 12}, {14, 10}}},
    {{{0, 0}, {1, 0}, {4, 12}, {6, 0}, {8, 7}, {10, 0}, {12, 12}, {14, 3}}},
}};
} // namespace

void groove_electro(Pattern& p, Rng& rng) {
    const std::vector<int> kick_a = with_downbeat(pick_steps(rng, kElectroKicks, rng.range(2, 3)));
    const std::vector<int> kick_b = mutate(kick_a, rng, kElectroKicks);
    std::vector<int> bells = pick_steps(rng, kElectroBells, rng.range(3, 5));
    const std::vector<int> stabs = pick_steps(rng, kElectroStabs, rng.range(2, 3));
    const auto& line_a =
        kElectroBasslines[rng.below(static_cast<std::uint32_t>(kElectroBasslines.size()))];
    const auto& line_b =
        kElectroBasslines[rng.below(static_cast<std::uint32_t>(kElectroBasslines.size()))];
    const bool open_offbeats = rng.chance(0.5);

    for (int bar = 0; bar < p.bars(); ++bar) {
        const BarRole role = role_of(bar, p.bars(), rng, 0.35);
        Bar b(p, bar);
        for (int s = 0; s < 16; ++s) // machine accents: loud 8ths, soft 16ths in between
            b.hit(I::HatClosed, s, vel(rng, s % 4 == 0 ? 0.8 : s % 2 == 0 ? 0.6 : 0.38, 0.03));
        if (bar % 2 == 1 && rng.chance(0.5)) bells = mutate(bells, rng, kElectroBells);
        for (int s : bells)
            b.hit(I::Cowbell, s, vel(rng, 0.7));
        for (int s : stabs)
            if (s == 0 || rng.chance(0.8)) b.hit(I::Keys, s, vel(rng, 0.75));
        if (role.intro) continue;

        if (open_offbeats)
            for (int s : {2, 6, 10, 14})
                if (rng.chance(0.6)) {
                    b.clear(I::HatClosed, s, s);
                    b.hit(I::HatOpen, s, vel(rng, 0.5));
                }
        for (int s : bar % 2 == 0 ? kick_a : kick_b)
            b.hit(I::Kick, s, vel(rng, s == 0 ? 1.0 : 0.85, 0.04));
        for (int s : {4, 12}) {
            b.hit(I::Snare, s, vel(rng, 0.9, 0.04));
            b.hit(I::Clap, s, vel(rng, 0.85, 0.04));
        }
        for (const SeqNote& n : bar % 2 == 0 ? line_a : line_b)
            if (n.step >= 0)
                b.hit(I::Bass, n.step, vel(rng, n.step % 4 == 0 ? 0.9 : 0.75), 0, n.note);

        if (role.dropout) { // breakdown: the low end and stabs drop, the claps roll on
            for (I part : {I::Kick, I::Bass, I::Keys})
                b.clear(part, 8, 15);
            b.hit(I::Clap, 14, vel(rng, 0.6));
            b.hit(I::Clap, 15, vel(rng, 0.75));
        } else if (role.phrase_end) { // 808 tom run, high to low
            b.clear(I::Kick, 12, 15);
            constexpr std::array<int, 4> kRun = {12, 7, 3, 0};
            for (int s = 12; s < 16; ++s)
                b.hit(I::Tom, s, vel(rng, 0.75 + 0.05 * (s - 12)), 0,
                      kRun[static_cast<std::size_t>(s - 12)]);
        }
    }
}

// ── Gallop (NWOBHM) ─────────────────────────────────────────────────────────
// The gallop is an eighth note plus two sixteenths on every beat (steps 0, 2, 3 of
// each group of four): da—da-da, da—da-da. Kick drum, bass and palm-muted power
// chords all ride it. Snare on 2 and 4, ride on the eighths, a crash to open each
// phrase; phrases end with a ringing open chord over a tom run, or a dead stop.

namespace {
/// Power-chord offset (semitones above the bar's chord root) for each beat of the riff.
constexpr std::array<std::array<int, 4>, 5> kGallopRiffs = {
    {{0, 0, 0, 0}, {0, 0, 0, 3}, {0, 0, 5, 3}, {0, 0, -2, -4}, {0, 0, 2, 3}}};
constexpr std::array<int, 3> kGallop = {0, 2, 3};
} // namespace

void groove_gallop(Pattern& p, Rng& rng) {
    const auto& riff_a = kGallopRiffs[rng.below(static_cast<std::uint32_t>(kGallopRiffs.size()))];
    const auto& riff_b = kGallopRiffs[rng.below(static_cast<std::uint32_t>(kGallopRiffs.size()))];
    const bool kick_every_beat = rng.chance(0.6); // else the kick gallops on beats 1 and 3
    const bool ride_eighths = rng.chance(0.6);    // else ride quarters with hats between

    for (int bar = 0; bar < p.bars(); ++bar) {
        const BarRole role = role_of(bar, p.bars(), rng, 0.35);
        const auto& riff = bar % 2 == 0 ? riff_a : riff_b;
        Bar b(p, bar);
        for (int beat = 0; beat < 4; ++beat)
            for (int k : kGallop) {
                const float v = vel(rng, k == 0 ? 0.95 : 0.78, 0.04);
                b.hit(I::Guitar, beat * 4 + k, v, 0, riff[static_cast<std::size_t>(beat)]);
                b.hit(I::Bass, beat * 4 + k, v, 0, riff[static_cast<std::size_t>(beat)]);
            }
        if (role.intro) continue; // the riff alone; the drums enter on a crash

        if (bar == 1 || bar % 4 == 0) b.hit(I::Crash, 0, vel(rng, 0.95, 0.03));
        if (ride_eighths) {
            for (int s = 0; s < 16; s += 2)
                b.hit(I::Ride, s, vel(rng, s % 4 == 0 ? 0.75 : 0.55));
        } else {
            for (int s : {0, 4, 8, 12})
                b.hit(I::Ride, s, vel(rng, 0.8));
            for (int s : {2, 6, 10, 14})
                b.hit(I::HatClosed, s, vel(rng, 0.5));
        }
        for (int beat = 0; beat < 4; ++beat)
            if (kick_every_beat || beat % 2 == 0)
                for (int k : kGallop)
                    b.hit(I::Kick, beat * 4 + k, vel(rng, k == 0 ? 0.95 : 0.8, 0.04));
        b.hit(I::Snare, 4, vel(rng, 0.95, 0.03));
        b.hit(I::Snare, 12, vel(rng, 0.97, 0.03));

        if (role.dropout) { // the dead stop: one huge hit, silence, snare pickup
            for (I part : {I::Guitar, I::Bass, I::Kick, I::Ride, I::HatClosed, I::Snare})
                b.clear(part, 1, 15);
            b.ring(I::Guitar, 0, 1.0f, riff[0]);
            b.hit(I::Crash, 0, vel(rng, 1.0, 0.0));
            b.hit(I::Snare, 14, vel(rng, 0.8));
            b.hit(I::Snare, 15, vel(rng, 0.9));
        } else if (role.phrase_end) { // open chord rings over a tom run
            for (I part : {I::Guitar, I::Bass, I::Kick, I::Ride, I::HatClosed, I::Snare})
                b.clear(part, 8, 15);
            b.ring(I::Guitar, 8, 1.0f, riff[2]);
            b.hit(I::Bass, 8, vel(rng, 0.95), 0, riff[2]);
            b.hit(I::Crash, 8, vel(rng, 0.85));
            constexpr std::array<int, 6> kRun = {12, 12, 7, 7, 0, 0};
            b.hit(I::Snare, 8, vel(rng, 0.9));
            b.hit(I::Snare, 9, vel(rng, 0.8));
            for (int s = 10; s < 16; ++s)
                b.hit(I::Tom, s, vel(rng, 0.8 + 0.03 * (s - 10)), 0,
                      kRun[static_cast<std::size_t>(s - 10)]);
        }
    }
}

} // namespace street::beats::detail
