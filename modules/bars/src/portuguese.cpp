// Brazilian Portuguese pronunciation.
//
// Portuguese spelling is close to phonemic, so a direct left-to-right transcriber with a
// handful of context rules does well:
//  * syllable nuclei are vowels; a following unaccented i/u glides into a falling
//    diphthong (pai, meu, sou, muito) unless the rules of hiatus apply (sa-ir, ju-iz,
//    ra-i-nha, sa-ú-de); rising sequences are counted apart, as grammars do (his-tó-ri-a);
//  * ão, ãe, õe are nasal diphthongs; a vowel before m/n + consonant is nasal (campo),
//    and final -am / -em / -ens become the nasal diphthongs they sound like (falam ≈ falão);
//  * the u of qu/gu is silent before e/i (quero, guerra) and a glide before a/o (quadra);
//  * c/ç/s/ss/sc all reduce to one /s/, intervocalic s is /z/, ch and x are /ʃ/, final z
//    is /s/, and syllable-final l vocalises to /w/ as in Brazil (Brasil rhymes with partiu).
// Stress follows the orthographic rules: an acute/circumflex accent wins, then a tilde;
// otherwise words ending in a, e, o (+s), am, em, ens are paroxytone, the rest oxytone.

#include <string>
#include <vector>

#include "phonetics_detail.hpp"
#include "street/core/text.hpp"

namespace street::bars::detail {
namespace {

struct Letter {
    char base = 0;            ///< a-z; 'c' also stands for ç (see cedilla)
    bool vowel = false;
    bool accent = false;      ///< acute or circumflex: marks the stressed syllable
    bool tilde = false;       ///< ã õ: nasal, and stressed unless another accent exists
    bool diaeresis = false;   ///< ü: the u of gü/qü is pronounced
    bool cedilla = false;
};

std::vector<Letter> spell(std::string_view word) {
    std::vector<Letter> letters;
    for (const char32_t c : text::utf8_decode(word)) {
        Letter l;
        switch (c) {
        case U'á': case U'â': l.base = 'a'; l.accent = true; break;
        case U'à': l.base = 'a'; break;
        case U'ã': l.base = 'a'; l.tilde = true; break;
        case U'é': case U'ê': l.base = 'e'; l.accent = true; break;
        case U'í': case U'î': l.base = 'i'; l.accent = true; break;
        case U'ó': case U'ô': l.base = 'o'; l.accent = true; break;
        case U'õ': l.base = 'o'; l.tilde = true; break;
        case U'ú': case U'û': l.base = 'u'; l.accent = true; break;
        case U'ü': l.base = 'u'; l.diaeresis = true; break;
        case U'ç': l.base = 'c'; l.cedilla = true; break;
        case U'y': l.base = 'i'; break;
        default:
            if (c >= U'a' && c <= U'z') l.base = static_cast<char>(c);
            break;
        }
        if (l.base == 0) continue; // apostrophes and other marks carry no sound
        l.vowel = l.base == 'a' || l.base == 'e' || l.base == 'i' || l.base == 'o' || l.base == 'u';
        letters.push_back(l);
    }
    return letters;
}

char nasal(char vowel) { return static_cast<char>(vowel - 'a' + 'A'); }

class Transcriber {
public:
    explicit Transcriber(std::vector<Letter> letters) : w_(std::move(letters)) {}

    Pronunciation run() {
        p_.language = Language::Portuguese;
        std::size_t i = 0;
        while (i < w_.size()) i = w_[i].vowel ? vowel(i) : consonant(i);
        p_.stress = stress();
        return std::move(p_);
    }

private:
    char base(std::size_t k) const { return k < w_.size() ? w_[k].base : '\0'; }
    bool is_vowel(std::size_t k) const { return k < w_.size() && w_[k].vowel; }
    bool is_consonant(std::size_t k) const { return k < w_.size() && !w_[k].vowel; }

    /// Does the unaccented i/u at k glide onto the vowel before it (falling diphthong)?
    bool glides(std::size_t k) const {
        if (k >= w_.size()) return false;
        const Letter& g = w_[k];
        if (!g.vowel || g.accent || g.tilde || (g.base != 'i' && g.base != 'u')) return false;
        if (g.base == w_[k - 1].base) return false; // ii, uu
        // In V+i+u / V+u+i the second pair wins: sa-iu, ca-iu (like par-tiu).
        if (k + 1 < w_.size() && w_[k + 1].vowel && !w_[k + 1].accent &&
            (base(k + 1) == 'i' || base(k + 1) == 'u') && base(k + 1) != g.base)
            return false;
        // Hiatus: i/u before nh (ra-i-nha), or closed by r, l, z, m, n inside the syllable
        // (sa-ir, ju-iz, ru-im, a-in-da, Ra-ul).
        const char next = base(k + 1);
        if (next == 'n' && base(k + 2) == 'h') return false;
        const bool closing = next == 'r' || next == 'l' || next == 'z' || next == 'm' || next == 'n';
        if (closing && (k + 2 >= w_.size() || (is_consonant(k + 2) && base(k + 2) != 'h')))
            return false;
        return true;
    }

