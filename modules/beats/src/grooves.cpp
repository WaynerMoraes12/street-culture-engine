#include "grooves.hpp"

#include "groove_tools.hpp"

#include <algorithm>
#include <array>
#include <span>
#include <vector>

namespace street::beats::detail {

using namespace groove;

namespace {

// ── Boom bap ────────────────────────────────────────────────────────────────
// Kick on 1 plus two or three syncopated pushes (the "and of 3" is the classic),
// snare cracking on 2 and 4 with ghost notes around it, eighth-note hats that
// occasionally open, a comping chord hit. Bass doubles the kick on the chord root.

constexpr Spot kBoomBapKicks[] = {{2, .15}, {3, .35}, {6, .2},   {7, .45}, {9, .35},
                                  {10, .7}, {11, .3}, {13, .25}, {14, .2}, {15, .1}};
constexpr Spot kBoomBapGhosts[] = {{3, .3}, {7, .5}, {9, .4}, {11, .2}, {15, .5}};
constexpr Spot kBoomBapComp[] = {{6, .2}, {7, .3}, {10, .3}, {11, .2}, {14, .2}};

} // namespace

void groove_boombap(Pattern& p, Rng& rng) {
    const std::vector<int> kick_a = with_downbeat(pick_steps(rng, kBoomBapKicks, rng.range(2, 3)));
    const std::vector<int> kick_b = mutate(kick_a, rng, kBoomBapKicks);
    const std::vector<int> ghosts = pick_steps(rng, kBoomBapGhosts, rng.range(1, 2));
    const bool busy_hats = rng.chance(0.3);
    const int open_spot = rng.chance(0.6) ? 14 : 10;
    const int comp_spot = pick_steps(rng, kBoomBapComp, 1).front();

    for (int bar = 0; bar < p.bars(); ++bar) {
        const BarRole role = role_of(bar, p.bars(), rng, 0.3);
        Bar b(p, bar);
        b.hit(I::Keys, 0, vel(rng, 0.8));
        if (rng.chance(0.55)) b.hit(I::Keys, comp_spot, vel(rng, 0.6));

        for (int s = 0; s < 16; ++s) {
            if (s % 2 == 1 && !(busy_hats && rng.chance(0.7))) continue;
            b.hit(I::HatClosed, s, vel(rng, s % 4 == 0 ? 0.75 : s % 2 == 0 ? 0.55 : 0.35));
        }
        if (rng.chance(0.45)) {
            b.clear(I::HatClosed, open_spot, open_spot);
            b.hit(I::HatOpen, open_spot, vel(rng, 0.6));
        }
        if (role.intro) continue;

        std::vector<int> kicks = bar % 2 == 0 ? kick_a : kick_b;
        if (rng.chance(0.25)) kicks = mutate(kicks, rng, kBoomBapKicks);
        for (int s : kicks) {
            b.hit(I::Kick, s, vel(rng, s == 0 ? 0.95 : 0.8));
            b.hit(I::Bass, s, vel(rng, 0.8));
        }
        b.hit(I::Snare, 4, vel(rng, 0.95, 0.04));
        b.hit(I::Snare, 12, vel(rng, 0.97, 0.03));
        for (int g : ghosts)
            if (rng.chance(0.75)) b.hit(I::Snare, g, vel(rng, 0.3, 0.06));

        if (role.dropout) {
            b.clear(I::Kick, 8, 15);
            b.clear(I::Bass, 8, 15);
            b.clear(I::HatClosed, 12, 15);
            b.hit(I::HatOpen, 14, vel(rng, 0.7));
        } else if (role.phrase_end) {
            b.clear(I::HatClosed, 12, 15);
            b.clear(I::HatOpen, 12, 15);
            switch (rng.range(0, 2)) {
            case 0: // snare run into the one
                b.hit(I::Snare, 13, vel(rng, 0.6));
                b.hit(I::Snare, 14, vel(rng, 0.75));
                b.hit(I::Snare, 15, vel(rng, 0.9));
                break;
            case 1: // kick-snare-flam
                b.hit(I::Kick, 13, vel(rng, 0.8));
                b.hit(I::Snare, 14, vel(rng, 0.8));
                b.hit(I::Snare, 15, vel(rng, 0.65), 2);
                break;
            default: // call and response
                b.hit(I::Snare, 10, vel(rng, 0.5));
                b.hit(I::Snare, 11, vel(rng, 0.6));
                b.hit(I::Snare, 14, vel(rng, 0.8));
                b.hit(I::Snare, 15, vel(rng, 0.95));
                break;
            }
        }
    }
}

// ── Trap ────────────────────────────────────────────────────────────────────
// Half-time: clap + snare on beat 3 (step 8) of every bar. Hats run in 8ths or
// 16ths with 1/32 and triplet rolls; the 808 doubles the kick on the chord root
// and swoops up an octave (sliding back down on the next downbeat).

namespace {
constexpr Spot kTrapKicks[] = {{3, .3},   {6, .25}, {7, .35}, {10, .5},
                               {11, .35}, {13, .2}, {14, .3}};
constexpr Spot kTrapRolls[] = {{3, .3}, {6, .3}, {7, .4}, {11, .4}, {14, .5}, {15, .4}};
} // namespace

void groove_trap(Pattern& p, Rng& rng) {
    const std::vector<int> kick_a = with_downbeat(pick_steps(rng, kTrapKicks, 2));
    const std::vector<int> kick_b = mutate(kick_a, rng, kTrapKicks);
    const bool sixteenths = rng.chance(0.5);
    const bool swoops = rng.chance(0.65);
    std::vector<int> rolls = pick_steps(rng, kTrapRolls, rng.range(1, 2));
    bool slide_back = false;

    for (int bar = 0; bar < p.bars(); ++bar) {
        const BarRole role = role_of(bar, p.bars(), rng, 0.35);
        Bar b(p, bar);
        b.hit(I::Keys, 0, vel(rng, 0.7));
        if (rng.chance(0.35)) b.hit(I::Keys, 10, vel(rng, 0.5));

        for (int s = 0; s < 16; ++s) {
            if ((!sixteenths || role.intro) && s % 2 == 1) continue;
            b.hit(I::HatClosed, s, vel(rng, s % 4 == 0 ? 0.75 : 0.5));
        }
        if (rng.chance(0.3)) {
            const int s = rng.chance(0.5) ? 10 : 2;
            b.clear(I::HatClosed, s, s);
            b.hit(I::HatOpen, s, vel(rng, 0.55));
        }
        if (role.intro) continue;

        if (rng.chance(0.5)) rolls = mutate(rolls, rng, kTrapRolls);
        for (int r : rolls)
            b.hit(I::HatClosed, r, vel(rng, 0.6), rng.chance(0.55) ? 2 : 3);

        b.hit(I::Clap, 8, vel(rng, 0.95, 0.03));
        b.hit(I::Snare, 8, vel(rng, 0.8));
        for (int s : bar % 2 == 0 ? kick_a : kick_b) {
            b.hit(I::Kick, s, vel(rng, 0.9));
            b.hit(I::Bass, s, vel(rng, 0.9), 0, 0, s == 0 && slide_back);
        }
        slide_back = false;
        if (swoops && rng.chance(0.5)) {
            const int s = rng.chance(0.5) ? 14 : 15;
            b.hit(I::Bass, s, vel(rng, 0.85), 0, 12, true);
            slide_back = true;
        }

        if (role.dropout) {
            b.clear(I::Kick, 8, 15);
            b.clear(I::Bass, 8, 15);
            b.hit(I::HatClosed, 15, vel(rng, 0.7), 3);
            slide_back = false;
        } else if (role.phrase_end) {
            for (int s = 12; s < 16; ++s)
                b.hit(I::HatClosed, s, vel(rng, 0.5 + 0.12 * (s - 12)), s < 14 ? 3 : 4);
            b.hit(I::Snare, 14, vel(rng, 0.55), 3);
            b.hit(I::Snare, 15, vel(rng, 0.75), 3);
        }
    }
}

// ── Lo-fi ───────────────────────────────────────────────────────────────────
// Sparse and soft: a kick or two, snare or side-stick on 2 and 4, eighth hats
// with gaps, an optional shaker. Long e-piano chords carry the track.

namespace {
constexpr Spot kLoFiKicks[] = {{3, .2}, {7, .4}, {10, .6}, {11, .3}, {13, .2}};
} // namespace

void groove_lofi(Pattern& p, Rng& rng) {
    const std::vector<int> kick_a = with_downbeat(pick_steps(rng, kLoFiKicks, rng.range(1, 2)));
    const std::vector<int> kick_b = mutate(kick_a, rng, kLoFiKicks);
    const I backbeat = rng.chance(0.4) ? I::Rim : I::Snare;
    const bool shaker = rng.chance(0.4);
    const bool restrike = rng.chance(0.4);

    for (int bar = 0; bar < p.bars(); ++bar) {
        const BarRole role = role_of(bar, p.bars(), rng, 0.4);
        Bar b(p, bar);
        b.hit(I::Keys, 0, vel(rng, 0.75));
        if (restrike && rng.chance(0.6)) b.hit(I::Keys, rng.chance(0.5) ? 8 : 10, vel(rng, 0.55));
        if (shaker)
            for (int s = 0; s < 16; ++s)
                b.hit(I::Shaker, s, vel(rng, s % 2 == 0 ? 0.35 : 0.22, 0.06));
        if (role.intro) continue;

        for (int s = 0; s < 16; s += 2)
            if (rng.chance(0.8)) b.hit(I::HatClosed, s, vel(rng, s % 4 == 0 ? 0.6 : 0.42, 0.08));
        const std::vector<int>& kicks = bar % 2 == 0 ? kick_a : kick_b;
        for (int s : kicks) {
            b.hit(I::Kick, s, vel(rng, s == 0 ? 0.8 : 0.65));
            if (s == 0 || rng.chance(0.5)) b.hit(I::Bass, s, vel(rng, 0.7));
        }
        b.hit(backbeat, 4, vel(rng, 0.8));
        b.hit(backbeat, 12, vel(rng, 0.8));
        if (rng.chance(0.35)) b.hit(I::Snare, rng.chance(0.5) ? 15 : 7, vel(rng, 0.25, 0.04));

        if (role.dropout) {
            for (I drum : {I::Kick, I::HatClosed, I::Snare, I::Rim, I::Bass})
                b.clear(drum, 8, 15);
        } else if (role.phrase_end) {
            b.clear(I::HatClosed, 14, 15);
            b.hit(I::Kick, 14, vel(rng, 0.6));
            b.hit(I::Snare, 15, vel(rng, 0.35));
        }
    }
}

// ── Drill ───────────────────────────────────────────────────────────────────
// Hats grouped in threes and twos (3-3-2-3-3-2 …) give drill its skippy,
// off-kilter drive; triplet rolls on top. Snare on beat 3 with off-beat stabs
// that alternate between bars. The 808 plays gliding melodic gestures.

namespace {
constexpr std::array<std::array<int, 6>, 5> kDrillHatGroups = {{{3, 3, 2, 3, 3, 2},
                                                                {3, 3, 4, 3, 3, 0},
                                                                {2, 3, 3, 2, 3, 3},
                                                                {3, 3, 3, 3, 2, 2},
                                                                {4, 3, 3, 3, 3, 0}}};

struct Glide {
    int step, note;
    bool slide;
};
constexpr std::array<std::array<Glide, 4>, 4> kDrillGestures = {{
    {{{0, 0, false}, {6, 7, true}, {10, 0, true}, {14, 12, true}}},
    {{{0, 0, false}, {3, 0, false}, {7, -2, true}, {11, 0, true}}},
    {{{0, 0, false}, {8, 3, true}, {11, 5, true}, {14, 0, true}}},
    {{{0, 0, false}, {6, 12, true}, {8, 0, true}, {13, -5, true}}},
}};
constexpr Spot kDrillKicks[] = {{10, .5}, {11, .3}, {6, .3}, {3, .2}};
} // namespace

void groove_drill(Pattern& p, Rng& rng) {
    std::vector<int> hat_steps;
    int position = 0;
    for (int group :
         kDrillHatGroups[rng.below(static_cast<std::uint32_t>(kDrillHatGroups.size()))]) {
        if (group == 0) break;
        hat_steps.push_back(position);
        position += group;
    }
    const auto& gesture_a =
        kDrillGestures[rng.below(static_cast<std::uint32_t>(kDrillGestures.size()))];
    const auto& gesture_b =
        kDrillGestures[rng.below(static_cast<std::uint32_t>(kDrillGestures.size()))];
    const std::vector<int> kick_a = with_downbeat(pick_steps(rng, kDrillKicks, 1));
    const std::vector<int> kick_b = mutate(kick_a, rng, kDrillKicks);

    for (int bar = 0; bar < p.bars(); ++bar) {
        const BarRole role = role_of(bar, p.bars(), rng, 0.35);
        Bar b(p, bar);
        b.hit(I::Keys, 0, vel(rng, 0.65));
        if (rng.chance(0.3)) b.hit(I::Keys, 8, vel(rng, 0.5));

        for (int s : hat_steps)
            b.hit(I::HatClosed, s, vel(rng, s % 4 == 0 ? 0.75 : 0.55));
        if (rng.chance(0.5)) {
            const int s = hat_steps[rng.below(static_cast<std::uint32_t>(hat_steps.size()))] + 1;
            if (s < 16 && !b.has(I::HatClosed, s)) b.hit(I::HatClosed, s, vel(rng, 0.4));
        }
        if (role.intro) continue;

        for (int k = rng.range(1, 2); k > 0; --k) {
            const int s =
                hat_steps[1 + rng.below(static_cast<std::uint32_t>(hat_steps.size() - 1))];
            b.hit(I::HatClosed, s, vel(rng, 0.6), 3);
        }
        b.hit(I::Snare, 8, vel(rng, 0.95, 0.03));
        b.hit(I::Clap, 8, vel(rng, 0.6));
        if (bar % 2 == 0) {
            if (rng.chance(0.6)) b.hit(I::Snare, 11, vel(rng, 0.55));
        } else if (rng.chance(0.75)) {
            b.hit(I::Snare, rng.chance(0.5) ? 14 : 15, vel(rng, 0.7));
        }
        if (rng.chance(0.3)) b.hit(I::Snare, 3, vel(rng, 0.35));

        for (int s : bar % 2 == 0 ? kick_a : kick_b)
            b.hit(I::Kick, s, vel(rng, 0.85));
        for (const Glide& g : bar % 2 == 0 ? gesture_a : gesture_b)
            b.hit(I::Bass, g.step, vel(rng, 0.9), 0, g.note, g.slide);

        if (role.dropout) {
            b.clear(I::Kick, 8, 15);
            b.clear(I::Bass, 8, 15);
        } else if (role.phrase_end) {
            b.hit(I::HatClosed, 12, vel(rng, 0.6), 3);
            b.hit(I::HatClosed, 13, vel(rng, 0.65), 3);
            b.hit(I::Snare, 14, vel(rng, 0.55), 3);
            b.hit(I::Snare, 15, vel(rng, 0.75), 3);
        }
    }
}

// ── Funk carioca (tamborzão) ────────────────────────────────────────────────
// The engine of a Rio baile: kick and low tom answering each other on the 3-3-2
// tresillo (steps 0-3-6 | 8-11-14), claps on 2 and 4, a shaker accenting the same
// 3-3-2 cells, and a huge 808 under the kicks. Toms roll down into each phrase.

void groove_funk(Pattern& p, Rng& rng) {
    const bool kick_on_six = rng.chance(0.35);
    const bool kick_on_fourteen = rng.chance(0.3);
    const bool hats = rng.chance(0.5);
    const bool rims = rng.chance(0.4);
    const bool stabs = rng.chance(0.6);
    constexpr std::array<int, 6> kTresillo = {0, 3, 6, 8, 11, 14};
    const auto on_tresillo = [&](int s) {
        return std::find(kTresillo.begin(), kTresillo.end(), s) != kTresillo.end();
    };

    for (int bar = 0; bar < p.bars(); ++bar) {
        const BarRole role = role_of(bar, p.bars(), rng, 0.35);
        Bar b(p, bar);
        for (int s = 0; s < 16; ++s)
            b.hit(I::Shaker, s, vel(rng, on_tresillo(s) ? 0.7 : 0.35));
        b.hit(I::Clap, 4, vel(rng, 0.9));
        b.hit(I::Clap, 12, vel(rng, 0.95, 0.03));
        if (rng.chance(0.4)) b.hit(I::Clap, rng.chance(0.5) ? 7 : 15, vel(rng, 0.5));
        if (hats)
            for (int s : {2, 6, 10, 14})
                b.hit(I::HatClosed, s, vel(rng, 0.5));
        if (role.intro) continue;

        if (stabs && rng.chance(0.6)) b.hit(I::Keys, 0, vel(rng, 0.6));
        for (int s : {0, 8}) {
            b.hit(I::Kick, s, vel(rng, s == 0 ? 1.0 : 0.92, 0.04));
            b.hit(I::Bass, s, vel(rng, 0.9));
        }
        for (int s : {3, 6, 11, 14}) {
            if ((s == 6 && kick_on_six) || (s == 14 && kick_on_fourteen)) {
                b.hit(I::Kick, s, vel(rng, 0.8));
                b.hit(I::Bass, s, vel(rng, 0.8));
            } else {
                b.hit(I::Tom, s, vel(rng, s == 14 ? 0.75 : 0.85), 0, s == 14 ? 5 : 0);
            }
        }
        if (rng.chance(0.25) && !b.has(I::Bass, 14)) b.hit(I::Bass, 15, vel(rng, 0.8), 0, 12, true);
        if (rims) {
            b.hit(I::Rim, 5, vel(rng, 0.5));
            b.hit(I::Rim, 13, vel(rng, 0.45));
        }

        if (role.dropout) {
            for (I low : {I::Kick, I::Tom, I::Bass})
                b.clear(low, 8, 15);
        } else if (role.phrase_end) {
            for (I low : {I::Kick, I::Tom, I::Bass})
                b.clear(low, 12, 15);
            constexpr std::array<int, 4> kRollDown = {7, 5, 2, 0};
            for (int s = 12; s < 16; ++s)
                b.hit(I::Tom, s, vel(rng, 0.7 + 0.07 * (s - 12)), 0,
                      kRollDown[static_cast<std::size_t>(s - 12)]);
        }
    }
}

} // namespace street::beats::detail
