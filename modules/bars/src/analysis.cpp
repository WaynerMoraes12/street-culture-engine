#include "street/bars/analysis.hpp"

#include <algorithm>
#include <numeric>
#include <set>
#include <unordered_map>

#include "street/bars/phonetics.hpp"
#include "street/bars/rhyme.hpp"
#include "street/bars/tokenizer.hpp"

namespace street::bars {
namespace {

/// Union-find with path halving; the smaller index becomes the root so components are
/// naturally identified by their first member.
class DisjointSets {
public:
    explicit DisjointSets(std::size_t n) : parent_(n) {
        std::iota(parent_.begin(), parent_.end(), std::size_t{0});
    }
    std::size_t find(std::size_t x) {
        while (parent_[x] != x) {
            parent_[x] = parent_[parent_[x]];
            x = parent_[x];
        }
        return x;
    }
    void unite(std::size_t a, std::size_t b) {
        a = find(a);
        b = find(b);
        if (a != b) parent_[std::max(a, b)] = std::min(a, b);
    }

private:
    std::vector<std::size_t> parent_;
};

/// One word occurrence with everything the rhyme search needs.
struct Slot {
    std::size_t line = 0;   ///< index into Analysis::lines
    std::size_t word = 0;   ///< index into Line::words
    int bar = 0;            ///< index among lines that have words
    int stanza = 0;
    int first_syllable = 0; ///< offset of the word's first syllable within its line
    Pronunciation pron;
    std::string norm, key, skeleton;
    bool line_end = false;
    bool rhymable = false;
};

constexpr int kInternalWindow = 2; ///< bars within which internal rhymes are heard
constexpr int kSlantEndWindow = 4; ///< max bars between slant end rhymes (same stanza only)

char scheme_letter(int index) {
    if (index < 26) return static_cast<char>('A' + index);
    if (index < 52) return static_cast<char>('a' + index - 26);
    return '?';
}

int vowel_count(std::string_view skeleton, Language lang) {
    return static_cast<int>(std::count_if(skeleton.begin(), skeleton.end(),
                                          [lang](char c) { return is_vowel_phoneme(c, lang); }));
}

class Analyzer {
public:
    Analyzer(std::string_view verse, Language lang) {
        result_.language = resolve_language(lang, verse);
        read(verse);
    }

    Analysis run() {
        link_rhymes();
        build_families();
        build_scheme();
        finish_metrics();
        return std::move(result_);
    }

private:
    void read(std::string_view verse) {
        std::set<std::string> unique;
        int stanza = 0;
        int bar = 0;
        bool pending_stanza = false;
        for (const TokenLine& tl : tokenize(verse)) {
            Line line;
            line.text = tl.text;
            line.header = tl.header;
            line.stanza = stanza;
            const std::size_t line_index = result_.lines.size();
            vowels_.emplace_back();
            if (tl.is_break()) {
                pending_stanza = true;
                result_.lines.push_back(std::move(line));
                continue;
            }
            if (pending_stanza && bar > 0) ++stanza;
            pending_stanza = false;
            line.stanza = stanza;

            for (std::size_t w = 0; w < tl.tokens.size(); ++w) {
                const Token& token = tl.tokens[w];
                Slot slot;
                slot.pron = pronounce(token.text, result_.language);
                slot.norm = normalize_word(token.text);
                slot.key = slot.pron.rhyme_key();
                slot.skeleton = slot.pron.vowel_skeleton();
                slot.line = line_index;
                slot.word = w;
                slot.bar = bar;
                slot.stanza = stanza;
                slot.first_syllable = line.syllables;
                slot.line_end = w + 1 == tl.tokens.size();
                slot.rhymable = !slot.key.empty() &&
                                (slot.line_end || !is_function_word(slot.norm, result_.language));

                const std::vector<std::string> vowels = slot.pron.vowels();
                vowels_.back().insert(vowels_.back().end(), vowels.begin(), vowels.end());
                owners_.resize(line_index + 1);
                owners_[line_index].insert(owners_[line_index].end(), vowels.size(), slots_.size());

                Word word;
                word.text = token.text;
                word.syllables = slot.pron.syllables();
                word.key = slot.key;
                word.phonemes = slot.pron.phonemes;
                word.begin = token.begin;
                word.end = token.end;
                line.syllables += word.syllables;
                line.words.push_back(std::move(word));
                unique.insert(slot.norm);
                slots_.push_back(std::move(slot));
            }
            result_.total_words += static_cast<int>(line.words.size());
            result_.total_syllables += line.syllables;
            marked_.resize(line_index + 1);
            marked_[line_index].assign(static_cast<std::size_t>(line.syllables), 0);
            ++bar;
            result_.lines.push_back(std::move(line));
        }
        marked_.resize(result_.lines.size());
        owners_.resize(result_.lines.size());
        result_.bars = bar;
        result_.unique_words = static_cast<int>(unique.size());
    }

