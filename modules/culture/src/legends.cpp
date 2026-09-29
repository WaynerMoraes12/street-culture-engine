#include "street/culture/legends.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <vector>

#include "street/core/term.hpp"
#include "street/core/text.hpp"

namespace street::culture {
namespace {

using namespace term;

// clang-format off
constexpr std::array kMoments = {
    Moment{1973, "Aug 11", Pillar::Music, "Hip-hop's birthday party",
           "DJ Kool Herc loops drum breaks on two turntables at a back-to-school party at 1520 Sedgwick Avenue, the Bronx.",
           "the break lives on in boom-bap: street beat --style boombap"},
    Moment{1975, "", Pillar::Metal, "Iron Maiden forms in East London",
           "Bassist Steve Harris starts the band that will lead the New Wave of British Heavy Metal.",
           "the gallop style is a tribute: street beat --style gallop"},
    Moment{1979, "", Pillar::Music, "\"Rapper's Delight\"",
           "The Sugarhill Gang carry rap from the parks of New York to radio stations across the world.",
           "street bars measures the rhymes and the flow"},
    Moment{1980, "", Pillar::Tech, "The Roland TR-808",
           "A commercial flop at launch, its booming kick drum becomes the low end of hip-hop for decades.",
           "every beat's kick is a synthesized 808: sine + pitch drop + drive"},
    Moment{1980, "Apr 14", Pillar::Metal, "Iron Maiden's debut album",
           "The self-titled record introduces Eddie, the most famous mascot in heavy metal.",
           "street beat --style gallop"},
    Moment{1982, "", Pillar::Music, "\"Planet Rock\"",
           "Afrika Bambaataa & the Soulsonic Force build electro on the TR-808.",
           "street beat --style electro"},
    Moment{1982, "", Pillar::Music, "\"The Message\"",
           "Grandmaster Flash and the Furious Five turn rap into street reporting.",
           "street bars analyzes verses like a scout reads a box score"},
    Moment{1983, "", Pillar::Metal, "\"The Trooper\"",
           "Iron Maiden's galloping bassline, eighth note plus two sixteenths, defines a sound.",
           "the gallop style plays that rhythm on kick and bass"},
    Moment{1984, "Jun 19", Pillar::Hoops, "The Bulls draft Michael Jordan",
           "Chicago takes him third overall. Six championships follow in the 1990s.",
           "hoops commentary still compares great fadeaways to his"},
    Moment{1985, "", Pillar::Kicks, "The Air Jordan 1",
           "Nike's first Jordan signature shoe; the \"Chicago\" colorway defines sneaker culture.",
           "AJ1-CHI is the grail of the kicks exchange"},
    Moment{1985, "", Pillar::Music, "\"I Can't Live Without My Radio\"",
           "LL Cool J's ode to the boombox, the loudspeaker of every 80s block party.",
           "street beat writes WAVs your boombox can play"},
    Moment{1986, "", Pillar::Kicks, "\"My Adidas\"",
           "Run-DMC turn the shell-toe adidas Superstar into hip-hop's uniform.",
           "SSTR-WB trades on the kicks exchange"},
    Moment{1987, "", Pillar::Tech, "The E-mu SP-1200",
           "Its gritty 12-bit sampling becomes the texture of golden-age boom-bap.",
           "the boom-bap kit runs through a bit crusher to sound like one"},
    Moment{1988, "Feb 6", Pillar::Hoops, "The free-throw-line dunk",
           "Jordan wins the Slam Dunk Contest in Chicago, taking off from the free-throw line in the Air Jordan 3.",
           "the Air Jordan 3 trades as AJ3-BLC"},
    Moment{1988, "", Pillar::Tech, "The Akai MPC60",
           "Designed with Roger Linn, its swing quantize gives hip-hop drums their bounce.",
           "street beat --swing 62 applies the same off-beat delay"},
    Moment{1988, "", Pillar::Music, "\"It Takes a Nation of Millions to Hold Us Back\"",
           "Public Enemy's dense, political wall of samples.",
           "street bars --ghostwrite writes verses of its own"},
    Moment{1989, "May 7", Pillar::Hoops, "\"The Shot\"",
           "Jordan's buzzer-beater over Craig Ehlo knocks Cleveland out of the playoffs.",
           "clutch buckets in street game get the big-moment call"},
    Moment{1992, "Dec 15", Pillar::Music, "\"The Chronic\"",
           "Dr. Dre's debut solo album takes G-funk from Compton to the world.",
           "the league's CPT Compton Lowriders"},
    Moment{1994, "Apr 19", Pillar::Music, "\"Illmatic\"",
           "Nas, 20 years old, puts the Queensbridge Houses on the map.",
           "the league's QNS Queens Bridge Poets"},
    Moment{1994, "", Pillar::Music, "\"Southernplayalisticadillacmuzik\"",
           "OutKast's debut announces that the South has something to say.",
           "the league's ATL Atlanta 808s"},
    Moment{1996, "Jun 26", Pillar::Hoops, "Allen Iverson goes first overall",
           "The 76ers draft \"The Answer\"; Reebok gives him his first signature shoe, the Question.",
           "QSTN-RED trades on the kicks exchange"},
    Moment{1997, "Mar 12", Pillar::Hoops, "The crossover",
           "Rookie Iverson shakes Michael Jordan with a crossover and hits the jumper, the most famous handle ever.",
           "ankle-breaking crossovers in street game earn an Iverson shout-out"},
    Moment{1997, "Jun 11", Pillar::Kicks, "The Flu Game",
           "Visibly ill, Jordan scores 38 in Game 5 of the Finals wearing the Air Jordan 12.",
           "AJ12-FLU trades on the kicks exchange"},
    Moment{1998, "", Pillar::Film, "\"He Got Game\"",
           "Spike Lee's basketball film with Ray Allen and Denzel Washington nicknames the Air Jordan 13.",
           "AJ13-HGG trades on the kicks exchange"},
    Moment{2002, "", Pillar::Music, "\"Lose Yourself\"",
           "Eminem's song from 8 Mile, set in Detroit, becomes the first rap song to win the Oscar for Best Original Song.",
           "the league's DET Detroit Crate Diggers"},
    Moment{2003, "Jun 26", Pillar::Hoops, "LeBron James goes first overall",
           "Cleveland drafts the 18-year-old from Akron; Nike launches his first signature shoe, the Air Zoom Generation.",
           "AZG-WHT, his first signature, trades on the kicks exchange"},
    Moment{2006, "Jan 22", Pillar::Hoops, "81",
           "Kobe Bryant scores 81 points against Toronto, the second-highest total in NBA history.",
           "KB6-GRN, the Kobe 6 \"Grinch\", trades on the kicks exchange"},
    Moment{2016, "Apr 13", Pillar::Hoops, "Mamba out",
           "Kobe scores 60 in his final game, against Utah.",
           "the culture index asks whether a shoe follows its athlete"},
    Moment{2016, "Jun 19", Pillar::Hoops, "The Block",
           "LeBron chases down Andre Iguodala in Game 7; Cleveland completes a comeback from 3-1 down.",
           "chase-down blocks in street game get the LeBron treatment"},
    Moment{2023, "Feb 7", Pillar::Hoops, "The scoring crown",
           "LeBron passes Kareem Abdul-Jabbar to become the NBA's all-time leading scorer.",
           "street season --odds runs thousands of seasons in parallel"},
};
// clang-format on

Rgb pillar_color(Pillar pillar) {
    switch (pillar) {
    case Pillar::Music: return palette::pink;
    case Pillar::Metal: return {200, 200, 210};
    case Pillar::Tech: return palette::purple;
    case Pillar::Hoops: return palette::orange;
    case Pillar::Kicks: return palette::cyan;
    case Pillar::Film: return palette::gold;
    }
    return palette::white;
}

struct Era {
    int decade;
    std::string_view label;
    std::array<Rgb, 3> colors; // header gradient
};

// Each decade gets its own look: 70s funk, 80s neon, 90s Bulls red, 2000s chrome, 2010s gold.
constexpr std::array kEras = {
    Era{1970, "THE 70s", {{{150, 90, 50}, {204, 102, 0}, {222, 170, 40}}}},
    Era{1980, "THE 80s", {{{255, 64, 129}, {139, 92, 246}, {34, 211, 238}}}},
    Era{1990, "THE 90s", {{{206, 17, 65}, {239, 68, 68}, {236, 236, 236}}}},
    Era{2000, "THE 2000s", {{{120, 144, 156}, {200, 210, 220}, {59, 130, 246}}}},
    Era{2010, "THE 2010s", {{{85, 37, 130}, {139, 92, 246}, {253, 185, 39}}}},
    Era{2020, "THE 2020s", {{{253, 185, 39}, {255, 196, 0}, {236, 236, 236}}}},
};

// Greedy word wrap; stories are plain ASCII/UTF-8 prose without escape codes.
std::vector<std::string> wrap(std::string_view prose, std::size_t columns) {
    std::vector<std::string> lines(1);
    for (const auto& word : text::split(prose, ' ', true)) {
        if (!lines.back().empty() && width(lines.back()) + 1 + width(word) > columns) lines.emplace_back();
        if (!lines.back().empty()) lines.back() += ' ';
        lines.back() += word;
    }
    return lines;
}

const Era& era_of(int year) {
    const int decade = year / 10 * 10;
    for (const auto& era : kEras)
        if (era.decade == decade) return era;
    return kEras.back();
}

std::string cassette(std::size_t count, int first, int last) {
    // A C-90 tape label: two reels, the title and the running order.
    const std::string title = std::format("THE CULTURE  ·  C-90  ·  {} → {}", first, last);
    const std::array<Rgb, 3> neon = {palette::gold, palette::pink, palette::purple};
    std::vector<std::string> lines;
    lines.push_back(center(gradient(title, neon), 70));
    lines.push_back("");
    lines.push_back(paint("     ╭───╮      ", palette::gray) + paint(repeat("▓", 44), {90, 60, 40}) +
                    paint("      ╭───╮", palette::gray));
    lines.push_back(paint("     │ ◯ │      ", palette::gray) +
                    center(paint(std::format("{} moments · hip-hop · hoops · kicks · metal", count),
                                 palette::gray),
                           44) +
                    paint("      │ ◯ │", palette::gray));
    lines.push_back(paint("     ╰───╯      ", palette::gray) + paint(repeat("▓", 44), {90, 60, 40}) +
                    paint("      ╰───╯", palette::gray));
    return panel("", lines, palette::dark, 70);
}

} // namespace

std::string_view pillar_name(Pillar pillar) noexcept {
    switch (pillar) {
    case Pillar::Music: return "MUSIC";
    case Pillar::Metal: return "METAL";
    case Pillar::Tech: return "TECH";
    case Pillar::Hoops: return "HOOPS";
    case Pillar::Kicks: return "KICKS";
    case Pillar::Film: return "FILM";
    }
    return "";
}

std::optional<Pillar> parse_pillar(std::string_view name) noexcept {
    for (Pillar p : {Pillar::Music, Pillar::Metal, Pillar::Tech, Pillar::Hoops, Pillar::Kicks, Pillar::Film})
        if (text::iequals(name, pillar_name(p))) return p;
    return std::nullopt;
}

std::span<const Moment> moments() noexcept { return kMoments; }

std::string render_timeline(const TimelineOptions& options) {
    std::vector<const Moment*> shown;
    for (const auto& m : kMoments)
        if (m.year >= options.from && m.year <= options.to && (!options.pillar || m.pillar == *options.pillar))
            shown.push_back(&m);
    if (shown.empty()) return paint("  no moments match those filters\n", palette::gray);

    std::string out = cassette(shown.size(), shown.front()->year, shown.back()->year);
    int current_decade = -1;
    bool side_b = false;
    for (const Moment* m : shown) {
        if (m->year >= 1990 && !side_b) {
            side_b = true;
            if (current_decade != -1) out += '\n' + paint("  ▶ flip the tape", palette::gray) + '\n';
            out += '\n' + paint_bold("  SIDE B", palette::white) + paint("  1990 → today", palette::gray) + '\n';
        } else if (current_decade == -1 && m->year < 1990) {
            out += '\n' + paint_bold("  SIDE A", palette::white) + paint("  1973 → 1989", palette::gray) + '\n';
        }
        const Era& era = era_of(m->year);
        if (era.decade != current_decade) {
            current_decade = era.decade;
            const std::string header = std::format("▓▒░ {} ░▒▓", era.label);
            out += '\n' + std::string("  ") + gradient(header, era.colors) + ' ' +
                   gradient(repeat("━", 72 - width(header)), era.colors) + "\n\n";
        }
        const std::string tag = paint_bold(pad_right(pillar_name(m->pillar), 6), pillar_color(m->pillar));
        out += std::format("  {}  {}  {}  {}\n", paint_bold(std::to_string(m->year), era.colors[1]),
                           paint(pad_right(m->date, 6), palette::gray), tag, emphasize(m->title));
        const std::string indent(24, ' '); // lines up with the title
        for (const auto& line : wrap(m->story, 72)) out += indent + line + '\n';
        out += indent + paint("↳ " + std::string(m->engine), palette::dark) + '\n';
    }
    return out;
}

} // namespace street::culture
