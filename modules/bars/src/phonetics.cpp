#include "street/bars/phonetics.hpp"

#include "phonetics_detail.hpp"
#include "street/bars/tokenizer.hpp"
#include "street/core/text.hpp"

namespace street::bars {
namespace {

constexpr std::string_view kEnglishVowels = "aAeEiIoOQuUVWYR@";
constexpr std::string_view kPortugueseVowels = "aeiouAEIOU";

bool is_digit(char c) { return c >= '0' && c <= '9'; }

/// Appends `part` to `whole`; the stress of a compound is the stress of its last part
/// that has any vowel ("hip-HOP", "twenty-THREE").
void append_part(Pronunciation& whole, const Pronunciation& part) {
    if (part.nuclei.empty()) {
        whole.phonemes += part.phonemes;
        return;
    }
    const std::size_t offset = whole.phonemes.size();
    const int base = static_cast<int>(whole.nuclei.size());
    whole.phonemes += part.phonemes;
    for (const Nucleus& n : part.nuclei) whole.nuclei.push_back({n.begin + offset, n.length});
    whole.stress = base + part.stress;
}

Pronunciation pronounce_simple(std::string_view piece, Language lang) {
    return lang == Language::Portuguese ? detail::pronounce_portuguese(piece)
                                        : detail::pronounce_english(piece);
}

} // namespace

bool is_vowel_phoneme(char c, Language lang) noexcept {
    const std::string_view set = lang == Language::Portuguese ? kPortugueseVowels : kEnglishVowels;
    return c != '\0' && set.find(c) != std::string_view::npos;
}

std::string skeleton_of(std::string_view key, Language lang) {
    std::string out;
    for (std::size_t i = 0; i < key.size(); ++i) {
        const char c = key[i];
        if (is_vowel_phoneme(c, lang)) {
            out += c;
        } else if (lang == Language::Portuguese && (c == 'y' || c == 'w') && i > 0 &&
                   is_vowel_phoneme(key[i - 1], lang) &&
                   !(i + 1 < key.size() && is_vowel_phoneme(key[i + 1], lang))) {
            out += c; // the glide belongs to the diphthong: "pai" -> "ay"
        }
    }
    return out;
}

std::string Pronunciation::rhyme_key() const {
    if (nuclei.empty()) return {};
    const auto s = static_cast<std::size_t>(stress < 0 ? 0 : stress);
    std::string key = phonemes.substr(nuclei[s < nuclei.size() ? s : nuclei.size() - 1].begin);
    if (language != Language::Portuguese) {
        // An r-coloured schwa right after a vowel is heard as a plain r: fire ~ higher,
        // hour ~ power, so both spellings share one key.
        for (std::size_t i = 1; i < key.size(); ++i)
            if (key[i] == 'R' && is_vowel_phoneme(key[i - 1], language)) key[i] = 'r';
    }
    return key;
}

std::string Pronunciation::vowel_skeleton() const { return skeleton_of(rhyme_key(), language); }

std::vector<std::string> Pronunciation::vowels() const {
    std::vector<std::string> out;
    out.reserve(nuclei.size());
    for (const Nucleus& n : nuclei) out.push_back(phonemes.substr(n.begin, n.length));
    return out;
}

Pronunciation pronounce(std::string_view word, Language lang) {
    if (lang == Language::Auto) lang = detect_language(word);
    const std::string lower = normalize_word(word);
    const std::string folded = lang == Language::English ? text::fold_accents(lower) : lower;

    Pronunciation whole;
    whole.language = lang;
    for (const std::string& piece : text::split(folded, '-', true)) {
        // Runs of digits are read as numbers; letters around them are pronounced as usual.
        std::size_t i = 0;
        while (i < piece.size()) {
            std::size_t j = i;
            const bool digits = is_digit(piece[i]);
            while (j < piece.size() && is_digit(piece[j]) == digits) ++j;
            const std::string_view run = std::string_view(piece).substr(i, j - i);
            if (digits) {
                for (const std::string& w : text::split(detail::spell_number(run, lang), ' ', true))
                    append_part(whole, pronounce_simple(w, lang));
            } else if (run != "'") {
                append_part(whole, pronounce_simple(run, lang));
            }
            i = j;
        }
    }
    if (whole.stress < 0) whole.stress = 0;
    return whole;
}

int count_syllables(std::string_view word, Language lang) { return pronounce(word, lang).syllables(); }

std::string rhyme_key(std::string_view word, Language lang) { return pronounce(word, lang).rhyme_key(); }

std::string sound_label(std::string_view skeleton, Language lang) {
    std::string out;
    const auto sep = [&] {
        if (!out.empty()) out += '-';
    };
    if (lang == Language::Portuguese) {
        for (std::size_t i = 0; i < skeleton.size(); ++i) {
            const char c = skeleton[i];
            const char glide = i + 1 < skeleton.size() && (skeleton[i + 1] == 'y' || skeleton[i + 1] == 'w')
                                   ? skeleton[i + 1]
                                   : '\0';
            if (c == 'y' || c == 'w') continue; // consumed with its vowel
            sep();
            switch (c) {
            case 'A': out += glide == 'w' ? "ão" : glide == 'y' ? "ãe" : "an"; break;
            case 'O': out += glide == 'y' ? "õe" : "on"; break;
            case 'E': out += glide == 'y' ? "em" : "en"; break;
            case 'I': out += "in"; break;
            case 'U': out += "un"; break;
            default:
                out += c;
                if (glide) out += glide == 'y' ? 'i' : 'u';
                break;
            }
        }
        return out;
    }
    for (const char c : skeleton) {
        sep();
        switch (c) {
        case 'A': out += "ay"; break;
        case 'E': out += "ee"; break;
        case 'I': out += "igh"; break;
        case 'O': out += "oh"; break;
        case 'U': out += "oo"; break;
        case 'V': out += "oo"; break;
        case 'W': out += "ow"; break;
        case 'Y': out += "oy"; break;
        case 'Q': out += "aw"; break;
        case 'R': out += "er"; break;
        case '@': out += "uh"; break;
        case 'o': out += "ah"; break;
        case 'u': out += "uh"; break;
        default: out += c; break;
        }
    }
    return out;
}

namespace detail {

Pronunciation from_marked_phonemes(std::string_view marked, Language lang) {
    Pronunciation p;
    p.language = lang;
    p.stress = -1;
    bool mark = false;
    for (const char c : marked) {
        if (c == '\'') {
            mark = true;
            continue;
        }
        if (is_vowel_phoneme(c, lang)) {
            p.nuclei.push_back({p.phonemes.size(), 1});
            if (mark) p.stress = static_cast<int>(p.nuclei.size()) - 1;
            mark = false;
        }
        p.phonemes += c;
    }
    return p;
}

} // namespace detail

} // namespace street::bars
