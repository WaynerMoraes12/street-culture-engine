#pragma once
// street::bars — lyric & flow analyzer for English and Brazilian Portuguese.
//
// Umbrella header: include this to get the whole public API.
//
//   auto a = street::bars::analyze(text);              // syllables, rhyme scheme, density...
//   std::cout << street::bars::render(a);             // colours when the terminal has them
//   auto words = street::bars::rhymes_for("game", street::bars::Language::English);
//   street::bars::VerseRequest request;  request.subject = "Jay";  request.seed = 7;
//   auto verse = street::bars::ghostwrite(request);
//
// Pipeline: tokenizer -> phonetics (syllables, stress, rhyme keys) -> rhyme linking and
// families -> scheme and metrics -> render. Each stage has its own header.

#include "street/bars/analysis.hpp"    // Word, Line, Analysis, analyze()
#include "street/bars/ghostwriter.hpp" // VerseRequest, ghostwrite(), sample_verse()
#include "street/bars/language.hpp"    // Language, detect_language()
#include "street/bars/lexicon.hpp"     // rhymes_for(), find_rhymes()
#include "street/bars/phonetics.hpp"   // count_syllables(), rhyme_key(), pronounce()
#include "street/bars/render.hpp"      // RenderOptions, render(), render_rhymes()
#include "street/bars/rhyme.hpp"       // RhymeKind, classify_rhyme()
#include "street/bars/tokenizer.hpp"   // tokenize(), split_words()
