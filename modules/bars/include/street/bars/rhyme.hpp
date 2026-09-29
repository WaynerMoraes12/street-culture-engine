#pragma once
// Comparing words as rhymes.
//
//   perfect  identical rhyme keys (same sounds from the stressed vowel on): game / claim
//   slant    same vowel skeleton, different consonants:                      game / rain
//   multisyllabic  the number of trailing syllables whose vowels line up, possibly
//                  across word boundaries:  "legendary" / "necessary" -> 4

#include <span>
#include <string>
#include <string_view>

#include "street/bars/phonetics.hpp"

namespace street::bars {

enum class RhymeKind { None, Slant, Perfect };

std::string_view rhyme_kind_name(RhymeKind kind) noexcept; ///< "none", "slant", "perfect"

/// Compares two pronunciations. A repeated word is not a rhyme; callers check identity.
RhymeKind classify_rhyme(const Pronunciation& a, const Pronunciation& b);

/// Compares two words; the same word (case-insensitive) is never a rhyme with itself.
RhymeKind classify_rhyme(std::string_view a, std::string_view b, Language lang);

/// Vowel sounds that count as a match inside a multisyllabic rhyme. Exact matches always
/// do; unstressed vowels are lenient (English schwa vs. /ɪ/ /ʌ/, Portuguese e~i and o~u,
/// the reductions Brazilian speech applies to unstressed syllables).
bool similar_vowels(std::string_view a, std::string_view b, Language lang) noexcept;

/// Number of trailing positions where the two vowel sequences match (see similar_vowels).
int matching_tail(std::span<const std::string> a, std::span<const std::string> b,
                  Language lang) noexcept;

} // namespace street::bars
