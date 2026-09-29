#include "street/core/term.hpp"
#include "street/core/text.hpp"
#include "street/kicks/bench.hpp"
#include "street/kicks/market.hpp"
#include "street/kicks/render.hpp"

#include <doctest/doctest.h>
#include <string>
#include <vector>

using namespace street::kicks;
namespace term = street::term;
namespace text = street::text;

namespace {

/// Restores the global colour setting when a test ends.
struct ColorGuard {
    bool saved = term::color_enabled();
    explicit ColorGuard(bool enabled) { term::set_color_enabled(enabled); }
    ~ColorGuard() { term::set_color_enabled(saved); }
    ColorGuard(const ColorGuard&) = delete;
    ColorGuard& operator=(const ColorGuard&) = delete;
};

std::size_t widest_line(const std::string& s) {
    std::size_t widest = 0;
    for (const std::string& line : text::split_lines(s))
        widest = std::max(widest, term::width(line));
    return widest;
}

bool contains(const std::string& haystack, std::string_view needle) {
    return haystack.find(needle) != std::string::npos;
}

std::vector<Candle> zigzag(int days) {
    std::vector<Candle> candles;
    Price close = dollars(200);
    for (int d = 1; d <= days; ++d) {
        const Price open = close;
        close = open + (d % 3 == 0 ? -dollars(6) : dollars(4));
        candles.push_back({open, std::max(open, close) + dollars(3),
                           std::min(open, close) - dollars(2), close, 10 + d, d});
    }
    return candles;
}

const Market& shared_market() {
    static const Market market = [] {
        Market m(classic_catalog(), 1234);
        m.run(40);
        return m;
    }();
    return market;
}

} // namespace

TEST_CASE("candlestick chart: axes, glyphs, volume and plain-text parity") {
    const std::vector<Candle> candles = zigzag(30);
    const ChartOptions options{40, 10, "AJ1-CHI", 3};
    std::string plain;
    {
        const ColorGuard guard(false);
        plain = render_candles(candles, options);
    }
    CHECK(contains(plain, "AJ1-CHI"));
    CHECK(contains(plain, "┃"));  // bodies
    CHECK(contains(plain, "┤"));  // labelled price axis
    CHECK(contains(plain, "└┬")); // day axis
    CHECK(contains(plain, "d1"));
    CHECK(contains(plain, "d20"));
    CHECK(contains(plain, "vol"));
    CHECK(contains(plain, "◄ $220")); // last close marker
    CHECK_FALSE(contains(plain, "\x1b["));
    CHECK(widest_line(plain) <= 100);

    const ColorGuard guard(true);
    const std::string colored = render_candles(candles, options);
    CHECK(contains(colored, "\x1b["));
    CHECK(term::strip_ansi(colored) == plain);
}

TEST_CASE("candlestick chart keeps only the latest candles that fit") {
    const ColorGuard guard(false);
    const std::string chart = render_candles(zigzag(90), {30, 8, "", 0});
    CHECK(contains(chart, "d90"));
    CHECK_FALSE(contains(chart, "d10 "));
    CHECK(contains(render_candles({}, {}), "no trading history"));
}

TEST_CASE("depth ladder: asks over bids around the spread line") {
    OrderBook book;
    book.limit(1, Side::Sell, dollars(210), 3);
    book.limit(2, Side::Sell, dollars(205), 1);
    book.limit(3, Side::Buy, dollars(200), 4);
    book.limit(4, Side::Buy, dollars(199), 2);

    const ColorGuard guard(false);
    const std::string ladder = render_depth(book, 8);
    const auto ask_far = ladder.find("$210");
    const auto ask_best = ladder.find("$205");
    const auto spread = ladder.find("spread $5");
    const auto bid_best = ladder.find("$200");
    const auto bid_far = ladder.find("$199");
    REQUIRE(ask_far != std::string::npos);
    CHECK(ask_far < ask_best);
    CHECK(ask_best < spread);
    CHECK(spread < bid_best);
    CHECK(bid_best < bid_far);
    CHECK(contains(ladder, "mid $202.50"));
    CHECK(contains(ladder, "████")); // the biggest level gets the longest bar

    CHECK(contains(render_depth(OrderBook{}), "empty"));
    OrderBook one_sided;
    one_sided.limit(1, Side::Buy, dollars(100), 1);
    CHECK(contains(render_depth(one_sided), "no asks"));
}

TEST_CASE("ticker lists every SKU within 100 columns") {
    const Market& market = shared_market();
    std::string plain;
    {
        const ColorGuard guard(false);
        plain = render_ticker(market);
    }
    for (const std::string& sku : market.skus())
        CHECK(contains(plain, sku));
    CHECK(contains(plain, "PREMIUM"));
    CHECK(contains(plain, "Chicago"));
    CHECK_FALSE(contains(plain, "\x1b["));
    CHECK(widest_line(plain) <= 100);

    const ColorGuard guard(true);
    CHECK(term::strip_ansi(render_ticker(market)) == plain);
}

TEST_CASE("news, floor and catalog renderers") {
    const ColorGuard guard(false);
    const Market& market = shared_market();
    const std::string news = render_news(market, 3);
    CHECK(text::split_lines(news).size() == std::min<std::size_t>(3, market.news().size()));
    CHECK(widest_line(news) <= 100);

    const std::string floor = render_floor(market);
    CHECK(contains(floor, "collector"));
    CHECK(contains(floor, "market maker"));
    CHECK(widest_line(floor) <= 100);

    const std::string catalog = render_catalog(classic_catalog());
    CHECK(contains(catalog, "Air Jordan 11 Retro \"Concord\""));
    CHECK(contains(catalog, "LEGENDS"));
    CHECK(contains(catalog, "Flu Game"));
    CHECK(widest_line(catalog) <= 100);
}

TEST_CASE("the ASCII sneaker is plain ASCII and compact") {
    const ColorGuard guard(false);
    const std::string art = ascii_sneaker();
    CHECK(text::split_lines(art).size() >= 8);
    CHECK(widest_line(art) <= 48);
    for (const char c : art)
        CHECK(static_cast<unsigned char>(c) < 0x80);
}

TEST_CASE("benchmark replays a deterministic flow and reports sane numbers") {
    const BenchResult a = benchmark(20'000, 7);
    const BenchResult b = benchmark(20'000, 7);
    CHECK(a.orders == 20'000);
    CHECK(a.limits + a.markets + a.cancels == a.orders);
    CHECK(a.cancels > a.orders / 4);
    CHECK(a.trades > 0);
    CHECK(a.trades == b.trades); // same flow, same matching
    CHECK(a.resting == b.resting);
    CHECK(a.seconds > 0.0);
    CHECK(a.orders_per_second > 0.0);
    CHECK(a.p50_ns <= a.p99_ns);
    CHECK(a.p99_ns <= a.p999_ns);

    const ColorGuard guard(false);
    const std::string panel = render_bench(a);
    CHECK(contains(panel, "orders/s"));
    CHECK(contains(panel, "p99"));
}
