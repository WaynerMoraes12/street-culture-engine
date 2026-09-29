// ASCII half-court shot chart.
//
// The court is drawn on a 51 x 24 character grid: one column per foot from
// sideline to sideline and one row per two feet from the baseline (top) to half
// court (bottom). Terminal cells are about twice as tall as they are wide, so
// this scale keeps the three-point arc round. Court lines are rasterised from
// the real dimensions; each shot lands in the cell containing its (x, y).

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <numbers>

#include "render_util.hpp"
#include "street/core/text.hpp"
#include "street/hoops/render.hpp"

namespace street::hoops {
namespace {

constexpr int kCols = 51;
constexpr int kRows = 24;
constexpr double kRowFeet = 2.0;
constexpr double kPaintHalfWidth = 8.0;
constexpr double kFreeThrowY = 19.0;
constexpr double kCircleRadius = 6.0;
constexpr double kHalfCourtY = 47.0;
constexpr term::Rgb kLineColor{150, 112, 72}; // hardwood-toned court lines

struct Cell {
    std::string glyph = " ";
    bool line = false;
    bool rim = false;
    int made = 0, missed = 0;
};

using Grid = std::vector<std::array<Cell, kCols>>; // heap: ~1,200 cells

double cell_x(int col) { return col - 25.0; }
double cell_y(int row) { return (row + 0.5) * kRowFeet; }
int col_of(double x) { return std::clamp(static_cast<int>(std::lround(x + 25.0)), 0, kCols - 1); }
int row_of(double y) { return std::clamp(static_cast<int>(y / kRowFeet), 0, kRows - 1); }

/// Box-drawing glyph that follows a circle's tangent at offset (rx, ry) from its
/// center. Screen y grows downward, like court y with the baseline on top.
std::string_view arc_glyph(double rx, double ry) {
    double angle = std::atan2(rx, -ry) * 180.0 / std::numbers::pi; // tangent (-ry, rx)
    if (angle < 0.0) angle += 180.0;
    if (angle < 22.5 || angle >= 157.5) return "─";
    if (angle < 67.5) return "╲";
    if (angle < 112.5) return "│";
    return "╱";
}

void put_line(Grid& g, int row, int col, std::string_view glyph) {
    if (row < 0 || row >= kRows || col < 0 || col >= kCols) return;
    Cell& c = g[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)];
    c.glyph = std::string(glyph);
    c.line = true;
}

/// Rasterises the part of a circle with min_y <= y <= max_y like a line drawer:
/// where the curve is shallow (in screen cells) step column by column, where it
/// is steep step row by row, so the stroke is one cell thick and unbroken.
void draw_arc(Grid& g, double cx, double cy, double r, double min_y, double max_y) {
    const auto mark = [&](double x, double y, bool from_columns) {
        if (y < min_y || y > max_y || y < 0.0 || y >= kRows * kRowFeet || std::abs(x) > 25.5) return;
        const double rx = x - cx, ry = y - cy;
        // Tangent slope in cells: rows are two feet tall, columns one foot wide.
        const bool shallow = std::abs(ry) * 2.0 >= std::abs(rx);
        if (shallow != from_columns) return;
        put_line(g, row_of(y), col_of(x), arc_glyph(rx, ry));
    };
    for (int row = 0; row < kRows; ++row) {
        const double dy = cell_y(row) - cy;
        if (std::abs(dy) > r) continue;
        const double dx = std::sqrt(r * r - dy * dy);
        mark(cx - dx, cell_y(row), false);
        mark(cx + dx, cell_y(row), false);
    }
    for (int col = 0; col < kCols; ++col) {
        const double dx = cell_x(col) - cx;
        if (std::abs(dx) > r) continue;
        const double dy = std::sqrt(r * r - dx * dx);
        mark(cell_x(col), cy - dy, true);
        mark(cell_x(col), cy + dy, true);
    }
}

Grid draw_court() {
    Grid g(kRows);
    // Three-point line: straight corners at 22 ft, then the 23.75 ft arc.
    const double corner_end = kRimY + std::sqrt(kThreeRadius * kThreeRadius - kCornerThree * kCornerThree);
    for (int row = 0; row <= row_of(corner_end - 0.01); ++row) {
        put_line(g, row, col_of(-kCornerThree), "│");
        put_line(g, row, col_of(kCornerThree), "│");
    }
    draw_arc(g, 0.0, kRimY, kThreeRadius, corner_end, kHalfCourtY);
    // The paint and the free-throw circle below the line.
    const int ft_row = row_of(kFreeThrowY);
    for (int row = 0; row < ft_row; ++row) {
        put_line(g, row, col_of(-kPaintHalfWidth), "│");
        put_line(g, row, col_of(kPaintHalfWidth), "│");
    }
    for (int col = col_of(-kPaintHalfWidth); col <= col_of(kPaintHalfWidth); ++col)
        put_line(g, ft_row, col, "─");
    put_line(g, ft_row, col_of(-kPaintHalfWidth), "└");
    put_line(g, ft_row, col_of(kPaintHalfWidth), "┘");
    draw_arc(g, 0.0, kFreeThrowY, kCircleRadius, kFreeThrowY + kRowFeet, kHalfCourtY);
    // Center circle at half court.
    draw_arc(g, 0.0, kHalfCourtY, kCircleRadius, 0.0, kHalfCourtY);
    // Backboard and rim.
    for (int col = col_of(-3.0); col <= col_of(3.0); ++col) put_line(g, row_of(3.0), col, "─");
    Cell& rim = g[static_cast<std::size_t>(row_of(kRimY))][static_cast<std::size_t>(col_of(0.0))];
    rim.glyph = "O";
    rim.rim = true;
    return g;
}

std::string render_cell(const Cell& c) {
    using namespace term;
    if (c.made + c.missed > 0) {
        if (c.made >= c.missed && c.made > 0) return paint_bold("●", palette::green);
        return paint(c.missed >= 2 ? "○" : "·", palette::red);
    }
    if (c.rim) return paint_bold(c.glyph, palette::orange);
    if (c.line) return paint(c.glyph, kLineColor);
    return " ";
}

struct ZoneLine {
    int made = 0, attempts = 0;
    double pct() const { return attempts > 0 ? static_cast<double>(made) / attempts : 0.0; }
};

std::string zone_row(std::string_view label, const ZoneLine& z, double league_avg) {
    using namespace term;
    const std::string pct = z.attempts > 0 ? text::percent(z.pct()) : std::string("—");
    const Rgb tone = z.attempts == 0 ? palette::gray
                     : z.pct() >= league_avg ? palette::green
                                             : palette::red;
    return std::string(label) + pad_left(detail::made_of(z.made, z.attempts), 8) + "  " +
           paint(pad_left(pct, 6), tone) + "  " + paint(meter(z.pct(), 10), tone);
}

} // namespace

