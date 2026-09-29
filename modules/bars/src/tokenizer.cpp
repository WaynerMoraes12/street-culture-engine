#include "street/bars/tokenizer.hpp"


#include "street/core/text.hpp"

namespace street::bars {
namespace {

bool is_ascii_letter(char32_t c) { return (c >= U'a' && c <= U'z') || (c >= U'A' && c <= U'Z'); }
bool is_digit(char32_t c) { return c >= U'0' && c <= U'9'; }
bool is_apostrophe(char32_t c) { return c == U'\'' || c == U'’' || c == U'‘'; }

/// Letters beyond ASCII: Latin-1 and Latin Extended-A/B (accented Portuguese letters and
/// friends). The multiplication and division signs are the only non-letters in that range.
bool is_letter(char32_t c) {
    if (is_ascii_letter(c)) return true;
    if (c >= 0xC0 && c <= 0x24F) return c != 0xD7 && c != 0xF7;
    return false;
}

bool is_word_char(char32_t c) { return is_letter(c) || is_digit(c); }

/// Leading-apostrophe elisions common in lyrics ('cause, 'em, 'til ...).
constexpr std::string_view kElisions[] = {"cause", "em", "til", "bout", "round",
                                          "fore", "nuff", "kay", "sup"};

bool is_elision(std::string_view word) {
    const std::string lower = text::to_lower(word);
    for (std::string_view e : kElisions)
        if (lower == e) return true;
    return false;
}

struct Cp {
    char32_t value;
    std::size_t begin; ///< byte offset
    std::size_t end;
};

/// Decodes UTF-8 keeping byte offsets, so tokens can point back into the line.
std::vector<Cp> decode_with_offsets(std::string_view s) {
    std::vector<Cp> out;
    std::size_t i = 0;
    while (i < s.size()) {
        const auto b0 = static_cast<unsigned char>(s[i]);
        std::size_t len = 1; // ASCII, or an invalid lead byte decoded on its own
        if ((b0 >> 5) == 0x6) len = 2;
        else if ((b0 >> 4) == 0xE) len = 3;
        else if ((b0 >> 3) == 0x1E) len = 4;
        const std::size_t end = i + len <= s.size() ? i + len : s.size();
        const std::u32string cp = text::utf8_decode(s.substr(i, end - i));
        out.push_back({cp.empty() ? U'�' : cp.front(), i, end});
        i = end;
    }
    return out;
}

bool is_header(std::string_view line) {
    if (line.size() < 2) return false;
    return (line.front() == '[' && line.back() == ']') || (line.front() == '(' && line.back() == ')');
}

} // namespace

std::vector<Token> split_words(std::string_view line) {
    const std::vector<Cp> cps = decode_with_offsets(line);
    std::vector<Token> tokens;
    std::size_t i = 0;
    const std::size_t n = cps.size();
    while (i < n) {
        if (!is_word_char(cps[i].value)) {
            ++i;
            continue;
        }
        // Optional leading elision apostrophe: 'cause, 'em.
        std::size_t start = i;
        const bool lead_apostrophe = i > 0 && is_apostrophe(cps[i - 1].value) &&
                                     (i < 2 || !is_word_char(cps[i - 2].value));

        std::size_t j = i;
        while (j < n) {
            if (is_word_char(cps[j].value)) {
                ++j;
            } else if ((is_apostrophe(cps[j].value) || cps[j].value == U'-') && j + 1 < n &&
                       is_word_char(cps[j + 1].value)) {
                j += 2; // inner apostrophe (don't) or hyphen (hip-hop)
            } else {
                break;
            }
        }
        std::size_t stop = j;
        // Closing apostrophe: g-dropping ("ballin'") and plural possessives ("players'").
        if (j < n && is_apostrophe(cps[j].value)) {
            const char32_t last = cps[j - 1].value;
            const bool after_in = j >= 2 && (last == U'n' || last == U'N') &&
                                  (cps[j - 2].value == U'i' || cps[j - 2].value == U'I');
            if (after_in || last == U's' || last == U'S') stop = j + 1;
        }

        std::string word;
        const std::size_t begin_byte = cps[start].begin;
        for (std::size_t k = start; k < stop; ++k) {
            if (is_apostrophe(cps[k].value))
                word += '\'';
            else
                text::utf8_append(word, cps[k].value);
        }
        std::size_t token_begin = begin_byte;
        if (lead_apostrophe && is_elision(word)) {
            word.insert(word.begin(), '\'');
            token_begin = cps[start - 1].begin;
        }
        tokens.push_back({std::move(word), token_begin, cps[stop - 1].end});
        i = stop;
    }
    return tokens;
}

std::vector<TokenLine> tokenize(std::string_view verse) {
    std::vector<TokenLine> lines;
    bool pending_break = false;
    for (const std::string& raw : text::split_lines(verse)) {
        const std::string_view trimmed = text::trim(raw);
        TokenLine line;
        line.text = std::string(trimmed);
        if (!trimmed.empty() && !is_header(trimmed)) line.tokens = split_words(line.text);

        if (is_header(trimmed)) {
            // A header is itself a stanza separator, so any pending blank line is absorbed.
            line.header = true;
            lines.push_back(std::move(line));
            pending_break = false;
            continue;
        }
        if (line.tokens.empty()) {
            // Blank lines and lines of pure punctuation separate stanzas.
            pending_break = !lines.empty();
            continue;
        }
        if (pending_break && !lines.back().header) lines.push_back(TokenLine{});
        pending_break = false;
        lines.push_back(std::move(line));
    }
    return lines;
}

std::string normalize_word(std::string_view word) {
    std::string out = text::to_lower(word);
    // Curly apostrophes are three bytes in UTF-8; fold them to ASCII.
    std::string result;
    result.reserve(out.size());
    for (std::size_t i = 0; i < out.size(); ++i) {
        if (i + 2 < out.size() && static_cast<unsigned char>(out[i]) == 0xE2 &&
            static_cast<unsigned char>(out[i + 1]) == 0x80 &&
            (static_cast<unsigned char>(out[i + 2]) == 0x98 ||
             static_cast<unsigned char>(out[i + 2]) == 0x99)) {
            result += '\'';
            i += 2;
        } else {
            result += out[i];
        }
    }
    return result;
}

} // namespace street::bars
