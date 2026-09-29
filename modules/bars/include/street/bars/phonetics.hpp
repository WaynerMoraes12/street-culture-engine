#pragma once
// Letter-to-sound heuristics: syllables, stress and rhyme keys for English and PT-BR.
//
// A word is transcribed into a compact ASCII phoneme string, one character per
// sound, plus the position of every syllable nucleus and the stressed one.
// Everything the analyzer needs is derived from that single representation:
//
//   syllables       = number of nuclei
//   rhyme key       = the phonemes from the stressed vowel to the end ("game" -> "Am")
//   vowel skeleton  = just the vowels of the rhyme key (slant rhymes compare these)
//   vowels()        = the vowel of every syllable (multisyllabic rhymes compare these)
//
// English alphabet — vowels: a (cat) A (day) e (bed) E (see) i (sit, happy) I (night)
//   o (hot, car) O (go) Q (law, court) u (cup) U (blue) V (book) W (now) Y (boy)
//   R (bird, butter) @ (schwa); consonants: b d f g h j (judge) k l m n p r s t v w y z
//   plus C (church) S (shoe) T (thin) D (this) N (sing) Z (vision).
// Portuguese alphabet — vowels: a e i o u, nasal A E I O U; glides y w (the second half
//   of a diphthong: "pai" -> "pay", "pão" -> "pAw"); consonants: b d f g (hard) j (gente,
//   janela) k l L (lh) m n N (nh) p r (caro) R (carro) s t v x (chave, xis) z.

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "street/bars/language.hpp"

namespace street::bars {

/// Where one syllable's vowel sits inside Pronunciation::phonemes.
struct Nucleus {
    std::size_t begin = 0;
    std::size_t length = 1; ///< > 1 only for Portuguese diphthongs ("ay", "Aw")
};

struct Pronunciation {
    Language language = Language::English;
    std::string phonemes;        ///< e.g. "nIt" for "night", "korasAw" for "coração"
    std::vector<Nucleus> nuclei; ///< one per syllable, in order
    int stress = 0;              ///< index into nuclei where the rhyme starts

    int syllables() const noexcept { return static_cast<int>(nuclei.size()); }
    /// Phonemes from the stressed vowel to the end, normalised so that sounds a listener
    /// hears as identical compare equal (e.g. "fire" and "higher" both give "Ir").
    std::string rhyme_key() const;
    /// Only the vowel sounds of the rhyme key: "sneakers" -> "Er". Two words with the same
    /// skeleton but different consonants are slant rhymes.
    std::string vowel_skeleton() const;
    /// The vowel sound of every syllable, in order ("crossover" -> o, O, R).
    std::vector<std::string> vowels() const;
};

/// Transcribes one word. Hyphenated compounds and numbers ("hip-hop", "23") are handled;
/// Language::Auto guesses the language from the word's spelling.
Pronunciation pronounce(std::string_view word, Language lang);

/// Syllable count of a word (0 for tokens without letters or digits).
int count_syllables(std::string_view word, Language lang);

/// Rhyme key of a word: equal keys mean a perfect rhyme ("game"/"claim" -> "Am").
std::string rhyme_key(std::string_view word, Language lang);

/// Vowel skeleton of an arbitrary rhyme key (see Pronunciation::vowel_skeleton).
std::string skeleton_of(std::string_view rhyme_key, Language lang);

/// Human-readable spelling of a vowel skeleton for legends: "Er" -> "ee-er" (English),
/// "Aw" -> "ão", "aa" -> "a-a" (Portuguese).
std::string sound_label(std::string_view skeleton, Language lang);

/// True if `c` is a vowel symbol of the phoneme alphabet of `lang`.
bool is_vowel_phoneme(char c, Language lang) noexcept;

} // namespace street::bars
