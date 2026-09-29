#pragma once
// Verse analysis: syllables per bar, rhyme families, end-rhyme scheme and flow metrics.
//
// How rhymes are found:
//  * every word is transcribed (see phonetics.hpp); function words ("the", "de") only
//    count when they end a line;
//  * two different words rhyme when their rhyme keys are equal (perfect) or their vowel
//    skeletons are equal (slant). End words may rhyme across the whole verse (perfect) or
//    within four bars of the same stanza (slant). Internal rhymes must be within two bars
//    of each other, and a slant internal rhyme needs at least two matching vowels to rise
//    above plain assonance;
//  * rhyme links are merged into families (connected components); each family gets a
//    colour in the renderer;
//  * for every link the vowels are also compared backwards across word boundaries: the
//    number of matching syllables is the rhyme's multisyllabic length.

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "street/bars/language.hpp"

namespace street::bars {

struct Word {
    std::string text;        ///< as written (curly apostrophes normalised)
    int syllables = 0;
    std::string key;         ///< rhyme key (see phonetics.hpp)
    int group = -1;          ///< rhyme family id (index into Analysis::families), -1 if none
    std::string phonemes;    ///< full transcription, handy for debugging and tests
    std::size_t begin = 0;   ///< byte range of the word inside Line::text
    std::size_t end = 0;
};

struct Line {
    std::string text;
    std::vector<Word> words;
    int syllables = 0;
    char scheme = ' ';       ///< end-rhyme letter; ' ' for stanza breaks and headers
    int stanza = 0;          ///< 0-based stanza index
    bool header = false;     ///< "[Chorus]"-style annotation (no words)
    int multisyllabic = 0;   ///< longest vowel-matched tail its last word shares with a rhyme
    bool is_break() const noexcept { return words.empty(); }
};

struct RhymeFamily {
    int id = 0;
    std::string sound;              ///< readable vowel skeleton, e.g. "ay" or "ee-er"
    std::vector<std::string> words; ///< distinct words (lowercase), in order of appearance
    int occurrences = 0;
};

struct Analysis {
    Language language = Language::English;
    std::vector<Line> lines;
    std::string scheme;             ///< e.g. "AABB CCDD" (space between stanzas)
    int total_words = 0;
    int unique_words = 0;
    int total_syllables = 0;
    int rhyme_groups = 0;           ///< number of rhyme families
    int longest_multisyllabic = 0;  ///< syllables in the longest rhyme (0 = no rhyme)
    double rhyme_density = 0.0;     ///< rhymed syllables / total syllables
    double vocabulary_richness = 0.0; ///< unique words / total words

    int bars = 0;                   ///< lines with words (one bar each)
    int rhymed_syllables = 0;
    int internal_rhymes = 0;        ///< rhyming words that do not end their line
    std::vector<RhymeFamily> families;
    std::array<std::string, 2> longest_multisyllabic_words; ///< the two phrases, if any

    double syllables_per_bar() const noexcept;
    /// Delivery speed assuming one line per 4/4 bar at `bpm` beats per minute.
    double syllables_per_second(int bpm) const;
};

/// Analyses a verse. Language::Auto detects English or Portuguese from the text.
Analysis analyze(std::string_view verse, Language lang = Language::Auto);

} // namespace street::bars