    bool linked(const Slot& a, const Slot& b, bool both_end) const {
        if (a.key == b.key) return true;
        if (a.skeleton != b.skeleton) return false;
        if (both_end) return a.stanza == b.stanza && b.bar - a.bar <= kSlantEndWindow;
        return vowel_count(a.skeleton, result_.language) >= 2;
    }

    void link_rhymes() {
        families_ = DisjointSets(slots_.size());
        schemes_ = DisjointSets(result_.lines.size());
        rhymed_.assign(slots_.size(), 0);
        for (std::size_t i = 0; i < slots_.size(); ++i) {
            const Slot& a = slots_[i];
            if (!a.rhymable) continue;
            for (std::size_t j = i + 1; j < slots_.size(); ++j) {
                const Slot& b = slots_[j];
                const bool both_end = a.line_end && b.line_end;
                if (!both_end && b.bar - a.bar > kInternalWindow) {
                    if (!a.line_end) break; // slots are in reading order: nothing closer left
                    continue;
                }
                if (!b.rhymable) continue;
                if (a.norm == b.norm) {
                    // Repeating a word is not a rhyme, but a repeated end word keeps its letter.
                    if (both_end) schemes_.unite(a.line, b.line);
                    continue;
                }
                if (!linked(a, b, both_end)) continue;
                families_.unite(i, j);
                rhymed_[i] = rhymed_[j] = 1;
                if (both_end) schemes_.unite(a.line, b.line);
                measure(i, j);
            }
        }
    }

    /// Counts matching vowels backwards from the end of both words, across word
    /// boundaries, and marks the rhymed syllables of both lines. The walk stops where both
    /// sides reach the same word again: "one two three four / one two three more" is a
    /// one-syllable rhyme plus repetition, not a four-syllable rhyme.
    void measure(std::size_t ia, std::size_t ib) {
        const Slot& a = slots_[ia];
        const Slot& b = slots_[ib];
        const auto end_of = [](const Slot& s) { return s.first_syllable + s.pron.syllables(); };
        const std::vector<std::string>& va = vowels_[a.line];
        const std::vector<std::string>& vb = vowels_[b.line];
        const int floor_b = a.line == b.line ? end_of(a) : 0; // don't let b run back into a
        int length = 0;
        for (int pa = end_of(a) - 1, pb = end_of(b) - 1; pa >= 0 && pb >= floor_b; --pa, --pb) {
            const auto ua = static_cast<std::size_t>(pa);
            const auto ub = static_cast<std::size_t>(pb);
            if (!similar_vowels(va[ua], vb[ub], result_.language)) break;
            const std::size_t wa = owners_[a.line][ua];
            const std::size_t wb = owners_[b.line][ub];
            if (wa != ia && wb != ib && slots_[wa].norm == slots_[wb].norm) break;
            ++length;
        }
        length = std::max(length, 1); // linked words rhyme even if their vowels differ in length

        mark(a, length);
        mark(b, length);
        if (a.line_end) raise(result_.lines[a.line].multisyllabic, length);
        if (b.line_end) raise(result_.lines[b.line].multisyllabic, length);
        if (length > result_.longest_multisyllabic) {
            result_.longest_multisyllabic = length;
            result_.longest_multisyllabic_words = {phrase(a, length), phrase(b, length)};
        }
    }

