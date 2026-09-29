#pragma once
// Terminal renderers for the bars module. Pure functions returning strings: colourful
// when street::term colours are on, clean aligned plain text when they are off.

#include <cstddef>
#include <string>
#include <string_view>

#include "street/bars/analysis.hpp"
#include "street/bars/language.hpp"

namespace street::bars {

struct RenderOptions {
    int bpm = 90;              ///< tempo used for the flow-speed readout
    bool show_stats = true;    ///< stats panel and rhyme-family legend under the lyric
    int width = 100;           ///< total columns available (clamped to 60..160)
};

/// Numbered bars with scheme letter, syllable meter and rhymes highlighted per family,
/// followed (optionally) by a stats panel and a legend of rhyme families.
std::string render(const Analysis& analysis, const RenderOptions& options = {});

/// Flow label for a delivery speed: "laid-back" (< 3 syl/s), "steady" (< 5),
/// "rapid-fire" (< 7) or "chopper".
std::string_view flow_label(double syllables_per_second) noexcept;

/// Rhymes for `word` from the built-in lexicon, grouped into perfect and slant.
std::string render_rhymes(std::string_view word, Language lang, std::size_t limit = 24,
                          int width = 100);

} // namespace street::bars
