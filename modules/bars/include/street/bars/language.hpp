#pragma once
// Language identification for lyrics: English vs Brazilian Portuguese.
//
// Lyrics are short, slangy and full of loanwords, so instead of a statistical
// model we score two kinds of cheap, robust cues: frequent function words
// ("the", "you" vs "que", "não") and orthography that only one language uses
// (ã, õ, ç, -ção, -nh- vs "th", "w", "-ing").

#include <optional>
#include <string_view>

namespace street::bars {

enum class Language { Auto, English, Portuguese };

/// Guesses the language of a verse (or of a single word). Never returns Auto;
/// ties and texts without any cue resolve to English.
Language detect_language(std::string_view text);

/// Returns `lang` unchanged unless it is Auto, in which case the text decides.
Language resolve_language(Language lang, std::string_view text);

std::string_view language_name(Language lang) noexcept; ///< "English", "Portuguese", "auto"
std::string_view language_code(Language lang) noexcept; ///< "en", "pt", "auto"

/// Parses "auto", "en", "english", "pt", "pt-br", "br", "portuguese", "português"
/// (ASCII case-insensitive). Returns nullopt for anything else.
std::optional<Language> parse_language(std::string_view name);

/// True for articles, prepositions, pronouns and auxiliaries ("the", "de", "you", "que").
/// These never count as internal rhymes; `word` must already be lowercase.
bool is_function_word(std::string_view word, Language lang);

} // namespace street::bars
