#include "format.hpp"
#include "street/core/term.hpp"
#include "street/core/text.hpp"
#include "street/kicks/render.hpp"

#include <algorithm>
#include <format>

namespace street::kicks {
namespace {

constexpr std::size_t kBarWidth = 30;

struct Columns {
    std::size_t price = 5, size = 4, orders = 6;
};

std::string ladder_row(std::string_view tag, const Level& level, const Columns& cols,
                       std::int64_t max_quantity, term::Rgb color, term::Rgb bar_color) {
    using namespace term;
    const double fraction =
        max_quantity > 0 ? static_cast<double>(level.quantity) / static_cast<double>(max_quantity)
                         : 0.0;
    std::string bar = meter(fraction, kBarWidth);
    bar.erase(bar.find_last_not_of(' ') + 1);
    return "  " + paint(pad_right(tag, 4), color) +
           paint_bold(pad_left(detail::dollar_label(level.price), cols.price), color) + "  " +
           pad_left(text::thousands(level.quantity), cols.size) + "  " +
           paint(pad_left(std::to_string(level.orders), cols.orders), detail::colors::muted) +
           "  " + paint(bar, bar_color) + '\n';
}

} // namespace

std::string render_depth(const OrderBook& book, std::size_t levels) {
    using namespace term;
    using detail::colors::down;
    using detail::colors::muted;
    using detail::colors::up;

    std::vector<Level> asks = book.depth(Side::Sell, levels);
    const std::vector<Level> bids = book.depth(Side::Buy, levels);
    if (asks.empty() && bids.empty()) return paint("  the order book is empty", muted) + '\n';
    std::reverse(asks.begin(), asks.end()); // farthest ask on top, best ask next to the spread

    Columns cols;
    std::int64_t max_quantity = 0;
    const auto measure = [&](const std::vector<Level>& side) {
        for (const Level& level : side) {
            cols.price = std::max(cols.price, detail::dollar_label(level.price).size());
            cols.size = std::max(cols.size, text::thousands(level.quantity).size());
            max_quantity = std::max(max_quantity, level.quantity);
        }
    };
    measure(asks);
    measure(bids);

    std::string out =
        "  " +
        paint(pad_right("", 4) + pad_left("PRICE", cols.price) + "  " +
                  pad_left("SIZE", cols.size) + "  " + pad_left("ORDERS", cols.orders) + "  DEPTH",
              muted) +
        '\n';
    if (asks.empty()) out += paint("  (no asks)", muted) + '\n';
    for (const Level& level : asks)
        out += ladder_row("ask", level, cols, max_quantity, down, detail::colors::down_dim);

    const std::size_t row_width = 4 + cols.price + 2 + cols.size + 2 + cols.orders + 2 + kBarWidth;
    std::string middle;
    if (const auto spread = book.spread()) {
        const Price bid = *book.best_bid();
        const Price ask = *book.best_ask();
        const double pct = static_cast<double>(*spread) / (static_cast<double>(bid + ask) / 2.0);
        middle = std::format("spread {} ({})  ·  mid {}", detail::dollar_label(*spread),
                             text::percent(pct, 2), text::money((bid + ask) / 2));
    } else {
        middle = "one-sided book";
    }
    const std::size_t dashes = row_width > width(middle) + 6 ? row_width - width(middle) - 6 : 2;
    out += "  " + paint(repeat("─", dashes / 2) + " ", detail::colors::faint) + emphasize(middle) +
           paint(" " + repeat("─", dashes - dashes / 2), detail::colors::faint) + '\n';

    for (const Level& level : bids)
        out += ladder_row("bid", level, cols, max_quantity, up, detail::colors::up_dim);
    if (bids.empty()) out += paint("  (no bids)", muted) + '\n';
    return detail::trim_line_ends(out);
}

} // namespace street::kicks
