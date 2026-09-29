#pragma once
// Internal: context-sensitive letter-to-sound rules for English.

#include <string>
#include <string_view>

namespace street::bars::detail {

/// Applies the English letter-to-sound rule table to a lowercase a-z word and returns its
/// phoneme string (alphabet documented in street/bars/phonetics.hpp). An apostrophe in the
/// output marks a stressed vowel when a rule knows the stress (e.g. "-ique").
std::string apply_english_rules(std::string_view word);

} // namespace street::bars::detail
