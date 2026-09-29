#pragma once
// UTF-8 lyric tokenizer: verse -> lines -> words.
//
// Rules, chosen for how lyrics are actually typed:
//  * apostrophes inside or closing a word stay part of it ("don't", "ballin'",
//    "players'"); curly apostrophes (U+2018/U+2019) are normalised to ASCII;
//  * a leading apostrophe is kept only for common elisions ('cause, 'em, 'til);
//  * hyphens between letters join a compound ("hip-hop", "bem-vindo");
//  * digits form words too ("23", "50") — the phonetics layer spells them out;
//  * everything else (punctuation, quotes, dashes, emoji) separates words;
//  * blank lines are stanza breaks; runs of them collapse into one, and leading
//    or trailing blank lines are dropped;
//  * a line wrapped in brackets ("[Verse 1]", "(Chorus)") is a section header:
//    it separates stanzas and carries no words.

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace street::bars {

struct Token {
    std::string text;       ///< the word, apostrophes normalised to '
    std::size_t begin = 0;  ///< byte offset of the word in its line
    std::size_t end = 0;    ///< one past the last byte
};

struct TokenLine {
    std::string text;           ///< the line, trimmed; empty for a stanza break
    std::vector<Token> tokens;  ///< empty for stanza breaks and headers
    bool header = false;        ///< "[Hook]"-style annotation
    bool is_break() const noexcept { return tokens.empty(); }
};

/// Splits one line of text into words (see the rules above).
std::vector<Token> split_words(std::string_view line);

/// Splits a whole verse into lines of words; handles LF and CRLF input.
std::vector<TokenLine> tokenize(std::string_view verse);

/// Canonical form used to compare words: lowercase (Latin-1 aware), ASCII apostrophes.
std::string normalize_word(std::string_view word);

} // namespace street::bars
