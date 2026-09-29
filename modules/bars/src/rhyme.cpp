#include "street/bars/rhyme.hpp"

#include "street/bars/tokenizer.hpp"

namespace street::bars {

std::string_view rhyme_kind_name(RhymeKind kind) noexcept {
    switch (kind) {
    case RhymeKind::Perfect: return "perfect";
    case RhymeKind::Slant: return "slant";
    case RhymeKind::None: break;
    }
    return "none";
}

RhymeKind classify_rhyme(const Pronunciation& a, const Pronunciation& b) {
    if (a.nuclei.empty() || b.nuclei.empty()) return RhymeKind::None;
    if (a.rhyme_key() == b.rhyme_key()) return RhymeKind::Perfect;
    if (a.vowel_skeleton() == b.vowel_skeleton()) return RhymeKind::Slant;
    return RhymeKind::None;
}

RhymeKind classify_rhyme(std::string_view a, std::string_view b, Language lang) {
    if (normalize_word(a) == normalize_word(b)) return RhymeKind::None;
    if (lang == Language::Auto) lang = detect_language(std::string(a) + " " + std::string(b));
    return classify_rhyme(pronounce(a, lang), pronounce(b, lang));
}

bool similar_vowels(std::string_view a, std::string_view b, Language lang) noexcept {
    if (a == b) return true;
    if (a.size() != 1 || b.size() != 1) return false;
    const char x = a[0];
    const char y = b[0];
    const auto pair = [&](char p, char q) { return (x == p && y == q) || (x == q && y == p); };
    if (lang == Language::Portuguese) return pair('e', 'i') || pair('o', 'u');
    return pair('@', 'i') || pair('@', 'u') || pair('@', 'e');
}

int matching_tail(std::span<const std::string> a, std::span<const std::string> b,
                  Language lang) noexcept {
    int n = 0;
    std::size_t i = a.size();
    std::size_t j = b.size();
    while (i > 0 && j > 0 && similar_vowels(a[i - 1], b[j - 1], lang)) {
        --i;
        --j;
        ++n;
    }
    return n;
}

} // namespace street::bars
