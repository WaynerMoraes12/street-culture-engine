#pragma once
// Internal to the bars module: per-language transcribers shared by phonetics.cpp.

#include <string>
#include <string_view>

#include "street/bars/phonetics.hpp"

namespace street::bars::detail {

/// Transcribes one lowercase English word (letters and apostrophes only).
Pronunciation pronounce_english(std::string_view word);

/// Transcribes one lowercase Portuguese word (letters only, accents allowed).
Pronunciation pronounce_portuguese(std::string_view word);

/// "23" -> "twenty three" / "vinte e três". Numbers above 999,999 are read digit by digit.
std::string spell_number(std::string_view digits, Language lang);

/// Builds a Pronunciation from a phoneme string where every vowel symbol is one syllable
/// and an apostrophe marks the stressed vowel. Without a mark the stress is -1, to be
/// filled in by the caller's stress heuristic.
Pronunciation from_marked_phonemes(std::string_view marked, Language lang);

} // namespace street::bars::detail
