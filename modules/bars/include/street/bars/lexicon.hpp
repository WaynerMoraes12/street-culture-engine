#pragma once
// Built-in thematic rhyme lexicon (basketball, sneakers, streets, music, everyday words)
// and rhyme lookup against it.

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "street/bars/language.hpp"
#include "street/bars/rhyme.hpp"

namespace street::bars {

struct RhymeMatch {
    std::string word;
    RhymeKind kind = RhymeKind::None;
    int syllables_matched = 0; ///< trailing syllables whose vowels line up with the query
};

/// The lexicon for one language (Auto means English). Several hundred words, no duplicates.
std::span<const std::string_view> lexicon(Language lang);

/// Rhymes for `word`, best first: perfect rhymes before slant ones; within a kind, longer
/// multisyllabic matches, then matching final consonant class, then closer syllable
/// count, then alphabetical. The query itself is never returned. Auto detects the
/// language from the word's spelling.
std::vector<RhymeMatch> find_rhymes(std::string_view word, Language lang, std::size_t limit = 12);

/// Just the words of find_rhymes().
std::vector<std::string> rhymes_for(std::string_view word, Language lang, std::size_t limit = 12);

} // namespace street::bars
