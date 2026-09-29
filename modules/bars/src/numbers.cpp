// Numbers in lyrics ("dropped 50", "23 on my back") are read aloud, so their syllables
// and rhymes come from the spelled-out words.

#include <string>

#include "phonetics_detail.hpp"

namespace street::bars::detail {
namespace {

constexpr std::string_view kEnUnits[] = {
    "zero",    "one",     "two",       "three",    "four",     "five",    "six",
    "seven",   "eight",   "nine",      "ten",      "eleven",   "twelve",  "thirteen",
    "fourteen", "fifteen", "sixteen",  "seventeen", "eighteen", "nineteen"};
constexpr std::string_view kEnTens[] = {"",      "",      "twenty",  "thirty", "forty",
                                        "fifty", "sixty", "seventy", "eighty", "ninety"};

constexpr std::string_view kPtUnits[] = {
    "zero",   "um",     "dois",     "três",      "quatro",  "cinco",    "seis",
    "sete",   "oito",   "nove",     "dez",       "onze",    "doze",     "treze",
    "catorze", "quinze", "dezesseis", "dezessete", "dezoito", "dezenove"};
constexpr std::string_view kPtTens[] = {"",          "",         "vinte",   "trinta",
                                        "quarenta",  "cinquenta", "sessenta", "setenta",
                                        "oitenta",   "noventa"};
constexpr std::string_view kPtHundreds[] = {"",           "cento",      "duzentos",
                                            "trezentos",  "quatrocentos", "quinhentos",
                                            "seiscentos", "setecentos", "oitocentos",
                                            "novecentos"};

void append(std::string& out, std::string_view word) {
    if (!out.empty()) out += ' ';
    out += word;
}

std::string english_below_1000(int n) {
    std::string out;
    if (n >= 100) {
        append(out, kEnUnits[n / 100]);
        append(out, "hundred");
        n %= 100;
    }
    if (n >= 20) {
        append(out, kEnTens[n / 10]);
        if (n % 10) append(out, kEnUnits[n % 10]);
    } else if (n > 0 || out.empty()) {
        append(out, kEnUnits[n]);
    }
    return out;
}

std::string portuguese_below_1000(int n) {
    if (n == 100) return "cem";
    std::string out;
    if (n >= 100) {
        append(out, kPtHundreds[n / 100]);
        n %= 100;
        if (n) append(out, "e");
    }
    if (n >= 20) {
        append(out, kPtTens[n / 10]);
        if (n % 10) {
            append(out, "e");
            append(out, kPtUnits[n % 10]);
        }
    } else if (n > 0 || out.empty()) {
        append(out, kPtUnits[n]);
    }
    return out;
}

} // namespace

std::string spell_number(std::string_view digits, Language lang) {
    const bool pt = lang == Language::Portuguese;
    if (digits.empty()) return {};
    if (digits.size() > 6) { // phone numbers, years written oddly: read digit by digit
        std::string out;
        for (char d : digits) append(out, pt ? kPtUnits[d - '0'] : kEnUnits[d - '0']);
        return out;
    }
    int value = 0;
    for (char d : digits) value = value * 10 + (d - '0');
    const auto below = [pt](int n) { return pt ? portuguese_below_1000(n) : english_below_1000(n); };
    if (value < 1000) return below(value);
    // English reads most four-digit years in pairs: 1996 -> nineteen ninety six.
    if (!pt && digits.size() == 4 && ((value >= 1100 && value < 2000) || value >= 2010) &&
        value % 100 != 0) {
        std::string out = below(value / 100);
        if (value % 100 < 10) append(out, "oh");
        append(out, below(value % 100));
        return out;
    }
    std::string out;
    const int thousands = value / 1000;
    const int rest = value % 1000;
    if (!(pt && thousands == 1)) append(out, below(thousands));
    append(out, pt ? "mil" : "thousand");
    if (rest) {
        if (pt && (rest < 100 || rest % 100 == 0)) append(out, "e");
        append(out, below(rest));
    }
    return out;
}

} // namespace street::bars::detail
