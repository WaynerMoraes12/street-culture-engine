#pragma once
// Internal: the drum grammar of each style.
//
// Every groove follows the same recipe: draw a little "DNA" once per seed (kick
// placements, ghost-note spots, hat grammar, 808 gestures), then write the bars,
// alternating A/B variants with small seeded mutations so no two bars are
// identical. Bar 1 of a 4+ bar beat is an intro (drums held back), and every
// 4-bar phrase ends with a fill or a drop-out that sets up the next downbeat.
// The pattern arrives sized, with its chord cycle already chosen.

#include "street/beats/pattern.hpp"
#include "street/core/rng.hpp"

namespace street::beats::detail {

void groove_boombap(Pattern& pattern, Rng& rng);
void groove_trap(Pattern& pattern, Rng& rng);
void groove_lofi(Pattern& pattern, Rng& rng);
void groove_drill(Pattern& pattern, Rng& rng);
void groove_funk(Pattern& pattern, Rng& rng);
void groove_electro(Pattern& pattern, Rng& rng);
void groove_gallop(Pattern& pattern, Rng& rng);

} // namespace street::beats::detail
