#pragma once
// Internal: shared vocabulary of the drum grammars (grooves.cpp, grooves_80s.cpp).

#include "street/beats/pattern.hpp"
#include "street/core/rng.hpp"

#include <algorithm>
#include <span>
#include <vector>

namespace street::beats::detail::groove {

using I = Instrument;

/// A candidate step and how idiomatic it is for the style.
struct Spot {
    int step;
    double weight;
};

/// Weighted draw of up to `count` distinct steps, returned in time order.
inline std::vector<int> pick_steps(Rng& rng, std::span<const Spot> spots, int count) {
    std::vector<Spot> pool(spots.begin(), spots.end());
    std::vector<int> picked;
    std::vector<double> weights;
    while (count-- > 0 && !pool.empty()) {
        weights.clear();
        for (const Spot& s : pool)
            weights.push_back(s.weight);
        const std::size_t i = rng.weighted(weights);
        picked.push_back(pool[i].step);
        pool.erase(pool.begin() + static_cast<std::ptrdiff_t>(i));
    }
    std::sort(picked.begin(), picked.end());
    return picked;
}

/// Moves one entry (never the downbeat) to another free candidate spot.
inline std::vector<int> mutate(std::vector<int> steps, Rng& rng, std::span<const Spot> spots) {
    std::vector<std::size_t> movable;
    for (std::size_t i = 0; i < steps.size(); ++i)
        if (steps[i] != 0) movable.push_back(i);
    std::vector<Spot> free;
    for (const Spot& s : spots)
        if (std::find(steps.begin(), steps.end(), s.step) == steps.end()) free.push_back(s);
    if (movable.empty() || free.empty()) return steps;
    steps[movable[rng.below(static_cast<std::uint32_t>(movable.size()))]] =
        pick_steps(rng, free, 1).front();
    std::sort(steps.begin(), steps.end());
    return steps;
}

inline std::vector<int> with_downbeat(std::vector<int> steps) {
    steps.insert(steps.begin(), 0);
    return steps;
}

inline float vel(Rng& rng, double base, double spread = 0.05) {
    return static_cast<float>(std::clamp(base + rng.uniform(-spread, spread), 0.08, 1.0));
}

struct BarRole {
    bool intro = false;      ///< first bar of a 4+ bar beat: drums held back
    bool phrase_end = false; ///< last bar of a 4-bar phrase (or of the beat)
    bool dropout = false;    ///< phrase end resolved by pulling the low end out instead of a fill
};

inline BarRole role_of(int bar, int bars, Rng& rng, double dropout_chance) {
    BarRole r;
    r.intro = bars >= 4 && bar == 0;
    r.phrase_end = bar % 4 == 3 || (bar == bars - 1 && bars > 1);
    r.dropout = r.phrase_end && bar != bars - 1 && rng.chance(dropout_chance);
    return r;
}

/// Write access to one bar of a pattern.
class Bar {
public:
    Bar(Pattern& pattern, int bar) : pattern_(pattern), bar_(bar) {}

    void hit(I instrument, int step, float velocity, int roll = 0, int note = 0,
             bool slide = false) {
        pattern_.at(instrument, bar_, step) =
            Step{velocity, static_cast<std::uint8_t>(roll), static_cast<std::int8_t>(note), slide};
    }
    /// A chord left ringing (guitar): see Step::ring.
    void ring(I instrument, int step, float velocity, int note = 0) {
        pattern_.at(instrument, bar_, step) =
            Step{velocity, 0, static_cast<std::int8_t>(note), false, true};
    }
    void clear(I instrument, int from, int to) {
        for (int s = from; s <= to; ++s)
            pattern_.at(instrument, bar_, s) = Step{};
    }
    bool has(I instrument, int step) const {
        const Track* t = pattern_.find(instrument);
        return t != nullptr &&
               t->steps[static_cast<std::size_t>(bar_ * Pattern::kStepsPerBar + step)].active();
    }

private:
    Pattern& pattern_;
    int bar_;
};

} // namespace street::beats::detail::groove