    static void raise(int& target, int value) { target = std::max(target, value); }

    void mark(const Slot& s, int length) {
        const int tail = s.pron.syllables() - s.pron.stress; // the word's own rhyme tail
        const int end = s.first_syllable + s.pron.syllables();
        const int start = std::max(0, end - std::max(length, tail));
        std::vector<char>& flags = marked_[s.line];
        for (int k = start; k < end; ++k) flags[static_cast<std::size_t>(k)] = 1;
    }

    /// The words that cover the last `length` syllables ending at slot `s`.
    std::string phrase(const Slot& s, int length) const {
        const Line& line = result_.lines[s.line];
        std::size_t first = s.word;
        int covered = line.words[first].syllables;
        while (covered < length && first > 0) covered += line.words[--first].syllables;
        const std::size_t begin = line.words[first].begin;
        return line.text.substr(begin, line.words[s.word].end - begin);
    }

    void build_families() {
        std::unordered_map<std::size_t, int> family_of_root;
        for (std::size_t i = 0; i < slots_.size(); ++i) {
            if (!rhymed_[i]) continue;
            const Slot& slot = slots_[i];
            const std::size_t root = families_.find(i);
            auto [it, inserted] =
                family_of_root.try_emplace(root, static_cast<int>(result_.families.size()));
            if (inserted) {
                RhymeFamily family;
                family.id = it->second;
                family.sound = sound_label(slot.skeleton, result_.language);
                result_.families.push_back(std::move(family));
            }
            RhymeFamily& family = result_.families[static_cast<std::size_t>(it->second)];
            ++family.occurrences;
            if (std::find(family.words.begin(), family.words.end(), slot.norm) == family.words.end())
                family.words.push_back(slot.norm);
            result_.lines[slot.line].words[slot.word].group = it->second;
            if (!slot.line_end) ++result_.internal_rhymes;
        }
        result_.rhyme_groups = static_cast<int>(result_.families.size());
    }

    void build_scheme() {
        std::unordered_map<std::size_t, int> letter_of_root;
        int previous_stanza = -1;
        for (std::size_t i = 0; i < result_.lines.size(); ++i) {
            Line& line = result_.lines[i];
            if (line.is_break()) continue;
            const std::size_t root = schemes_.find(i);
            const auto [it, inserted] =
                letter_of_root.try_emplace(root, static_cast<int>(letter_of_root.size()));
            line.scheme = scheme_letter(it->second);
            if (previous_stanza >= 0 && line.stanza != previous_stanza) result_.scheme += ' ';
            result_.scheme += line.scheme;
            previous_stanza = line.stanza;
        }
    }

    void finish_metrics() {
        for (const std::vector<char>& flags : marked_)
            result_.rhymed_syllables += static_cast<int>(std::count(flags.begin(), flags.end(), 1));
        if (result_.total_syllables > 0)
            result_.rhyme_density =
                static_cast<double>(result_.rhymed_syllables) / result_.total_syllables;
        if (result_.total_words > 0)
            result_.vocabulary_richness =
                static_cast<double>(result_.unique_words) / result_.total_words;
    }

    Analysis result_;
    std::vector<Slot> slots_;
    std::vector<std::vector<std::string>> vowels_; ///< vowel of every syllable, per line
    std::vector<std::vector<char>> marked_;        ///< rhymed flag per syllable, per line
    std::vector<std::vector<std::size_t>> owners_; ///< slot owning each syllable, per line
    std::vector<char> rhymed_;
    DisjointSets families_{0};
    DisjointSets schemes_{0};
};

} // namespace

double Analysis::syllables_per_bar() const noexcept {
    return bars > 0 ? static_cast<double>(total_syllables) / bars : 0.0;
}

double Analysis::syllables_per_second(int bpm) const {
    // One bar of 4/4 lasts four beats: 4 * 60 / bpm seconds.
    return bpm > 0 ? syllables_per_bar() * bpm / 240.0 : 0.0;
}

Analysis analyze(std::string_view verse, Language lang) { return Analyzer(verse, lang).run(); }

} // namespace street::bars
