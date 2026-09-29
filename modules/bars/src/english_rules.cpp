// English letter-to-sound rules in the style of the NRL rule set (Elovitz et al., 1976).
//
// Each rule reads "left[match]right -> phonemes": when `match` occurs at the current
// position, the text before it fits `left` and the text after it fits `right`, emit the
// phonemes and move past the match. Rules for a letter are tried in order, specific
// before general, so every table ends with a context-free default.
//
// Context symbols:
//   ' '  word boundary          '#'  one or more vowels      ':'  zero or more consonants
//   '^'  exactly one consonant  '+'  front vowel (e, i, y)   '.'  voiced consonant
//   '&'  sibilant (s c g z x j ch sh)                        '!'  consonant or boundary
//   '%'  suffix that keeps a vowel long: e es ed er ers est ing ings ely eless eness
//        eful ement le les led (make, hater, riding, table)
// Anything else matches itself.

#include "english_rules.hpp"

#include <array>
#include <span>

namespace street::bars::detail {
namespace {

struct Rule {
    std::string_view left;
    std::string_view match;
    std::string_view right;
    std::string_view out;
};

// --- vowels ---------------------------------------------------------------------------

constexpr Rule kA[] = {
    {" ", "a", " ", "@"},
    {" ", "a", "wa", "@"}, // away, award
    {" ", "a", "wo", "@"}, // awoke
    {"", "augh", "", "Q"}, // caught, daughter
    {"", "aigh", "", "A"}, // straight
    {"", "air", "", "er"},
    {"", "ai", "", "A"},
    {"", "ay", "", "A"},
    {"", "au", "", "Q"},
    {"", "aw", "", "Q"},
    {"#:", "age", " ", "ij"}, // image, damage: unstressed
    {"#:", "ages", " ", "ijiz"},
    {"#:", "aged", " ", "ijd"},
    {"#:", "able", " ", "@b@l"},
    {"#:", "an", " ", "@n"}, // human, urban
    {"#:", "ans", " ", "@nz"},
    {"#:", "ance", " ", "@ns"},
    {"#:", "ant", " ", "@nt"},
    {"#:", "ants", " ", "@nts"},
    {"#:", "al", " ", "@l"}, // final, animal
    {"#:", "als", " ", "@lz"},
    {"#:", "ally", " ", "@li"},
    {"", "are", " ", "er"},  // care, square
    {"", "arr", "", "er"},   // carry, arrow (US merger)
    {"w", "ar", "", "Qr"},   // war, warm
    {"qu", "ar", "", "Qr"},  // quarter
    {"", "ar", "#", "er"},   // parent, vary, scary
    {"", "ar", "", "or"},    // car, star, hard
    {"", "alk", "", "Qk"},   // talk, walk
    {"", "alm", "", "om"},   // calm, palm
    {"", "alf", "", "af"},   // half
    {"", "all", "!", "Ql"},  // ball, basketball
    {"", "all", "%", "Ql"},  // baller, calling
    {"", "a", "lt", "Q"},    // salt
    {"", "a", "ld", "Q"},    // bald
    {" ", "a", "ls", "Q"},   // also
    {"f", "a", "ls", "Q"},   // false
    {"", "a", "lw", "Q"},    // always
    {" ", "a", "lr", "Q"},   // already
    {" ", "a", "lm", "Q"},   // almost
    {"", "a", "nge", "A"},   // change, danger
    {"", "a", "sic", "A"},   // basic
    {" :", "a", "^al ", "A"}, // fatal, naval
    {" :", "a", "^ent", "A"}, // agent, patent
    {"", "a", "ste ", "A"},  // taste, waste
    {"", "a", "sted ", "A"},
    {"", "a", "stes ", "A"},
    {"", "a", "^%", "A"},    // make, hater, fading
    {"", "a", "^i#", "A"},   // nation, radio
    {"", "a", "^y ", "A"},   // baby, crazy
    {"", "a", "^or", "A"},   // major, flavor
    {"", "a", "^ure", "A"},  // nature
    {"", "a", "^ous", "A"},  // famous
    {"w", "a", "sh", "o"},   // wash
    {"w", "a", "tch", "o"},  // watch
    {"w", "a", "nt", "o"},   // want
    {"w", "a", "nd", "o"},   // wander
    {"w", "a", "t", "o"},    // watt
    {"w", "a", "sp", "o"},   // wasp
    {"sw", "a", "p", "o"},   // swap
    {"sw", "a", "mp", "o"},  // swamp
    {"qu", "a", "", "o"},    // squad, quality
    {"#:", "a", " ", "@"},   // drama, arena
    {"", "a", " ", "o"},     // spa, ha
    {"", "a", "", "a"},
};

constexpr Rule kE[] = {
    {" ", "e", " ", "E"},
    {" :", "e", " ", "E"}, // be, he, she, we
    {"", "eigh", "", "A"}, // eight, weigh
    {"", "eau", "", "O"},
    {"", "ear", "n", "R"},  // earn, learn
    {"", "ear", "th", "R"}, // earth
    {"", "ear", "ch", "R"}, // search
    {"", "ear", "l", "R"},  // pearl, early
    {"h", "ear", "d", "R"}, // heard
    {"", "ear", "", "ir"},  // fear, clear, year
    {"", "eer", "", "ir"},
    {"", "ere", " ", "ir"}, // here, sincere
    {"", "ea", "d", "e"},   // head, ready, instead
    {"", "ea", "lth", "e"}, // health, wealth
    {"", "ea", "sure", "e"},
    {"", "ea", "ther", "e"},
    {"", "ea", "vy", "e"},
    {"", "ea", "ven", "e"},
    {"", "ea", "pon", "e"},
    {"", "ea", "nt", "e"},
    {"", "ea", "lous", "e"},
    {"", "ea", "sant", "e"},
    {"#:", "ea", " ", "E@"}, // idea, area
    {"", "ea", "", "E"},
    {"", "ee", "", "E"},
    {"", "e", "ing", "E"}, // being
    {"c", "ei", "", "E"}, // receive, ceiling
    {"", "ei", "", "A"},  // vein, reign
    {" :", "ey", " ", "A"}, // they, hey
    {"", "ey", " ", "i"},   // money, jersey
    {"", "ey", "s ", "i"},
    {"", "eu", "", "U"},
    {"", "ew", "", "U"},
    {"", "e", "o", "E"}, // video, neon
    {"#:t", "ed", " ", "id"},
    {"#:d", "ed", " ", "id"},
    {"#:p", "ed", " ", "t"},
    {"#:k", "ed", " ", "t"},
    {"#:f", "ed", " ", "t"},
    {"#:s", "ed", " ", "t"},
    {"#:x", "ed", " ", "t"},
    {"#:ch", "ed", " ", "t"},
    {"#:sh", "ed", " ", "t"},
    {"#:", "ed", " ", "d"},
    {"#:&", "es", " ", "iz"}, // places, changes, boxes
    {"#:p", "es", " ", "s"},
    {"#:k", "es", " ", "s"},
    {"#:t", "es", " ", "s"},
    {"#:f", "es", " ", "s"},
    {"#:", "es", " ", "z"}, // games
    {"#:", "est", " ", "ist"},
    {"#:", "et", " ", "it"}, // jacket, basket
    {"#:", "ets", " ", "its"},
    {"#:", "el", " ", "@l"}, // level, travel
    {"#:", "els", " ", "@lz"},
    {"#:", "ence", " ", "@ns"},
    {"#:", "ent", " ", "@nt"}, // moment, silent
    {"#:", "ents", " ", "@nts"},
    {"#:", "en", " ", "@n"}, // golden, seven
    {"#:", "ens", " ", "@nz"},
    {"#:", "en", "ed ", "@n"},
    {"#:", "en", "ing", "@n"},
    {" :", "e", "^al ", "E"},  // legal
    {" :", "e", "^ent", "E"},  // recent, decent
    {" ", "e", "^en", "E"},    // even, eden
    {"#:l", "e", "ment", "@"}, // element
    {"#:", "e", "ment", ""},   // movement
    {"#:", "e", "ly ", ""},    // lately
    {"#:", "e", "less", ""},   // homeless
    {"#:", "e", "ness", ""},
    {"#:", "e", "ful", ""},    // hopeful
    {"", "err", "", "er"},     // error, terror
    {"", "er", "#", "er"},     // very, merit
    {"", "er", "", "R"},       // her, person, sneaker
    {"#:", "e", " ", ""},      // silent final e
    {"", "e", "^%", "E"},      // these, complete, meter
    {"", "e", "", "e"},
};

constexpr Rule kI[] = {
    {" ", "i", " ", "I"},
    {"", "igh", "", "I"},
    {"", "iew", "", "U"}, // view
    {" :", "ies", " ", "Iz"}, // tries, flies
    {"", "ies", " ", "iz"},   // cities, parties
    {" :", "ied", " ", "Id"},
    {"", "ied", " ", "id"},
    {"", "ier", " ", "iR"},   // happier
    {"", "iers", " ", "iRz"},
    {"", "iest", " ", "iist"},
    {" :", "ie", " ", "I"},   // die, tie
    {"", "ie", " ", "i"},     // cookie, rookie
    {"sc", "ie", "", "I@"},   // science
    {"cl", "ie", "", "I@"},   // client
    {"qu", "ie", "", "I@"},   // quiet
    {"d", "ie", "t", "I@"},   // diet
    {"", "ie", "n", "i@"},    // audience, alien
    {"", "ie", "", "E"},      // field, believe
    {"ll", "ion", " ", "y@n"}, // million
    {"ll", "ions", " ", "y@nz"},
    {"n", "ion", " ", "y@n"},  // opinion, union
    {"n", "ions", " ", "y@nz"},
    // "fire" is one syllable in dictionaries; its key still matches "higher".
    {"", "ire", " ", "Ir"},
    {"", "ire", "d ", "Ir"},
    {"", "ire", "s ", "Ir"},
    {"", "ire", "ment", "Ir"},
    {"", "ire", "ly", "Ir"},
    {"", "ir", "ing", "Ir"},
    {"", "ir", "", "R"}, // bird, first
    {"", "i", "nd ", "I"}, // find, mind, grind
    {"", "i", "nds ", "I"},
    {"", "i", "nd%", "I"},
    {"", "i", "ld ", "I"}, // child, wild
    {"", "i", "gn ", "I"}, // sign, design
    {"", "i", "gns ", "I"},
    {"", "i", "gned ", "I"},
    {"#:", "ice", " ", "is"}, // notice, justice
    {"#:", "ive", " ", "iv"}, // active, massive
    {"#:", "ives", " ", "ivz"},
    {"#:^", "i", "^i ", "E"}, // bikini, zucchini
    {" :", "i", "^al ", "I"}, // final, vital, rival
    {" :", "i", "^ent", "I"}, // silent
    {"", "i", "que", "'E"},   // unique, technique
    {"", "ia", " ", "i@"},    // media, via
    {"", "i", "^%", "I"},     // bite, rider, shining
    {" :", "i", " ", "I"},    // hi
    {"", "i", "", "i"},
};

constexpr Rule kO[] = {
    {" ", "o", " ", "O"},
    {"", "ough", "t", "Q"}, // thought, brought
    {"", "ough", "", "O"},  // though, dough
    {"", "oar", "", "Qr"},
    {"", "oor", "", "Qr"},
    {"", "oa", "", "O"},
    {"", "oe", "s ", "O"},
    {"", "oe", " ", "O"},
    {"", "o", "ing", "O"}, // going
    {"", "oi", "", "Y"},
    {"", "oy", "", "Y"},
    {"", "oo", "k", "V"},  // book, look
    {"g", "oo", "d", "V"}, // good
    {"h", "oo", "d", "V"},
    {"w", "oo", "d", "V"},
    {"t", "oo", "d", "V"}, // stood
    {"f", "oo", "t", "V"}, // foot
    {"", "oo", "", "U"},
    {"", "ous", " ", "@s"}, // famous, nervous
    {"", "oul", "d", "V"},  // could, would
    {"", "our", "t", "Qr"}, // court
    {"", "our", "se", "Qr"}, // course
    {"", "our", "n", "Qr"},
    {"", "our", "", "Wr"},  // our, sour
    {"", "ou", "ble", "u"}, // double, trouble
    {"", "ou", "ple", "u"},
    {"", "ou", "ntry", "u"},
    {"", "ou", "ng", "u"},  // young
    {"", "ou", "sin", "u"},
    {"", "ou", "", "W"},    // out, sound, house
    // Final -ow is usually /oʊ/ (flow, show); the /aʊ/ words (now, how) are exceptions.
    {"", "ow", " ", "O"},
    {"", "ow", "s ", "O"},
    {"", "ow", "%", "O"},
    {"", "ow", "th", "O"},
    {"", "ow", "", "W"},    // down, crowd, owl
    {"w", "orr", "", "R"},  // worry
    {"w", "or", "!", "R"},  // work, world
    {"", "orr", "", "or"},  // sorry, tomorrow
    {"#:^", "or", " ", "R"}, // actor, motor: unstressed
    {"#:^", "ors", " ", "Rz"},
    {"", "or", "", "Qr"},
    {"#:", "on", " ", "@n"}, // person, reason
    {"#:", "ons", " ", "@nz"},
    {"", "o", "ther", "u"},  // other, mother
    {"", "o", "thing", "u"},
    {"", "olk", "", "Ok"},   // folk
    {"", "o", "ld", "O"},    // gold, cold
    {"", "o", "lt", "O"},
    {"", "o", "ll ", "O"},   // roll, troll
    {"", "o", "lls ", "O"},
    {"", "o", "lled ", "O"},
    {"", "o", "ller", "O"},
    {"", "o", "lling", "O"},
    {"", "o", "^%", "O"},    // home, over, lonely
    {"", "o", "^en", "O"},   // open, broken
    {"", "o", "^al ", "O"},  // total, vocal
    {"", "o", "^or", "O"},   // motor
    {"", "o", "^i#", "O"},   // motion, social
    {"", "o", "^us", "O"},   // focus, bonus
    {"", "o", "^ent", "O"},  // moment
    {"", "o", "cean", "O"},
    {"", "o", " ", "O"},     // go, pro, logo
    {"", "o", "", "o"},
};

constexpr Rule kU[] = {
    {"", "ue", " ", "U"}, // blue, true, value
    {"", "ues", " ", "Uz"},
    {"", "ued", " ", "Ud"},
    {"", "ui", "ld", "i"}, // build
    {"", "ui", "lt", "i"}, // built
    {"", "ui", "", "U"},   // suit, juice
    {"", "ure", " ", "Vr"}, // pure, cure
    {"", "urr", "", "R"},   // hurry
    {"", "ur", "#", "Vr"},  // during, jury
    {"", "ur", "", "R"},    // turn, hurt
    {"p", "u", "ll", "V"},
    {"f", "u", "ll", "V"},
    {"b", "u", "ll", "V"},
    {"p", "u", "sh", "V"},
    {"b", "u", "sh", "V"},
    {" p", "u", "t ", "V"},
    {"#:", "us", " ", "@s"}, // focus, status
    {"#:", "um", " ", "@m"}, // album, stadium
    {"#:", "ums", " ", "@mz"},
    {" ", "u", "ni", "U"},   // unique, universe
    {" ", "u", "su", "U"},   // usual
    {" :", "u", "^al ", "U"}, // brutal
    {"", "u", "^%", "U"},    // cute, ruler
    {"", "u", "^ure", "U"},  // future
    {"", "u", "^i#", "U"},   // union, studio
    {"", "u", "^id", "U"},   // stupid
    {"", "u", "^ent", "U"},  // student
    {"", "u", "^or", "U"},   // rumor
    {"", "u", "^ic", "U"},   // music
    {"", "u", "^an ", "U"},  // human
    {"", "u", "^y ", "U"},   // duty
    {"", "u", "", "u"},
};

constexpr Rule kY[] = {
    {" :^", "y", "%", "I"},  // flying, flyer
    {"#:", "y", "%", "i"},   // carrying, studying
    {"", "y", "#", "y"},     // yes, beyond: consonant before a vowel
    {" :^", "y", " ", "I"},  // my, fly, sky
    {"#:if", "y", " ", "I"}, // amplify, justify
    {"#:", "y", " ", "i"},   // happy, city
    {"", "y", "^%", "I"},    // type, style, rhyme
    {"", "y", "", "i"},      // gym, rhythm
};

// --- consonants -----------------------------------------------------------------------

constexpr Rule kB[] = {
    {"", "bb", "", "b"},  {"m", "b", " ", ""},   {"m", "b", "s ", ""}, {"m", "b", "ing", ""},
    {"m", "b", "ed ", ""}, {"u", "b", "t", ""},  {"e", "b", "t", ""},  {"", "b", "", "b"},
};

constexpr Rule kC[] = {
    {"", "ch", "r", "k"},  // chrome, christ
    {" ", "ch", "orus", "k"},
    {" ", "ch", "ord", "k"},
    {" ", "ch", "aos", "k"},
    {" ", "ch", "ara", "k"},
    {"e", "ch", "o", "k"}, // echo
    {"", "ch", "", "C"},
    {"", "cc", "+", "ks"}, // success
    {"", "cc", "", "k"},
    {"", "ck", "", "k"},
    {"", "cial", "", "S@l"},
    {"", "cian", "", "S@n"},
    {"", "cious", "", "S@s"},
    {"", "cient", "", "S@nt"},
    {"", "c", "+", "s"},
    {"", "c", "", "k"},
};

constexpr Rule kD[] = {{"", "dd", "", "d"}, {"", "dg", "", "j"}, {"", "d", "", "d"}};

constexpr Rule kF[] = {
    {"", "ff", "", "f"}, {"#:", "ful", " ", "f@l"}, {"#:", "fully", " ", "f@li"}, {"", "f", "", "f"}};

constexpr Rule kG[] = {
    {" ", "gh", "", "g"}, // ghost
    {"", "gh", "", ""},   // night, though
    {" ", "gn", "", "n"},
    {"", "gn", " ", "n"}, // sign, campaign
    {"", "gn", "s ", "n"},
    {"", "gn", "ed ", "n"},
    {"", "gn", "ing", "n"},
    {"", "gg", "", "g"},
    {"", "gue", " ", "g"}, // league, vague
    {"", "gue", "s ", "g"},
    {"n", "gu", "#", "gw"}, // language, penguin
    {"", "gu", "#", "g"},   // guess, guard
    {"", "g", "et", "g"},   // get, target, together
    {" ", "g", "ear", "g"},
    {" ", "g", "ee", "g"},
    {" ", "g", "i", "g"},   // girl, give, gift
    {"r", "g", "e", "j"},   // large, charger
    {"a", "g", "er", "j"},  // manager
    {"", "g", "er", "g"},   // tiger
    {"e", "g", "in", "g"},  // begin
    {"", "g", "+", "j"},    // age, gym, legend
    {"", "g", "", "g"},
};

constexpr Rule kH[] = {{"#", "h", " ", ""}, {"", "h", "#", "h"}, {"", "h", "", ""}};
constexpr Rule kJ[] = {{"", "j", "", "j"}};
constexpr Rule kK[] = {{" ", "kn", "", "n"}, {"", "k", "", "k"}};

constexpr Rule kL[] = {
    {"#:", "less", " ", "l@s"}, // homeless, careless
    {"#:", "ly", " ", "li"},
    {"^", "le", " ", "@l"}, // table, little
    {"^", "les", " ", "@lz"},
    {"^", "led", " ", "@ld"},
    {"^", "ler", "", "lR"},
    {"^", "ling", "", "liN"},
    {"", "ll", "", "l"},
    {"", "l", "", "l"},
};

constexpr Rule kM[] = {{"", "mm", "", "m"}, {"", "m", "", "m"}};

constexpr Rule kN[] = {
    {"#:", "ness", " ", "n@s"}, // darkness
    {"", "nn", "", "n"},
    {"m", "n", " ", ""},    // autumn, column
    {"", "ng", "e ", "nj"}, // change, orange
    {"", "ng", "es ", "nj"},
    {"", "ng", "ed ", "nj"},
    {"a", "ng", "er", "nj"}, // danger, stranger
    {"e", "ng", "er", "nj"}, // avenger
    {"", "ng", "%", "N"},    // singer, singing
    {"", "ng", "l", "Ng"},   // single, jungle
    {"", "ng", "r", "Ng"},   // hungry
    {"", "ng", "#", "Ng"},   // bingo
    {"", "ng", "", "N"},
    {"", "n", "k", "N"},     // think, bank
    {"", "n", "", "n"},
};

constexpr Rule kP[] = {{"", "ph", "", "f"},  {"", "pp", "", "p"},  {" ", "ps", "", "s"},
                       {" ", "pn", "", "n"}, {"", "p", "", "p"}};

constexpr Rule kQ[] = {
    {"", "que", " ", "k"}, {"", "que", "s ", "k"}, {"", "qu", "", "kw"}, {"", "q", "", "k"}};

constexpr Rule kR[] = {{" ", "rh", "", "r"}, {"", "rr", "", "r"}, {"", "r", "", "r"}};

constexpr Rule kS[] = {
    {"", "sch", "", "sk"},
    {"", "sh", "", "S"},
    {"", "ssion", "", "S@n"},
    {"#", "sion", "", "Z@n"}, // vision, decision
    {"", "sion", "", "S@n"},  // tension
    {"", "ssure", "", "SR"},
    {"#", "sure", "", "ZR"},  // measure, pleasure
    {"", "sure", "", "SVr"},  // sure
    {"", "stle", " ", "s@l"}, // hustle, castle
    {"", "stles", " ", "s@lz"},
    {"", "stler", "", "slR"},
    {"", "stling", "", "sliN"},
    {"", "sten", "", "s@n"},  // listen
    {"", "sc", "+", "s"},     // scene, science
    {"#", "sm", " ", "z@m"},  // racism, prism
    {"#", "sms", " ", "z@mz"},
    {"#:", "some", " ", "s@m"}, // awesome
    {"", "ss", "", "s"},
    {"a", "s", "e ", "s"},  // case, base
    {"ou", "s", "e", "s"},  // house
    {"oo", "s", "e", "s"},  // loose
    {"a", "s", "ic", "s"},  // basic
    {"u", "s", " ", "s"},   // bus, plus
    {"i", "s", " ", "s"},   // tennis
    {"a", "s", " ", "s"},   // gas, atlas
    {"#", "s", "#", "z"},   // music, reason, rose
    {"#", "s", " ", "z"},   // plays, zoos
    {".", "s", " ", "z"},   // kids, bars, dreams
    {"", "s", "", "s"},
};

constexpr Rule kT[] = {
    {"", "tch", "", "C"},
    {"", "thm", " ", "D@m"}, // rhythm
    {"", "th", "e ", "D"},   // breathe
    {"", "th", "er", "D"},   // mother, together
    {"", "th", "", "T"},
    {"s", "tion", "", "C@n"}, // question
    {"", "tion", "", "S@n"},
    {"", "tial", "", "S@l"},
    {"", "tious", "", "S@s"},
    {"", "tient", "", "S@nt"},
    {"", "tience", "", "S@ns"},
    {"", "tual", "", "CU@l"}, // actual
    {"", "ture", "", "CR"},   // nature, picture
    {"s", "t", "m", ""},      // christmas
    {"", "tt", "", "t"},
    {"", "t", "", "t"},
};

constexpr Rule kV[] = {{"", "v", "", "v"}};
constexpr Rule kW[] = {
    {" ", "wr", "", "r"}, {" ", "wh", "o", "h"}, {"", "wh", "", "w"}, {"", "w", "", "w"}};
constexpr Rule kX[] = {{" ", "x", "", "z"}, {"", "x", "", "ks"}};
constexpr Rule kZ[] = {{"", "zz", "", "z"}, {"", "z", "", "z"}};

constexpr std::array<std::span<const Rule>, 26> kRules = {
    kA, kB, kC, kD, kE, kF, kG, kH, kI, kJ, kK, kL, kM,
    kN, kO, kP, kQ, kR, kS, kT, kU, kV, kW, kX, kY, kZ,
};

// --- context matching -----------------------------------------------------------------

bool plain_vowel(char c) { return c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u'; }

/// 'y' is a vowel unless it starts the word or precedes another vowel (yes, beyond).
bool vowel_at(std::string_view s, std::size_t i) {
    const char c = s[i];
    if (plain_vowel(c)) return true;
    if (c != 'y') return false;
    if (i > 0 && s[i - 1] == ' ') return false;
    return !(i + 1 < s.size() && plain_vowel(s[i + 1]));
}

bool consonant_at(std::string_view s, std::size_t i) {
    const char c = s[i];
    return c >= 'a' && c <= 'z' && !vowel_at(s, i);
}

bool voiced(char c) {
    return c == 'b' || c == 'd' || c == 'g' || c == 'j' || c == 'l' || c == 'm' || c == 'n' ||
           c == 'r' || c == 'v' || c == 'w' || c == 'z';
}

bool front_vowel(char c) { return c == 'e' || c == 'i' || c == 'y'; }

bool single_sibilant(char c) {
    return c == 's' || c == 'c' || c == 'g' || c == 'z' || c == 'x' || c == 'j';
}

constexpr std::string_view kSuffixes[] = {"eless", "eness", "ement", "ings", "eful", "ing", "ers",
                                          "est",   "ely",   "les",   "led",  "es",   "ed",  "er",
                                          "le",    "e"};

/// Matches a right-hand context starting at position k of the padded word.
bool match_right(std::string_view pattern, std::string_view s, std::size_t k) {
    for (const char p : pattern) {
        switch (p) {
        case '#':
            if (k >= s.size() || !vowel_at(s, k)) return false;
            while (k < s.size() && vowel_at(s, k)) ++k;
            break;
        case ':':
            while (k < s.size() && consonant_at(s, k)) ++k;
            break;
        case '^':
            if (k >= s.size() || !consonant_at(s, k)) return false;
            ++k;
            break;
        case '+':
            if (k >= s.size() || !front_vowel(s[k])) return false;
            ++k;
            break;
        case '.':
            if (k >= s.size() || !voiced(s[k])) return false;
            ++k;
            break;
        case '&':
            if (k + 1 < s.size() && (s[k] == 'c' || s[k] == 's') && s[k + 1] == 'h') k += 2;
            else if (k < s.size() && single_sibilant(s[k])) ++k;
            else return false;
            break;
        case '!':
            if (k < s.size() && vowel_at(s, k)) return false;
            ++k;
            break;
        case '%': {
            bool found = false;
            for (std::string_view suffix : kSuffixes) {
                if (s.substr(k, suffix.size()) == suffix && k + suffix.size() < s.size() &&
                    s[k + suffix.size()] == ' ') {
                    k += suffix.size();
                    found = true;
                    break;
                }
            }
            if (!found) return false;
            break;
        }
        default:
            if (k >= s.size() || s[k] != p) return false;
            ++k;
            break;
        }
    }
    return true;
}

/// Matches a left-hand context ending just before position `end` (exclusive), read
/// right to left.
bool match_left(std::string_view pattern, std::string_view s, std::size_t end) {
    std::ptrdiff_t k = static_cast<std::ptrdiff_t>(end) - 1;
    const auto at = [&](std::ptrdiff_t i) { return static_cast<std::size_t>(i); };
    for (std::size_t pi = pattern.size(); pi-- > 0;) {
        const char p = pattern[pi];
        switch (p) {
        case '#':
            if (k < 0 || !vowel_at(s, at(k))) return false;
            while (k >= 0 && vowel_at(s, at(k))) --k;
            break;
        case ':':
            while (k >= 0 && consonant_at(s, at(k))) --k;
            break;
        case '^':
            if (k < 0 || !consonant_at(s, at(k))) return false;
            --k;
            break;
        case '+':
            if (k < 0 || !front_vowel(s[at(k)])) return false;
            --k;
            break;
        case '.':
            if (k < 0 || !voiced(s[at(k)])) return false;
            --k;
            break;
        case '&':
            if (k >= 1 && s[at(k)] == 'h' && (s[at(k - 1)] == 'c' || s[at(k - 1)] == 's')) k -= 2;
            else if (k >= 0 && single_sibilant(s[at(k)])) --k;
            else return false;
            break;
        case '!':
            if (k >= 0 && vowel_at(s, at(k))) return false;
            --k;
            break;
        default:
            if (k < 0 || s[at(k)] != p) return false;
            --k;
            break;
        }
    }
    return true;
}

} // namespace

std::string apply_english_rules(std::string_view word) {
    std::string padded;
    padded.reserve(word.size() + 2);
    padded += ' ';
    for (char c : word)
        if (c >= 'a' && c <= 'z') padded += c;
    padded += ' ';

    std::string out;
    std::size_t i = 1;
    while (i + 1 < padded.size()) {
        const char c = padded[i];
        bool matched = false;
        for (const Rule& rule : kRules[static_cast<std::size_t>(c - 'a')]) {
            if (padded.compare(i, rule.match.size(), rule.match) != 0) continue;
            if (!match_left(rule.left, padded, i)) continue;
            if (!match_right(rule.right, padded, i + rule.match.size())) continue;
            out += rule.out;
            i += rule.match.size();
            matched = true;
            break;
        }
        if (!matched) ++i; // unreachable: every table ends with a default rule
    }
    return out;
}

} // namespace street::bars::detail
