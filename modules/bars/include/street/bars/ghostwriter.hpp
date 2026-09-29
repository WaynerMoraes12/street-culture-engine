#pragma once
// The ghostwriter: template + rhyme-bank verse generator (used by the culture layer to
// give the Player of the Week an anthem), and the original sample verses shipped with
// the tool.
//
// Each couplet picks one rhyme family (a bank of line endings whose last words rhyme
// perfectly under this module's own analyzer) and two different endings from it; the
// front half of each line is a template with {subject}, {team}, {shoe} and {points}
// slots. Families in one verse have different vowel skeletons, so couplets never blur
// into each other and the scheme reads AABB CCDD. Output depends only on the request.

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "street/bars/language.hpp"

namespace street::bars {

struct VerseRequest {
    std::string subject;   ///< who the verse is about; empty -> "the rookie" / "o novato"
    std::string team;      ///< empty -> a generic crew name
    std::string shoe;      ///< empty -> a generic sneaker
    int points = 0;        ///< <= 0 -> a seeded big night between 30 and 61
    Language language = Language::English; ///< Auto is treated as English
    std::uint64_t seed = 1;
    int lines = 4;         ///< clamped to 2..32; an odd count ends on a rhyming triplet
};

/// Writes a verse, one line per bar, with a blank line between 4-bar stanzas.
std::string ghostwrite(const VerseRequest& request);

/// The built-in verses, all original lyrics written for this project.
enum class Sample {
    English,    ///< "Hardwood Hymn": hoops, sneakers and hip-hop
    Portuguese, ///< "Coração de Quadra": the same themes in Brazilian Portuguese
    Tribute,    ///< "Hall of Fame": an English homage to the culture's legends, by name
};

/// Original 16-bar verse in `lang` about hoops, sneakers and hip-hop (Auto means English).
std::string_view sample_verse(Language lang);

/// Any built-in verse, including the tribute.
std::string_view sample_verse(Sample sample);

/// Parses "en", "pt" (or "english", "portuguese", "pt-br") and "tribute".
std::optional<Sample> parse_sample(std::string_view name);

/// Language a built-in verse is written in.
Language sample_language(Sample sample) noexcept;

} // namespace street::bars
