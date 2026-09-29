#include <array>
#include <iostream>
#include <string>

#include "commands.hpp"
#include "street/core/term.hpp"
#include "street/core/version.hpp"

namespace street::app {
namespace {

// "ANSI Shadow" block letters, six rows each.
using Glyph = std::array<const char*, 6>;

constexpr Glyph kS = {"███████╗", "██╔════╝", "███████╗", "╚════██║", "███████║", "╚══════╝"};
constexpr Glyph kT = {"████████╗", "╚══██╔══╝", "   ██║   ", "   ██║   ", "   ██║   ", "   ╚═╝   "};
constexpr Glyph kR = {"██████╗ ", "██╔══██╗", "██████╔╝", "██╔══██╗", "██║  ██║", "╚═╝  ╚═╝"};
constexpr Glyph kE = {"███████╗", "██╔════╝", "█████╗  ", "██╔══╝  ", "███████╗", "╚══════╝"};

constexpr std::array<const Glyph*, 6> kWord = {&kS, &kT, &kR, &kE, &kE, &kT};

// A striped synthwave sun sinking into the horizon, one color per row.
constexpr std::array<const char*, 6> kSun = {
    "     ▄▄▄████▄▄▄     ", "   ▄████████████▄   ", "  ████████████████  ",
    "  ▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀  ", "   ▀▀▀▀▀▀▀▀▀▀▀▀▀▀   ", "     ▀▀▀▀▀▀▀▀▀▀     ",
};
constexpr std::array<term::Rgb, 6> kSunColors = {{
    {255, 232, 92}, {255, 196, 64}, {255, 146, 60}, {255, 96, 108}, {240, 64, 150}, {196, 52, 196},
}};

} // namespace

void print_banner() {
    using namespace term;
    // 1980s sunset: chrome gold → hot pink → neon purple.
    constexpr std::array<Rgb, 3> stops = {palette::gold, palette::pink, palette::purple};
    std::cout << '\n';
    for (std::size_t row = 0; row < 6; ++row) {
        std::string line;
        for (const Glyph* g : kWord) line += (*g)[row];
        std::cout << "  " << gradient(line, stops) << "    " << paint(kSun[row], kSunColors[row]) << '\n';
    }
    constexpr std::array<Rgb, 3> horizon = {palette::purple, palette::pink, palette::cyan};
    std::cout << "  " << gradient(repeat("━", 74), horizon) << '\n';
    std::cout << "  " << paint_bold("C U L T U R E   E N G I N E", palette::cyan)
              << paint("   est. 1973 · the Bronx", palette::dark) << '\n';
    std::cout << "  " << paint(std::string(kTagline), palette::gray) << paint("  v", palette::dark)
              << paint(std::string(kVersion), palette::dark) << "\n\n";
}

} // namespace street::app