std::string render_shot_chart(const GameResult& result, int side, const Team& team) {
    using namespace term;
    Grid grid = draw_court();
    std::array<ZoneLine, 3> zones{};
    for (const Shot& s : result.shots) {
        if (s.side != side) continue;
        Cell& c = grid[static_cast<std::size_t>(row_of(s.y))][static_cast<std::size_t>(col_of(s.x))];
        (s.made ? c.made : c.missed) += 1;
        ZoneLine& z = zones[static_cast<std::size_t>(s.zone)];
        ++z.attempts;
        z.made += s.made ? 1 : 0;
    }

    std::string chart = paint("╭" + repeat("─", kCols) + "╮", kLineColor) + '\n';
    for (const auto& row : grid) {
        std::string line = paint("│", kLineColor);
        for (const Cell& c : row) line += render_cell(c);
        chart += line + paint("│", kLineColor) + '\n';
    }
    chart += paint("╰" + repeat("─", kCols) + "╯", kLineColor) + '\n';

    const TeamTotals t = team_totals(result, side);
    const int attempts = zones[0].attempts + zones[1].attempts + zones[2].attempts;
    std::vector<std::string> legend = {
        paint_bold(text::to_upper(team.full_name()), detail::readable(team.color)),
        paint(std::format("shot chart · {} field-goal attempts", attempts), palette::gray),
        "",
        paint_bold("●", palette::green) + " made   " + paint("○", palette::red) + " missed   " +
            paint("·", palette::red) + " single miss",
        "",
        emphasize("ZONE        FG     PCT"),
        zone_row("RIM ", zones[0], 0.62),
        zone_row("MID ", zones[1], 0.41),
        zone_row("3PT ", zones[2], 0.36),
        "",
        "FG  " + pad_left(detail::made_of(t.fgm, t.fga), 8) + "  " + pad_left(text::percent(t.fg_pct()), 6),
        "FT  " + pad_left(detail::made_of(t.ftm, t.fta), 8) + "  " + pad_left(text::percent(t.ft_pct()), 6),
    };
    if (result.shots.empty())
        legend.push_back(paint("(no shot locations recorded)", palette::gray));
    std::string legend_block;
    for (const auto& l : legend) legend_block += l + '\n';
    return detail::hstack({chart, legend_block}, 3);
}

} // namespace street::hoops
