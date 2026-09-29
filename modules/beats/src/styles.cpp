#include "street/beats/style.hpp"
#include "street/core/text.hpp"

#include <algorithm>
#include <array>
#include <string>

namespace street::beats {
namespace {

constexpr std::array<std::string_view, 5> kBoomBapKeys = {"Am", "Dm", "Gm", "Cm", "Fm"};
constexpr std::array<std::string_view, 5> kTrapKeys = {"C#m", "Fm", "Ebm", "Gm", "Bbm"};
constexpr std::array<std::string_view, 5> kLoFiKeys = {"F", "Eb", "Dm", "Bb", "Am"};
constexpr std::array<std::string_view, 5> kDrillKeys = {"F#m", "C#m", "Gm", "Bbm", "Ebm"};
constexpr std::array<std::string_view, 5> kFunkKeys = {"Am", "Em", "Dm", "Gm", "Cm"};
constexpr std::array<std::string_view, 5> kElectroKeys = {"Em", "Am", "Bm", "Dm", "F#m"};
constexpr std::array<std::string_view, 5> kGallopKeys = {"Em", "F#m", "Am", "Dm", "Bm"};

constexpr std::array<StyleInfo, 7> kStyles = {{
    {Style::BoomBap, "boombap", "Boom Bap",
     "Dusty East Coast swing: syncopated kicks, cracking 2 & 4, ghost notes, jazzy 7th/9th "
     "chords through 12-bit SP-1200 grit",
     90, 0.63, "Am", 88, 94, kBoomBapKeys, term::Rgb{255, 170, 60},
     "dusty swing · ghost notes · 12-bit grit"},
    {Style::Trap, "trap", "Trap",
     "Half-time bounce: clap on 3, rolling 1/32 and triplet hats, sliding 808s under dark minor "
     "chords",
     140, 0.50, "C#m", 130, 150, kTrapKeys, term::Rgb{239, 68, 68},
     "half-time bounce · hat rolls · sliding 808s"},
    {Style::LoFi, "lofi", "Lo-Fi",
     "Lazy, late and warm: soft drums, FM e-piano maj7/min9 chords, tape wow, vinyl crackle, "
     "low-pass haze",
     78, 0.58, "F", 70, 85, kLoFiKeys, term::Rgb{139, 160, 255},
     "lazy swing · FM e-piano · tape and vinyl"},
    {Style::Drill, "drill", "Drill",
     "Skippy 3-3-2 hats, snares on 3 with off-beat stabs, gliding 808s, ominous harmonic minor",
     142, 0.52, "F#m", 140, 145, kDrillKeys, term::Rgb{150, 110, 255},
     "skippy 3-3-2 hats · gliding 808s"},
    {Style::Funk, "funk", "Funk Carioca",
     "Rio baile tamborzão: booming kick and low tom in a 3-3-2 tresillo, claps, shaker, big 808",
     130, 0.55, "Am", 126, 134, kFunkKeys, term::Rgb{34, 197, 94},
     "tamborzão 3-3-2 · claps · big 808"},
    // The eras: the Roland TR-808 (introduced 1980) defined early-80s Bronx electro-funk;
    // the New Wave of British Heavy Metal made the gallop its signature around the same time.
    {Style::Electro, "electro", "Electro 1982",
     "Early-80s Bronx electro-funk on a TR-808 (introduced 1980): syncopated boom kick, cowbell "
     "and handclaps, busy 16th hats, a sequenced octave-jumping synth bass, bright saw stabs",
     128, 0.50, "Em", 124, 132, kElectroKeys, term::Rgb{0, 220, 255},
     "TR-808 cowbell · sequenced bass · saw stabs"},
    {Style::Gallop, "gallop", "Metal Gallop",
     "A tribute to the New Wave of British Heavy Metal (London, 1980s): the eighth-two-sixteenths "
     "gallop on kick, bass and palm-muted power chords, ride and crash, ringing chords on phrase "
     "ends",
     172, 0.50, "Em", 160, 185, kGallopKeys, term::Rgb{200, 200, 215},
     "gallop rhythm · palm-muted power chords"},
}};

std::string normalise(std::string_view s) {
    std::string out;
    for (char c : street::text::to_lower(street::text::trim(s)))
        if (c != '-' && c != ' ' && c != '_') out += c;
    return out;
}

} // namespace

std::span<const StyleInfo> styles() {
    return kStyles;
}

const StyleInfo& info(Style style) {
    const auto it = std::find_if(kStyles.begin(), kStyles.end(),
                                 [&](const StyleInfo& s) { return s.style == style; });
    return it != kStyles.end() ? *it : kStyles.front();
}

std::string style_list(std::string_view separator) {
    std::string out;
    for (const auto& s : kStyles)
        out += (out.empty() ? "" : std::string(separator)) + std::string(s.id);
    return out;
}

std::optional<Style> parse_style(std::string_view id) {
    const std::string wanted = normalise(id);
    if (wanted.empty()) return std::nullopt;
    for (const auto& s : kStyles)
        if (wanted == normalise(s.id) || wanted == normalise(s.name)) return s.style;
    if (wanted == "carioca" || wanted == "tamborzao" || wanted == "baile") return Style::Funk;
    if (wanted == "electrofunk" || wanted == "808") return Style::Electro;
    if (wanted == "metal" || wanted == "nwobhm" || wanted == "heavymetal") return Style::Gallop;
    return std::nullopt;
}

} // namespace street::beats
