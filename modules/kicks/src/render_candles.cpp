#include "format.hpp"
#include "street/core/term.hpp"
#include "street/core/text.hpp"
#include "street/kicks/render.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <span>

namespace street::kicks {
namespace {

using detail::colors::down;
using detail::colors::down_dim;
using detail::colors::faint;
using detail::colors::muted;
using detail::colors::up;
using detail::colors::up_dim;

// What occupies one half of a character cell in a candle's column.
enum Mark : std::size_t { kEmpty = 0, kWick = 1, kBody = 2 };

// Glyph for (upper half, lower half). The box-drawing half-lines give each text row two
// price bands of vertical resolution: light strokes are wicks, heavy strokes are bodies.
// clang-format off
constexpr std::array<std::array<std::string_view, 3>, 3> kGlyph = {{
    {" ", "╷", "╻"}, // upper empty
    {"╵", "│", "╽"}, // upper wick
    {"╹", "╿", "┃"}, // upper body
}};
// clang-format on

constexpr std::array<std::string_view, 9> kVolume = {" ", "▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"};

struct Scale {
    Price lo = 0, hi = 0;
    int halves = 2;

    int half_of(Price price) const noexcept {
        const double t = static_cast<double>(price - lo) / static_cast<double>(hi - lo);
        return std::clamp(static_cast<int>(t * halves), 0, halves - 1);
    }
    Price row_price(int row, int rows) const noexcept {
        const double t = (row + 0.5) / rows;
        return lo + static_cast<Price>(t * static_cast<double>(hi - lo));
    }
};

Mark mark_at(const Candle& c, const Scale& scale, int half) noexcept {
    const int body_lo = scale.half_of(std::min(c.open, c.close));
    const int body_hi = scale.half_of(std::max(c.open, c.close));
    if (half >= body_lo && half <= body_hi) return kBody;
    if (half >= scale.half_of(c.low) && half <= scale.half_of(c.high)) return kWick;
    return kEmpty;
}

int day_of(std::span<const Candle> candles, std::size_t i) {
    return candles[i].day > 0 ? candles[i].day : static_cast<int>(i) + 1;
}

std::string summary_line(std::span<const Candle> candles, bool cents) {
    using namespace term;
    Price high = candles.front().high, low = candles.front().low;
    std::int64_t volume = 0;
    for (const Candle& c : candles) {
        high = std::max(high, c.high);
        low = std::min(low, c.low);
        volume += c.volume;
    }
    const auto field = [&](std::string_view name, Price value) {
        return paint(std::string(name) + " ", muted) + detail::price_label(value, cents) + "  ";
    };
    return field("open", candles.front().open) + field("high", high) + field("low", low) +
           field("close", candles.back().close) +
           detail::paint_change(detail::change(candles.front().open, candles.back().close)) +
           paint(std::format(" over {} days  ·  {} pairs traded", candles.size(),
                             text::thousands(volume)),
                 muted);
}

// "└┬─────────┬───" plus the labels under each tick, spaced so they never collide.
std::array<std::string, 2> day_axis(std::span<const Candle> candles, std::size_t label_width) {
    const int step = candles.size() > 40 ? 10 : 5;
    std::string ticks = term::repeat(" ", label_width + 1) + "└";
    std::string labels = term::repeat(" ", label_width + 2);
    std::size_t free_from = 0; // first column where the next label may start
    for (std::size_t i = 0; i < candles.size(); ++i) {
        const int day = day_of(candles, i);
        const bool tick = i == 0 || day % step == 0;
        const std::string label = "d" + std::to_string(day);
        if (tick && i >= free_from && i + label.size() <= candles.size() + 4) {
            ticks += "┬";
            labels += std::string(i - (term::width(labels) - label_width - 2), ' ') + label;
            free_from = i + label.size() + 1;
        } else {
            ticks += "─";
        }
    }
    return {term::paint(ticks, faint), term::paint(labels, muted)};
}

} // namespace

std::string render_candles(const std::vector<Candle>& all, const ChartOptions& options) {
    using namespace term;
    std::string out;
    if (!options.title.empty()) out += paint_bold(options.title, detail::colors::accent) + '\n';
    if (all.empty()) return out + paint("  no trading history yet", muted) + '\n';

    const auto width = static_cast<std::size_t>(std::max(options.width, 4));
    const int rows = std::max(options.height, 3);
    const std::size_t shown = std::min(all.size(), width);
    const std::span<const Candle> candles(all.data() + (all.size() - shown), shown);

    Scale scale{candles.front().low, candles.front().high, 2 * rows};
    std::int64_t max_volume = 0;
    for (const Candle& c : candles) {
        scale.lo = std::min(scale.lo, c.low);
        scale.hi = std::max(scale.hi, c.high);
        max_volume = std::max(max_volume, c.volume);
    }
    if (scale.hi == scale.lo) { // a perfectly flat history still needs a vertical range
        const Price pad = std::max<Price>(scale.hi / 50, 1);
        scale.lo -= pad;
        scale.hi += pad;
    }
    const bool cents = scale.hi - scale.lo < dollars(20);

    out += summary_line(candles, cents) + '\n';

    std::size_t label_width = 0;
    for (int row = 0; row < rows; ++row) {
        const std::string label = detail::price_label(scale.row_price(row, rows), cents);
        label_width = std::max(label_width, label.size());
    }
    label_width = std::max(label_width, text::thousands(max_volume).size());

    const Candle& last = candles.back();
    const int last_row = scale.half_of(last.close) / 2;
    const auto candle_color = [](const Candle& c) { return c.close >= c.open ? up : down; };

    for (int row = rows - 1; row >= 0; --row) {
        const bool labelled = (rows - 1 - row) % 3 == 0;
        std::string line =
            labelled ? pad_left(detail::price_label(scale.row_price(row, rows), cents), label_width)
                     : std::string(label_width, ' ');
        line = paint(line, muted) + paint(labelled ? " ┤" : " │", faint);
        for (const Candle& c : candles) {
            const Mark upper = mark_at(c, scale, 2 * row + 1);
            const Mark lower = mark_at(c, scale, 2 * row);
            const std::string_view glyph = kGlyph[upper][lower];
            line += glyph == " " ? std::string(" ") : paint(glyph, candle_color(c));
        }
        if (row == last_row)
            line += paint(" ◄ " + detail::price_label(last.close, cents), candle_color(last));
        out += line + '\n';
    }

    const int volume_rows = std::max(options.volume_height, 0);
    for (int row = volume_rows - 1; row >= 0; --row) {
        std::string label = row == volume_rows - 1 ? text::thousands(max_volume)
                            : row == 0             ? std::string("vol")
                                                   : std::string();
        std::string line = paint(pad_left(label, label_width), muted) +
                           paint(row == volume_rows - 1 ? " ┤" : " │", faint);
        for (const Candle& c : candles) {
            const double fraction =
                max_volume > 0 ? static_cast<double>(c.volume) / static_cast<double>(max_volume)
                               : 0.0;
            const auto eighths = static_cast<int>(fraction * volume_rows * 8 + 0.5);
            const int level = std::clamp(eighths - row * 8, 0, 8);
            // Keep the baseline row visible for any day with volume.
            const int shown_level = row == 0 && c.volume > 0 ? std::max(level, 1) : level;
            line += shown_level == 0 ? std::string(" ")
                                     : paint(kVolume[static_cast<std::size_t>(shown_level)],
                                             c.close >= c.open ? up_dim : down_dim);
        }
        out += line + '\n';
    }

    const auto axis = day_axis(candles, label_width);
    out += axis[0] + '\n' + axis[1] + '\n';
    return detail::trim_line_ends(out);
}

} // namespace street::kicks
