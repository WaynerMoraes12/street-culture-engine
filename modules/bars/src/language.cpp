#include "street/bars/language.hpp"

#include <string>

#include "street/bars/tokenizer.hpp"
#include "street/core/text.hpp"

namespace street::bars {
namespace {

// Function words that are frequent in lyrics and (nearly) exclusive to one language.
// Words shared by both ("a", "no", "do", "me", "se") are deliberately left out.
constexpr std::string_view kEnglishCues[] = {
    "the",   "and",  "you",  "your", "i'm",  "it's", "don't", "can't", "is",   "are",
    "was",   "were", "with", "this", "that", "what", "my",    "of",    "to",   "in",
    "on",    "for",  "it",   "be",   "got",  "get",  "just",  "like",  "all",  "when",
    "they",  "we",   "he",   "she",  "but",  "not",  "if",    "at",    "up",   "out",
    "how",   "who",  "i",    "an",   "from", "have", "never", "every", "ain't", "gonna",
    "yeah",  "his",  "her",  "them", "their", "i'll", "i've",  "won't"};

constexpr std::string_view kPortugueseCues[] = {
    "o",      "os",     "que",   "não",   "de",    "da",   "dos",   "das",  "um",
    "uma",    "eu",     "você",  "meu",   "minha", "na",   "nos",   "com",  "pra",
    "para",   "por",    "mais",  "mas",   "é",     "tá",   "tô",    "sou",  "vai",
    "tem",    "isso",   "esse",  "essa",  "ele",   "ela",  "nós",   "quando", "como",
    "porque", "também", "só",    "já",    "aqui",  "lá",   "bem",   "muito", "sem",
    "até",    "depois", "seu",   "sua",   "pro",   "num",  "numa",  "ao",   "aos",
    "pelo",   "pela",   "nem",   "ou",    "então", "vou",  "estou", "tão"};

template <std::size_t N>
bool contains(const std::string_view (&set)[N], std::string_view word) {
    for (std::string_view w : set)
        if (w == word) return true;
    return false;
}

bool ends_with(std::string_view s, std::string_view suffix) {
    return s.size() >= suffix.size() && s.substr(s.size() - suffix.size()) == suffix;
}

/// Orthographic evidence for Portuguese: accented vowels, cedilla, nasal tildes and the
/// digraphs nh/lh are all rare or impossible in English words.
int portuguese_spelling(std::string_view word) {
    int score = 0;
    for (std::string_view mark : {"ã", "õ", "ç", "á", "é", "í", "ó", "ú", "â", "ê", "ô", "à"})
        if (word.find(mark) != std::string_view::npos) score += 2;
    for (std::string_view tail : {"ção", "ções", "ão", "ões", "nho", "nha", "lho", "lha", "mente"})
        if (ends_with(word, tail)) score += 2;
    return score;
}

/// Orthographic evidence for English: "th", "w", "k", "y" and -ing / -'s are typical of
/// English and uncommon in native Portuguese words.
int english_spelling(std::string_view word) {
    int score = 0;
    if (word.find("th") != std::string_view::npos) score += 1;
    if (word.find('w') != std::string_view::npos) score += 1;
    if (word.find('y') != std::string_view::npos) score += 1;
    if (word.find('k') != std::string_view::npos) score += 1;
    if (ends_with(word, "ing") || ends_with(word, "in'")) score += 2;
    if (word.find('\'') != std::string_view::npos) score += 1;
    return score;
}

} // namespace

Language detect_language(std::string_view text) {
    int english = 0;
    int portuguese = 0;
    for (const Token& token : split_words(text)) {
        const std::string word = normalize_word(token.text);
        if (contains(kEnglishCues, word)) english += 3;
        if (contains(kPortugueseCues, word)) portuguese += 3;
        english += english_spelling(word);
        portuguese += portuguese_spelling(word);
    }
    return portuguese > english ? Language::Portuguese : Language::English;
}

Language resolve_language(Language lang, std::string_view text) {
    return lang == Language::Auto ? detect_language(text) : lang;
}

std::string_view language_name(Language lang) noexcept {
    switch (lang) {
    case Language::English: return "English";
    case Language::Portuguese: return "Portuguese";
    case Language::Auto: break;
    }
    return "auto";
}

std::string_view language_code(Language lang) noexcept {
    switch (lang) {
    case Language::English: return "en";
    case Language::Portuguese: return "pt";
    case Language::Auto: break;
    }
    return "auto";
}

std::optional<Language> parse_language(std::string_view name) {
    const std::string lower = text::to_lower(text::trim(name));
    if (lower == "auto") return Language::Auto;
    if (lower == "en" || lower == "eng" || lower == "english") return Language::English;
    if (lower == "pt" || lower == "pt-br" || lower == "br" || lower == "portuguese" ||
        lower == "português" || lower == "portugues")
        return Language::Portuguese;
    return std::nullopt;
}

bool is_function_word(std::string_view word, Language lang) {
    static constexpr std::string_view kEnglish[] = {
        "a",    "an",    "the",   "and",   "or",    "but",   "of",    "to",    "in",   "on",
        "at",   "by",    "for",   "with",  "from",  "as",    "is",    "am",    "are",  "was",
        "were", "be",    "been",  "it",    "its",   "it's",  "i",     "i'm",   "me",   "my",
        "you",  "your",  "he",    "him",   "his",   "she",   "her",   "we",    "us",   "our",
        "they", "them",  "their", "this",  "that",  "these", "those", "if",    "do",   "does",
        "did",  "not",   "than",  "then",  "into",  "onto",  "can",   "will",  "just", "got",
        "have", "has",   "had",   "an'"};
    static constexpr std::string_view kPortuguese[] = {
        "o",    "a",    "os",   "as",   "um",   "uma",   "uns",   "umas",  "de",   "do",
        "da",   "dos",  "das",  "em",   "no",   "na",    "nos",   "nas",   "e",    "é",
        "que",  "se",   "por",  "pra",  "pro",  "para",  "com",   "sem",   "eu",   "tu",
        "ele",  "ela",  "nós",  "eles", "elas", "me",    "te",    "lhe",   "meu",  "minha",
        "teu",  "tua",  "seu",  "sua",  "mas",  "ou",    "ao",    "aos",   "num",  "numa",
        "pelo", "pela", "isso", "isto", "esse", "essa",  "tá",    "vai"};
    if (lang == Language::Portuguese) return contains(kPortuguese, word);
    return contains(kEnglish, word);
}

} // namespace street::bars
