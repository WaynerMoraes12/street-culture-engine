#include "street/kicks/catalog.hpp"

#include "street/core/text.hpp"

#include <algorithm>

namespace street::kicks {

std::string Sneaker::display_name() const {
    if (colorway.empty()) return name;
    return name + " \"" + colorway + '"';
}

const std::vector<Sneaker>& classic_catalog() {
    // Grails at the top, general releases at the bottom. base_hype drives the resale premium
    // (see resale_multiplier): the 2015 "Chicago" lands near $1.5k, an AF1 just under retail.
    // clang-format off
    static const std::vector<Sneaker> catalog = {
        {"AJ1-CHI",  "Air Jordan 1 High OG", "Chicago",      2015, dollars(160),     80'000, 0.95,
         "Jordan's first signature shoe; the Chicago colorway debuted in 1985"},
        {"AJ1-BAN",  "Air Jordan 1 High OG", "Banned",       2016, dollars(160),    110'000, 0.78,
         "Star of Nike's 1985 \"Banned\" ad campaign"},
        {"KB6-GRN",  "Nike Kobe 6 Protro",   "Grinch",       2020, dollars(180),    150'000, 0.72,
         "Kobe Bryant wore the \"Grinch\" on Christmas Day 2010"},
        {"AJ3-BLC",  "Air Jordan 3 Retro",   "Black Cement", 2018, dollars(200),    250'000, 0.52,
         "The AJ3 of Jordan's free-throw-line win at the 1988 Slam Dunk Contest"},
        {"AJ4-BRD",  "Air Jordan 4 Retro",   "Bred",         2019, dollars(200),    700'000, 0.62,
         "Worn by Jordan for \"The Shot\" over Craig Ehlo, 1989 playoffs"},
        {"AJ5-FRD",  "Air Jordan 5 Retro",   "Fire Red",     2020, dollars(200),    400'000, 0.40,
         "Jordan scored his career-high 69 points in the AJ5 (1990)"},
        {"AJ6-INF",  "Air Jordan 6 Retro",   "Infrared",     2019, dollars(200),    300'000, 0.42,
         "The shoe of Jordan's first NBA title, 1991"},
        {"AJ11-CON", "Air Jordan 11 Retro",  "Concord",      2018, dollars(220),    800'000, 0.55,
         "Worn by Jordan through the Bulls' record 72-10 season, 1995-96"},
        {"AJ12-FLU", "Air Jordan 12 Retro",  "Flu Game",     2016, dollars(190),    350'000, 0.45,
         "Worn by Jordan in the 1997 \"Flu Game\", Game 5 of the Finals"},
        {"AJ13-HGG", "Air Jordan 13 Retro",  "He Got Game",  2018, dollars(190),    250'000, 0.35,
         "Named for Spike Lee's 1998 film \"He Got Game\", starring Ray Allen"},
        {"FOAM-RYL", "Air Foamposite One",   "Royal",        2011, dollars(220),     40'000, 0.52,
         "Made famous by Penny Hardaway in 1997"},
        {"AZG-WHT",  "Air Zoom Generation",  "White/Crimson", 2003, dollars(110),   200'000, 0.50,
         "LeBron James's first signature shoe, launched for his 2003 rookie season"},
        {"QSTN-RED", "Reebok Question Mid",  "Red Toe",      2016, dollars(150),     60'000, 0.28,
         "Allen Iverson's first signature shoe (1996), worn in his Rookie of the Year season"},
        {"DUNK-PND", "Nike Dunk Low",        "Panda",        2021, dollars(100),  2'000'000, 0.38,
         "A 1985 college basketball shoe, now an everyday best-seller"},
        {"AF1-WHT",  "Nike Air Force 1 '07", "Triple White", 2007, dollars(115), 10'000'000, 0.12,
         "Bruce Kilgore's 1982 design, Nike's first basketball shoe with Air"},
        {"SSTR-WB",  "adidas Superstar",     "White/Black",  1969, dollars(100),  5'000'000, 0.10,
         "Immortalised by Run-DMC's \"My Adidas\" (1986)"},
    };
    // clang-format on
    return catalog;
}

const Sneaker* find_sneaker(const std::vector<Sneaker>& catalog, std::string_view sku) noexcept {
    const auto it = std::find_if(catalog.begin(), catalog.end(),
                                 [&](const Sneaker& s) { return text::iequals(s.sku, sku); });
    return it == catalog.end() ? nullptr : &*it;
}

double resale_multiplier(double hype) noexcept {
    const double h = std::clamp(hype, 0.0, 1.5);
    const double h2 = h * h;
    return 0.8 + 1.2 * h2 + 9.0 * h2 * h2;
}

} // namespace street::kicks
