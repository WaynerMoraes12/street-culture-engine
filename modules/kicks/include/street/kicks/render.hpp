#pragma once
// Text renderers for the exchange.
//
// All renderers are pure functions that return strings; callers decide where to print. With
// colours disabled (NO_COLOR, piped output, term::set_color_enabled(false)) they emit clean,
// aligned plain text, and the coloured output is exactly the plain text plus ANSI styling.

#include "street/kicks/catalog.hpp"
#include "street/kicks/market.hpp"
#include "street/kicks/order_book.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace street::kicks {

struct ChartOptions {
    int width = 60;  ///< max candles shown (one column each); the latest ones win
    int height = 14; ///< price rows (each row resolves two price bands)
    std::string title;
    int volume_height = 3; ///< rows of volume bars under the price pane (0 hides them)
};

/// ASCII candlestick chart: bodies and wicks at half-row resolution, green up / red down
/// candles, price axis labels, volume bars underneath and a day axis.
std::string render_candles(const std::vector<Candle>& candles, const ChartOptions& options = {});

/// Depth ladder: asks (red) on top, the spread line, bids (green) below, with bars
/// proportional to the size resting at each level.
std::string render_depth(const OrderBook& book, std::size_t levels = 8);

/// Market board: SKU, colorway, retail, last, 1d %, 7d %, premium over retail, a 30-day
/// sparkline and a hype meter for every listed sneaker.
std::string render_ticker(const Market& market);

/// The headlines with the largest hype impact, each with the price reaction that followed.
std::string render_news(const Market& market, std::size_t limit = 5);

/// The catalog as a table: SKU, sneaker, year, retail, pairs, base hype.
std::string render_catalog(const std::vector<Sneaker>& catalog);

/// Who is winning: holdings, cash and mark-to-market P&L per trader persona.
std::string render_floor(const Market& market);

/// A high-top in ASCII art (box-drawing and ASCII only), for command headers.
std::string ascii_sneaker();

} // namespace street::kicks