    std::size_t vowel(std::size_t i) {
        const Letter& l = w_[i];
        const std::size_t start = p_.phonemes.size();
        std::string nucleus(1, l.tilde ? nasal(l.base) : l.base);
        std::size_t j = i + 1;
        if (l.tilde && (base(j) == 'o' || base(j) == 'e') && !w_[j].accent) {
            nucleus += base(j) == 'o' ? 'w' : 'y'; // ão, ãe, õe
            ++j;
        } else if (!l.tilde && glides(j)) {
            nucleus += base(j) == 'i' ? 'y' : 'w';
            ++j;
        }
        // Vowel + m/n closing the syllable is a nasal vowel (campo, mundo, também).
        const bool coda_nasal = (base(j) == 'm' || base(j) == 'n') &&
                                (j + 1 >= w_.size() || (is_consonant(j + 1) && base(j + 1) != 'h'));
        if (nucleus.size() == 1 && !l.tilde && coda_nasal) {
            const bool word_final = j + 1 == w_.size() || (j + 2 == w_.size() && base(j + 1) == 's');
            nucleus[0] = nasal(l.base);
            if (word_final && l.base == 'e') nucleus = "Ey";                     // bem, jovens
            else if (word_final && l.base == 'a' && base(j) == 'm') nucleus = "Aw"; // falam
            ++j;
        }
        p_.phonemes += nucleus;
        p_.nuclei.push_back({start, nucleus.size()});
        const int index = static_cast<int>(p_.nuclei.size()) - 1;
        if (l.accent) accented_ = index;
        if (l.tilde) tilde_ = index;
        return j;
    }

    std::size_t consonant(std::size_t i) {
        const char c = w_[i].base;
        const char next = base(i + 1);
        const bool front = next == 'e' || next == 'i';
        std::string& out = p_.phonemes;
        switch (c) {
        case 'c':
            if (w_[i].cedilla) break;
            if (next == 'h') {
                out += 'x';
                return i + 2;
            }
            out += front ? 's' : 'k';
            return i + 1;
        case 'q':
        case 'g': {
            const char hard = c == 'q' ? 'k' : 'g';
            if (next == 'u' && is_vowel(i + 2)) {
                const char after = base(i + 2);
                const bool silent_u = (after == 'e' || after == 'i') && !w_[i + 1].diaeresis;
                out += hard;
                if (!silent_u) out += 'w'; // quadra, água
                return i + 2;
            }
            if (c == 'g' && front) {
                out += 'j';
                return i + 1;
            }
            out += hard;
            return next == 'u' && c == 'q' ? i + 2 : i + 1;
        }
        case 'l':
            if (next == 'h') {
                out += 'L';
                return i + 2;
            }
            if (!is_vowel(i + 1)) {
                out += 'w'; // Brazilian vocalised l: Brasil, sol, futebol
                if (!p_.nuclei.empty()) {
                    Nucleus& last = p_.nuclei.back();
                    if (last.begin + last.length + 1 == out.size()) ++last.length;
                }
                return i + 1;
            }
            out += 'l';
            return i + 1;
        case 'n':
            if (next == 'h') {
                out += 'N';
                return i + 2;
            }
            out += 'n';
            return i + 1;
        case 'r': {
            const char prev = i > 0 ? w_[i - 1].base : '\0';
            if (next == 'r') {
                out += 'R';
                return i + 2;
            }
            out += (i == 0 || prev == 'n' || prev == 'l' || prev == 's') ? 'R' : 'r';
            return i + 1;
        }
        case 's':
            if (next == 's') {
                out += 's';
                return i + 2;
            }
            if (next == 'c' && (base(i + 2) == 'e' || base(i + 2) == 'i')) {
                out += 's'; // nascer, descer
                return i + 2;
            }
            out += (i > 0 && w_[i - 1].vowel && is_vowel(i + 1)) ? 'z' : 's';
            return i + 1;
        case 'x':
            if (next == 'c' && (base(i + 2) == 'e' || base(i + 2) == 'i')) {
                out += 's'; // exceção
                return i + 2;
            }
            out += (i == 1 && w_[0].base == 'e' && is_vowel(i + 1)) ? 'z' : 'x'; // exame
            return i + 1;
        case 'z':
            out += i + 1 == w_.size() ? 's' : 'z';
            return i + 1;
        case 'h':
            return i + 1; // silent
        default:
            out += c;
            return i + 1;
        }
        out += 's'; // ç
        return i + 1;
    }

    int stress() const {
        const int n = static_cast<int>(p_.nuclei.size());
        if (accented_ >= 0) return accented_;
        if (tilde_ >= 0) return tilde_;
        if (n <= 1) return 0;
        const std::size_t len = w_.size();
        const char last = base(len - 1);
        const char before = len >= 2 ? base(len - 2) : '\0';
        const char before2 = len >= 3 ? base(len - 3) : '\0';
        const auto oral = [](char v) { return v == 'a' || v == 'e' || v == 'o'; };
        const bool paroxytone = oral(last) || (last == 's' && oral(before)) ||
                                (last == 'm' && (before == 'a' || before == 'e')) ||
                                (last == 's' && before == 'n' && before2 == 'e');
        return paroxytone ? n - 2 : n - 1;
    }

    std::vector<Letter> w_;
    Pronunciation p_;
    int accented_ = -1;
    int tilde_ = -1;
};

} // namespace

Pronunciation pronounce_portuguese(std::string_view word) {
    return Transcriber(spell(text::to_lower(word))).run();
}

} // namespace street::bars::detail
