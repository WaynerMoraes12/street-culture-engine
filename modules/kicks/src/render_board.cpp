// Tabular renderers: the ticker board, top headlines, the floor P&L and the catalog.

#include "format.hpp"
#include "street/core/table.hpp"
#include "street/core/term.hpp"
#include "street/core/text.hpp"
#include "street/kicks/render.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <optional>

namespace street::kicks {
namespace {

using detail::colors::accent;
using detail::colors::faint;
using detail::colors::muted;

/// Borderless, column-aligned board (an exchange ticker reads better without box lines, and
/// dropping them buys the room for a 30-day sparkline within 100 columns).
class Board {
public:
    explicit Board(std::vector<std::string> headers, std::vector<Align> aligns)
        : headers_(std::move(headers)), aligns_(std::move(aligns)) {}

    void add(std::vector<std::string> cells) { rows_.push_back(std::move(cells)); }

    std::string render() const {
        std::vector<std::size_t> widths(headers_.size());
        for (std::size_t c = 0; c < headers_.size(); ++c)
            widths[c] = term::width(headers_[c]);
        for (const auto& row : rows_)
            for (std::size_t c = 0; c < row.size() && c < widths.size(); ++c)
                widths[c] = std::max(widths[c], term::width(row[c]));

        const auto line = [&](const std::vector<std::string>& cells, bool header) {
            std::string out = " ";
            for (std::size_t c = 0; c < widths.size(); ++c) {
                const std::string raw = c < cells.size() ? cells[c] : std::string();
                const std::string cell = header ? term::paint(raw, muted) : raw;
                out += ' ';
                out += aligns_[c] == Align::Right ? term::pad_left(cell, widths[c])
                                                  : term::pad_right(cell, widths[c]);
                out += ' ';
            }
            return out + '\n';
        };

        std::size_t total = 1;
        for (const std::size_t w : widths)
            total += w + 2;
        std::string out = line(headers_, true);
        out += "  " + term::paint(term::repeat("─", total - 2), faint) + '\n';
        for (const auto& row : rows_)
            out += line(row, false);
        return detail::trim_line_ends(out);
    }

private:
    std::vector<std::string> headers_;
    std::vector<Align> aligns_;
    std::vector<std::vector<std::string>> rows_;
};

/// 80000 -> "80K", 2500000 -> "2.5M": production runs are estimates, so keep them coarse.
std::string compact_count(std::int64_t n) {
    if (n >= 1'000'000) {
        const std::string m = text::fixed(static_cast<double>(n) / 1e6, n % 1'000'000 == 0 ? 0 : 1);
        return m + "M";
    }
    if (n >= 1'000) return std::to_string(n / 1'000) + "K";
    return std::to_string(n);
}

/// Hype temperature: cold cyan -> gold -> hot red.
term::Rgb heat_color(double level) {
    constexpr std::array<term::Rgb, 3> stops = {term::Rgb{34, 211, 238}, term::palette::gold,
                                                term::palette::red};
    const double t = std::clamp(level, 0.0, 1.0) * 2.0;
    return t < 1.0 ? term::lerp(stops[0], stops[1], t) : term::lerp(stops[1], stops[2], t - 1.0);
}

/// Sparkline of the last 30 closes squeezed into 15 cells (each cell averages two days).
std::string sparkline_30d(const std::vector<Candle>& candles) {
    const std::size_t n = std::min<std::size_t>(candles.size(), 30);
    if (n < 2) return term::paint("--", faint);
    std::vector<double> closes;
    for (std::size_t i = candles.size() - n; i < candles.size(); ++i)
        closes.push_back(static_cast<double>(candles[i].close));
    std::vector<double> cells;
    std::size_t i = 0;
    if (closes.size() % 2 == 1) cells.push_back(closes[i++]); // odd count: oldest day alone
    for (; i + 1 < closes.size(); i += 2)
        cells.push_back((closes[i] + closes[i + 1]) / 2.0);
    const bool rising = closes.back() >= closes.front();
    return term::paint(term::sparkline(cells), rising ? detail::colors::up : detail::colors::down);
}

std::optional<double> change_over(const std::vector<Candle>& candles, std::size_t days) {
    if (candles.size() < days + 1) return std::nullopt;
    return detail::change(candles[candles.size() - 1 - days].close, candles.back().close);
}

/// Close on a given trading day, or the nearest earlier one; nullopt before the history.
std::optional<Price> close_on(const std::vector<Candle>& candles, int day) {
    std::optional<Price> found;
    for (const Candle& c : candles) {
        if (c.day > day) break;
        found = c.close;
    }
    return found;
}

} // namespace

std::string render_ticker(const Market& market) {
    using namespace term;
    Board board({"SKU", "SNEAKER", "RETAIL", "LAST", "1D", "7D", "PREMIUM", "30D", "HYPE"},
                {Align::Left, Align::Left, Align::Right, Align::Right, Align::Right, Align::Right,
                 Align::Right, Align::Left, Align::Left});
    for (const std::string& sku : market.skus()) {
        const Sneaker& sneaker = market.sneaker(sku);
        const auto& candles = market.candles(sku);
        const Price last = market.last_price(sku);
        const double hype = market.hype(sku);
        const std::string name = sneaker.colorway.empty() ? sneaker.name : sneaker.colorway;
        board.add(
            {paint_bold(sku, accent), truncate(name, 14),
             paint(detail::dollar_label(sneaker.retail), muted),
             emphasize(detail::dollar_label(last)), detail::paint_change(change_over(candles, 1)),
             detail::paint_change(change_over(candles, 7)),
             detail::paint_change(detail::change(sneaker.retail, last), 0), sparkline_30d(candles),
             paint(meter(hype, 6), heat_color(hype)) + ' ' +
                 paint(pad_left(std::to_string(std::lround(hype * 100)), 2), muted)});
    }
    return board.render();
}

std::string render_news(const Market& market, std::size_t limit) {
    using namespace term;
    std::vector<const News*> top;
    for (const News& item : market.news())
        top.push_back(&item);
    std::stable_sort(top.begin(), top.end(), [](const News* a, const News* b) {
        return std::abs(a->impact) > std::abs(b->impact);
    });
    if (top.size() > limit) top.resize(limit);
    if (top.empty()) return paint("  a quiet tape: no headlines yet", muted) + '\n';

    std::string out;
    for (const News* item : top) {
        std::string reaction = paint("pending", faint);
        if (market.has(item->sku)) {
            const auto& candles = market.candles(item->sku);
            const auto before = item->day > 1 ? close_on(candles, item->day - 1) : std::nullopt;
            const Price base = before ? *before : (candles.empty() ? 0 : candles.front().open);
            const int until = std::min(item->day + 2, market.day());
            if (until >= item->day && base > 0) {
                if (const auto after = close_on(candles, until))
                    reaction = detail::paint_change(detail::change(base, *after)) +
                               paint(std::format(" in {}d", until - item->day + 1), muted);
            }
        }
        const term::Rgb tone = item->impact >= 0 ? detail::colors::up : detail::colors::down;
        out += "  " + paint(pad_left("d" + std::to_string(item->day), 4), muted) + "  " +
               paint_bold(pad_right(item->sku, 8), accent) + "  " +
               paint(pad_left(text::signed_percent(item->impact, 0), 4), tone) + "  " +
               pad_right(truncate(item->headline, 54), 54) + "  " + reaction + '\n';
    }
    return detail::trim_line_ends(out);
}

std::string render_floor(const Market& market) {
    using namespace term;
    Table table({"PERSONA", "TRADERS", "PAIRS", "CASH", "P&L", "PLAYBOOK"});
    table.align_right_from(1).align(5, Align::Left).accent(detail::colors::faint);
    for (const PersonaStats& stats : market.persona_stats()) {
        std::string playbook;
        switch (stats.persona) {
        case Persona::Collector:
            playbook = "buy under value, hold for years";
            break;
        case Persona::Reseller:
            playbook = "flip drops at a markup, cut if stale";
            break;
        case Persona::Hypebeast:
            playbook = "chase heat, panic on dips";
            break;
        case Persona::MarketMaker:
            playbook = "quote both sides, lean on inventory";
            break;
        }
        const std::string pnl = (stats.pnl >= 0 ? "+" : "") + detail::dollar_label(stats.pnl);
        table.add_row({emphasize(std::string(persona_name(stats.persona))),
                       std::to_string(stats.traders), text::thousands(stats.pairs),
                       detail::dollar_label(stats.cash),
                       paint(pnl, stats.pnl >= 0 ? detail::colors::up : detail::colors::down),
                       paint(playbook, muted)});
    }
    return table.render();
}

std::string render_catalog(const std::vector<Sneaker>& catalog) {
    using namespace term;
    Table table({"SKU", "SNEAKER", "YEAR", "RETAIL", "PAIRS", "RESALE", "HYPE"});
    table.align_right_from(2).align(6, Align::Left).accent(detail::colors::faint);
    for (const Sneaker& sneaker : catalog) {
        const auto resale = static_cast<Price>(static_cast<double>(sneaker.retail) *
                                               resale_multiplier(sneaker.base_hype));
        table.add_row(
            {paint_bold(sneaker.sku, accent), sneaker.display_name(), std::to_string(sneaker.year),
             detail::dollar_label(sneaker.retail), compact_count(sneaker.pairs),
             paint("~" + detail::dollar_label(resale), muted),
             paint(meter(sneaker.base_hype, 8), heat_color(sneaker.base_hype)) + ' ' +
                 paint(pad_left(std::to_string(std::lround(sneaker.base_hype * 100)), 2), muted)});
    }
    std::string out = table.render();
    std::string stories;
    for (const Sneaker& sneaker : catalog) {
        if (sneaker.story.empty()) continue;
        stories += "  " + paint_bold(pad_right(sneaker.sku, 10), accent) +
                   paint(truncate(sneaker.story, 85), muted) + '\n';
    }
    if (!stories.empty()) out += '\n' + paint_bold("  LEGENDS", accent) + '\n' + stories;
    return out;
}

} // namespace street::kicks
