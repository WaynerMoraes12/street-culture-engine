// English pronunciation: exceptions dictionary + contractions + compounds on top of the
// letter-to-sound rules, then a stress heuristic that finds where the rhyme starts.

#include <string>
#include <string_view>
#include <unordered_map>

#include "english_rules.hpp"
#include "phonetics_detail.hpp"

namespace street::bars::detail {
namespace {

// Irregular words the rules cannot know, biased towards lyric and hoops vocabulary.
// An apostrophe marks the stressed vowel where the default heuristic would miss it.
constexpr std::pair<std::string_view, std::string_view> kExceptions[] = {
    // function words and pronouns
    {"a", "@"}, {"the", "D@"}, {"of", "uv"}, {"to", "tU"}, {"do", "dU"}, {"who", "hU"},
    {"two", "tU"}, {"into", "intU"}, {"onto", "ontU"}, {"you", "yU"}, {"your", "yQr"},
    {"yours", "yQrz"}, {"you're", "yQr"}, {"we're", "wir"}, {"they're", "Der"}, {"are", "or"},
    {"were", "wR"}, {"where", "wer"}, {"there", "Der"}, {"their", "Der"}, {"this", "Dis"},
    {"that", "Dat"}, {"these", "DEz"}, {"those", "DOz"}, {"them", "Dem"}, {"then", "Den"},
    {"than", "Dan"}, {"they", "DA"}, {"though", "DO"}, {"thus", "Dus"}, {"with", "wiD"},
    {"was", "wuz"}, {"is", "iz"}, {"his", "hiz"}, {"has", "haz"}, {"as", "az"},
    {"does", "duz"}, {"says", "sez"}, {"said", "sed"}, {"what", "wut"}, {"i'm", "Im"},
    {"'em", "@m"}, {"'cause", "kuz"}, {"because", "b@'kuz"}, {"cause", "kQz"},
    {"yes", "yes"}, {"us", "us"}, {"bus", "bus"}, {"gas", "gas"}, {"plus", "plus"},
    {"yeah", "ye@"}, {"gonna", "gun@"}, {"wanna", "won@"}, {"gotta", "got@"},
    {"tryna", "trIn@"}, {"ain't", "Ant"}, {"can't", "kant"}, {"don't", "dOnt"},
    {"won't", "wOnt"}, {"aren't", "ornt"}, {"weren't", "wRnt"}, {"okay", "O'kA"},
    // irregular vowels
    {"have", "hav"}, {"give", "giv"}, {"live", "liv"}, {"gone", "gQn"}, {"done", "dun"},
    {"one", "wun"}, {"none", "nun"}, {"some", "sum"}, {"come", "kum"}, {"become", "b@'kum"},
    {"love", "luv"}, {"glove", "gluv"}, {"above", "@'buv"}, {"dove", "duv"}, {"shove", "Suv"},
    {"move", "mUv"}, {"prove", "prUv"}, {"lose", "lUz"}, {"whose", "hUz"}, {"choose", "CUz"},
    {"own", "On"}, {"known", "nOn"}, {"grown", "grOn"}, {"shown", "SOn"}, {"blown", "blOn"},
    {"flown", "flOn"}, {"thrown", "TrOn"}, {"unknown", "un'nOn"}, {"bowl", "bOl"},
    {"now", "nW"}, {"how", "hW"}, {"wow", "wW"}, {"cow", "kW"}, {"allow", "@'lW"},
    {"vow", "vW"}, {"brow", "brW"}, {"pow", "pW"}, {"power", "pWR"}, {"tower", "tWR"},
    {"flower", "flWR"}, {"shower", "SWR"}, {"again", "@'gen"}, {"against", "@'genst"},
    {"any", "eni"}, {"many", "meni"}, {"busy", "bizi"}, {"eye", "I"}, {"buy", "bI"},
    {"guy", "gI"}, {"bye", "bI"}, {"friend", "frend"}, {"been", "bin"}, {"sew", "sO"},
    {"sugar", "SVgR"}, {"heart", "hort"}, {"laugh", "laf"}, {"through", "TrU"},
    {"tough", "tuf"}, {"rough", "ruf"}, {"enough", "@'nuf"}, {"cough", "kQf"},
    {"drought", "drWt"}, {"touch", "tuC"}, {"youth", "yUT"}, {"group", "grUp"},
    {"soup", "sUp"}, {"route", "rUt"}, {"tour", "tVr"}, {"four", "fQr"}, {"pour", "pQr"},
    {"journey", "jRni"}, {"soul", "sOl"}, {"shoulder", "SOldR"}, {"blood", "blud"},
    {"flood", "flud"}, {"poor", "pVr"}, {"broad", "brQd"}, {"shoe", "SU"}, {"poem", "pO@m"},
    {"poet", "pO@t"}, {"choir", "kwIR"}, {"people", "pEp@l"}, {"woman", "wVm@n"},
    {"women", "wimin"}, {"water", "wQtR"}, {"father", "foDR"}, {"only", "Onli"},
    {"iron", "IRn"}, {"answer", "ansR"}, {"sword", "sQrd"}, {"honest", "onist"},
    {"hour", "Wr"}, {"honor", "onR"}, {"often", "Qf@n"}, {"island", "Il@nd"},
    {"climb", "klIm"}, {"tomb", "tUm"}, {"never", "nevR"}, {"ever", "evR"}, {"every", "evri"},
    {"clever", "klevR"}, {"river", "rivR"}, {"shiver", "SivR"}, {"deliver", "d@'livR"},
    {"cover", "kuvR"}, {"discover", "dis'kuvR"}, {"recover", "rE'kuvR"}, {"oven", "uv@n"},
    {"color", "kulR"}, {"honey", "huni"}, {"money", "muni"}, {"monkey", "muNki"},
    {"month", "munT"}, {"front", "frunt"}, {"wonder", "wundR"}, {"dozen", "duz@n"},
    {"son", "sun"}, {"ton", "tun"}, {"won", "wun"}, {"among", "@'muN"}, {"tongue", "tuN"},
    {"monday", "mundA"}, {"company", "kump@ni"}, {"comfort", "kumfRt"}, {"onion", "uny@n"},
    {"nothing", "nuTiN"}, {"bother", "boDR"}, {"dollar", "dolR"}, {"cheddar", "CedR"},
    {"regular", "regy@lR"}, {"popular", "popy@lR"}, {"mountain", "mWnt@n"},
    {"captain", "kapt@n"}, {"certain", "sRt@n"}, {"movie", "mUvi"}, {"homie", "hOmi"},
    {"machine", "m@'SEn"}, {"routine", "rU'tEn"}, {"police", "p@'lEs"}, {"engine", "enjin"},
    {"imagine", "i'majin"}, {"promise", "promis"}, {"favorite", "fAvrit"},
    {"alive", "@'lIv"}, {"arrive", "@'rIv"}, {"survive", "sR'vIv"}, {"advance", "@d'vans"},
    {"romance", "rO'mans"}, {"represent", "repr@'zent"}, {"event", "E'vent"},
    {"prevent", "prE'vent"}, {"invent", "in'vent"}, {"percent", "pR'sent"},
    {"content", "kontent"}, {"unless", "@n'les"}, {"upon", "@'pon"}, {"begin", "b@'gin"},
    {"began", "b@'gan"}, {"admit", "@d'mit"}, {"commit", "k@'mit"}, {"until", "@n'til"},
    {"assist", "@'sist"}, {"resist", "r@'zist"}, {"insist", "in'sist"}, {"exist", "ig'zist"},
    {"forget", "fR'get"}, {"regret", "r@'gret"}, {"upset", "up'set"}, {"hotel", "hO'tel"},
    {"beautiful", "byUtif@l"}, {"beauty", "byUti"}, {"height", "hIt"}, {"key", "kE"},
    {"create", "krE'At"}, {"creative", "krE'Ativ"}, {"creation", "krE'AS@n"},
    {"react", "rE'akt"}, {"reality", "rE'aliti"}, {"realize", "rE@lIz"}, {"serious", "siri@s"},
    {"series", "siriz"}, {"period", "piri@d"}, {"experience", "ik'spiri@ns"},
    {"hero", "hirO"}, {"zero", "zirO"}, {"wind", "wind"}, {"window", "windO"},
    {"children", "Cildr@n"}, {"argue", "orgyU"}, {"anger", "aNgR"}, {"angel", "Anj@l"},
    {"banger", "baNR"}, {"eager", "EgR"}, {"burger", "bRgR"}, {"giant", "jI@nt"},
    {"changing", "CAnjiN"}, {"challenging", "Cal@njiN"}, {"worn", "wQrn"}, {"muscle", "mus@l"},
    {"miracle", "mir@k@l"}, {"study", "studi"}, {"tiny", "tIni"}, {"most", "mOst"},
    {"post", "pOst"}, {"host", "hOst"}, {"ghost", "gOst"}, {"almost", "QlmOst"},
    {"legend", "lej@nd"}, {"mic", "mIk"}, {"crew", "krU"}, {"squad", "skwod"},
    {"sneaker", "snEkR"}, {"jordan", "jQrd@n"}, {"rhythm", "riD@m"}, {"lyric", "lirik"},
    {"verse", "vRs"}, {"break", "brAk"}, {"great", "grAt"}, {"steak", "stAk"},
    // legends and era words from the tribute verse
    {"kobe", "kObi"}, {"lebron", "l@'bron"}, {"iverson", "IvRs@n"}, {"gallop", "gal@p"},
    {"bass", "bAs"}, {"dmc", "dE'emsE"}, {"nineties", "nIntiz"}, {"seventies", "sev@ntiz"},
    {"trophy", "trOfi"}, {"flu", "flU"}, {"eminem", "em@nem"},
    {"breath", "breT"}, {"death", "deT"}, {"sweat", "swet"}, {"threat", "Tret"}, {"ballin'", "bQlin"}, {"suede", "swAd"}, {"business", "bizn@s"},
    // number words the rules get wrong (the spelled-out digits use them)
    {"nineteen", "nIntEn"}, {"ninety", "nInti"}, {"seventeen", "sev@ntEn"},
    {"seventy", "sev@nti"}, {"eleven", "@'lev@n"}, {"hundred", "hundr@d"},
    {"thousand", "TWz@nd"}, {"eighteen", "AtEn"}, {"eighty", "Ati"}, {"thirty", "TRti"},
    // brands and acronyms that turn up in bars
    {"versace", "vR'soCA"}, {"gucci", "gUCi"}, {"ferrari", "f@'rori"}, {"nike", "nIki"},
    {"adidas", "@'dEd@s"}, {"lamborghini", "lambQr'gEni"}, {"nba", "enbE'A"},
    {"mvp", "emvE'pE"}, {"dj", "dE'jA"}, {"mc", "em'sE"}, {"og", "O'jE"}, {"tv", "tE'vE"},
    {"ok", "O'kA"},
};

const std::unordered_map<std::string_view, std::string_view>& exceptions() {
    static const std::unordered_map<std::string_view, std::string_view> table = [] {
        std::unordered_map<std::string_view, std::string_view> map;
        for (const auto& [word, phones] : kExceptions) map.emplace(word, phones);
        return map;
    }();
    return table;
}

bool ends_with(std::string_view s, std::string_view suffix) {
    return s.size() > suffix.size() && s.substr(s.size() - suffix.size()) == suffix;
}

bool voiceless(char phoneme) {
    return phoneme == 'p' || phoneme == 't' || phoneme == 'k' || phoneme == 'f' ||
           phoneme == 'T' || phoneme == 's' || phoneme == 'S' || phoneme == 'C';
}

char last_sound(std::string_view phones) {
    for (std::size_t i = phones.size(); i-- > 0;)
        if (phones[i] != '\'') return phones[i];
    return '\0';
}

/// Plural / third-person "-s": /iz/ after sibilants, /s/ after voiceless sounds, else /z/.
std::string add_s(std::string phones) {
    const char last = last_sound(phones);
    if (last == 's' || last == 'z' || last == 'S' || last == 'Z' || last == 'C' || last == 'j')
        return phones + "iz";
    return phones + (voiceless(last) ? "s" : "z");
}

/// Past tense "-ed": /id/ after t or d, /t/ after voiceless sounds, else /d/.
std::string add_ed(std::string phones) {
    const char last = last_sound(phones);
    if (last == 't' || last == 'd') return phones + "id";
    return phones + (voiceless(last) ? "t" : "d");
}

/// Looks a word up in the exceptions, also trying common inflections of an exception
/// ("ones", "coming", "lovers", "proved").
std::string lookup(std::string_view word) {
    const auto& table = exceptions();
    if (const auto it = table.find(word); it != table.end()) return std::string(it->second);

    const auto stem_of = [&](std::string_view stem) -> const std::string_view* {
        if (const auto it = table.find(stem); it != table.end()) return &it->second;
        return nullptr;
    };
    const auto try_stem = [&](std::string_view stem) -> std::string {
        // The stem must be a plausible word on its own: "th(ing)" is not "the".
        if (stem.size() < 2 || stem.find_first_of("aeiouy") == std::string_view::npos) return {};
        if (const auto* hit = stem_of(stem)) return std::string(*hit);
        const std::string with_e = std::string(stem) + "e"; // com(ing) -> come
        if (const auto* hit = stem_of(with_e)) return std::string(*hit);
        if (stem.size() > 2 && stem[stem.size() - 1] == stem[stem.size() - 2])
            if (const auto* hit = stem_of(stem.substr(0, stem.size() - 1))) return std::string(*hit);
        return {};
    };

    if (ends_with(word, "'s"))
        if (auto base = try_stem(word.substr(0, word.size() - 2)); !base.empty()) return add_s(base);
    if (ends_with(word, "s") && !ends_with(word, "ss"))
        if (auto base = try_stem(word.substr(0, word.size() - 1)); !base.empty()) return add_s(base);
    if (ends_with(word, "es"))
        if (auto base = try_stem(word.substr(0, word.size() - 2)); !base.empty()) return add_s(base);
    if (ends_with(word, "ing"))
        if (auto base = try_stem(word.substr(0, word.size() - 3)); !base.empty()) return base + "iN";
    if (ends_with(word, "in'"))
        if (auto base = try_stem(word.substr(0, word.size() - 3)); !base.empty()) return base + "in";
    if (ends_with(word, "ed"))
        if (auto base = try_stem(word.substr(0, word.size() - 2)); !base.empty()) return add_ed(base);
    if (ends_with(word, "ers"))
        if (auto base = try_stem(word.substr(0, word.size() - 3)); !base.empty()) return base + "Rz";
    if (ends_with(word, "er"))
        if (auto base = try_stem(word.substr(0, word.size() - 2)); !base.empty()) return base + "R";
    if (ends_with(word, "ly"))
        if (auto base = try_stem(word.substr(0, word.size() - 2)); !base.empty()) return base + "li";
    constexpr std::pair<std::string_view, std::string_view> kDerivations[] = {
        {"ment", "m@nt"}, {"less", "l@s"}, {"ness", "n@s"}, {"ful", "f@l"}};
    for (const auto& [suffix, sound] : kDerivations)
        if (ends_with(word, suffix))
            if (auto base = try_stem(word.substr(0, word.size() - suffix.size())); !base.empty())
                return base + std::string(sound);
    return {};
}

// Compounds are split only when both halves are known building blocks, so "someone"
// becomes some+one while "notice" is left to the rules.
constexpr std::string_view kCompoundHeads[] = {
    "come", "some", "any", "every", "no", "basket", "foot", "base", "hard", "cross", "take", "sneaker",
    "back", "full", "half", "down", "up", "head", "dead", "main", "him", "her", "my", "your",
    "them", "our", "it", "with", "what", "when", "where", "how", "for", "over", "under", "out",
    "in", "play", "street", "mid", "fast", "free", "jump", "lay", "turn", "home", "all", "super",
    "rock", "hip", "heart", "life", "work", "court", "side", "day", "night", "black", "gold",
    "pay", "check", "thank", "wind", "sun", "moon", "star", "high", "low", "hand"};
constexpr std::string_view kCompoundTails[] = {
    "one", "thing", "things", "body", "where", "how", "time", "times", "self", "selves", "ball",
    "balls", "wood", "court", "courts", "board", "town", "side", "line", "lines", "head", "heads",
    "light", "lights", "night", "day", "days", "stream", "fall", "over", "out", "ever", "way",
    "ways", "star", "stars", "room", "house", "ground", "top", "shot", "shots", "back", "break",
    "game", "games", "walk", "work", "life", "style", "world", "land", "hop", "up", "to",
    "check", "man", "men", "mill", "shine", "bound", "shake"};

template <std::size_t N>
bool listed(const std::string_view (&list)[N], std::string_view word) {
    for (std::string_view w : list)
        if (w == word) return true;
    return false;
}

std::string transcribe(std::string_view word, int depth);

std::string transcribe_compound(std::string_view word, int depth) {
    if (depth > 2) return {};
    for (std::size_t cut = word.size() - 2; cut >= 2; --cut) {
        const std::string_view head = word.substr(0, cut);
        const std::string_view tail = word.substr(cut);
        if (listed(kCompoundHeads, head) && listed(kCompoundTails, tail))
            return transcribe(head, depth + 1) + transcribe(tail, depth + 1);
    }
    return {};
}

/// Handles contractions: "n't" adds a syllable after a consonant (didn't, couldn't) and
/// the clitics 'll 've 'd 're 'm attach to the host word without one.
std::string transcribe_contraction(std::string_view word, int depth) {
    if (ends_with(word, "n't")) {
        const std::string host = transcribe(word.substr(0, word.size() - 3), depth + 1);
        return host + "@nt";
    }
    const std::pair<std::string_view, std::string_view> clitics[] = {
        {"'ll", "l"}, {"'ve", "v"}, {"'d", "d"}, {"'re", "r"}, {"'m", "m"}, {"'s", ""}};
    for (const auto& [clitic, sound] : clitics) {
        if (!ends_with(word, clitic)) continue;
        std::string host = transcribe(word.substr(0, word.size() - clitic.size()), depth + 1);
        return clitic == "'s" ? add_s(host) : host + std::string(sound);
    }
    return {};
}

std::string transcribe(std::string_view word, int depth) {
    if (word.empty()) return {};
    if (auto hit = lookup(word); !hit.empty()) return hit;
    if (word.find('\'') != std::string_view::npos && depth < 3) {
        if (auto contracted = transcribe_contraction(word, depth); !contracted.empty())
            return contracted;
    }
    if (word.size() >= 5 && word.find('\'') == std::string_view::npos)
        if (auto compound = transcribe_compound(word, depth); !compound.empty()) return compound;

    // Rap spelling of -ing: "ballin'" sounds like "ballin", but the vowel rules need the
    // full suffix to know the stem vowel is long ("ridin'" -> ride).
    if (ends_with(word, "in'")) {
        std::string full(word.substr(0, word.size() - 1));
        full += 'g';
        std::string phones = apply_english_rules(full);
        if (ends_with(phones, "iN")) phones.back() = 'n';
        return phones;
    }
    std::string letters;
    for (char c : word)
        if (c >= 'a' && c <= 'z') letters += c;
    return apply_english_rules(letters);
}

/// Chooses the syllable a rhyme starts from when no exception marked it.
///
/// English stress is lexical, but for rhyming only the *last* stressed vowel matters,
/// and that is usually the final syllable unless the word ends in a weak one: a schwa
/// (-tion, -le, -ous), or a final -er, -y, -ing, -ic, -et. We step back over at most two
/// weak syllables ("legendary" -> leg-en-DAR-y) and never land on a schwa.
int english_stress(const Pronunciation& p) {
    const int n = p.syllables();
    if (n <= 1) return 0;
    const auto vowel = [&](int k) { return p.phonemes[p.nuclei[static_cast<std::size_t>(k)].begin]; };
    const auto weak = [&](int k) {
        const char v = vowel(k);
        if (v == '@') return true;
        if (k != n - 1) return false;
        if (v == 'R') return true;
        if (v == 'i') {
            // /ɪr/ as in "appear" or "career" is a full, stressed vowel.
            const std::size_t after = p.nuclei[static_cast<std::size_t>(k)].begin + 1;
            return !(after < p.phonemes.size() && p.phonemes[after] == 'r');
        }
        return false;
    };
    int k = n - 1;
    int skipped = 0;
    while (k > 0 && weak(k) && skipped < 2) {
        --k;
        ++skipped;
    }
    if (vowel(k) == '@') {
        for (int j = k + 1; j < n; ++j)
            if (vowel(j) != '@') return j;
    }
    return k;
}

} // namespace

Pronunciation pronounce_english(std::string_view word) {
    Pronunciation p = from_marked_phonemes(transcribe(word, 0), Language::English);
    if (p.stress < 0) p.stress = english_stress(p);
    return p;
}

} // namespace street::bars::detail
