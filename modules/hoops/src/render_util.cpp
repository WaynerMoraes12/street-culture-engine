#include "render_util.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <stdexcept>

#include "street/core/text.hpp"

namespace street::hoops::detail {

term::Rgb readable(term::Rgb c) {
    const double luma = 0.2126 * c.r + 0.7152 * c.g + 0.0722 * c.b;
    return luma < 90.0 ? term::lerp(c, term::palette::white, 0.35) : c;
}

std::string team_tag(const Team& team) { return term::paint_bold(team.abbrev, readable(team.color)); }

std::string team_label(const Team& team) { return term::paint(team.full_name(), readable(team.color)); }

const Team& team_by_id(const std::vector<Team>& teams, std::uint32_t id) {
    for (const auto& t : teams)
        if (t.id == id) return t;
    throw std::out_of_range("unknown team id " + std::to_string(id));
}

std::string win_pct(double pct) {
    const std::string s = text::fixed(pct, 3);
    return s.starts_with("0.") ? s.substr(1) : s;
}

std::string signed_fixed(double value, int decimals) {
    const std::string body = text::fixed(std::abs(value), decimals);
    const bool zero = std::abs(value) < 0.5 * std::pow(10.0, -decimals);
    return (zero ? " " : value > 0 ? "+" : "-") + body;
}

std::string made_of(int made, int attempts) { return std::format("{}-{}", made, attempts); }

std::string rating_cell(int rating) {
    const std::string s = std::to_string(rating);
    if (rating >= 88) return term::paint_bold(s, term::palette::gold);
    if (rating >= 78) return term::paint(s, term::palette::green);
    if (rating >= 60) return s;
    return term::paint(s, term::palette::dark);
}

/// Drops trailing spaces, including spaces hidden in front of trailing ANSI escapes,
/// so colored and plain renderings stay identical once colors are stripped.
static void trim_right_visible(std::string& s) {
    std::string escapes;
    for (;;) {
        if (!s.empty() && s.back() == ' ') {
            s.pop_back();
        } else if (!s.empty() && s.back() == 'm') {
            // Only a complete SGR sequence "\x1b[<digits;>m" at the very end counts.
            const auto start = s.rfind('\x1b');
            if (start == std::string::npos || start + 2 > s.size() - 1 || s[start + 1] != '[') break;
            const bool sgr = std::all_of(s.begin() + static_cast<std::ptrdiff_t>(start) + 2, s.end() - 1,
                                         [](char c) { return (c >= '0' && c <= '9') || c == ';'; });
            if (!sgr) break;
            escapes.insert(0, s.substr(start));
            s.erase(start);
        } else {
            break;
        }
    }
    s += escapes;
}

std::string hstack(const std::vector<std::string>& blocks, std::size_t gap) {
    std::vector<std::vector<std::string>> lines;
    std::vector<std::size_t> widths;
    std::size_t rows = 0;
    for (const auto& block : blocks) {
        auto split = text::split_lines(block);
        while (!split.empty() && split.back().empty()) split.pop_back();
        std::size_t w = 0;
        for (const auto& l : split) w = std::max(w, term::width(l));
        rows = std::max(rows, split.size());
        widths.push_back(w);
        lines.push_back(std::move(split));
    }
    std::string out;
    for (std::size_t r = 0; r < rows; ++r) {
        std::string row;
        for (std::size_t b = 0; b < lines.size(); ++b) {
            const std::string cell = r < lines[b].size() ? lines[b][r] : std::string{};
            const bool last = b + 1 == lines.size();
            row += last ? cell : term::pad_right(cell, widths[b]) + std::string(gap, ' ');
        }
        trim_right_visible(row);
        out += row + '\n';
    }
    return out;
}

std::vector<std::string> wrap(std::string_view text_in, std::size_t width) {
    std::vector<std::string> out;
    std::string current;
    for (const auto& word : text::split(text_in, ' ', true)) {
        if (!current.empty() && term::width(current) + 1 + term::width(word) > width) {
            out.push_back(current);
            current.clear();
        }
        current += (current.empty() ? "" : " ") + word;
    }
    if (!current.empty()) out.push_back(current);
    if (out.empty()) out.emplace_back();
    return out;
}

} // namespace street::hoops::detail
