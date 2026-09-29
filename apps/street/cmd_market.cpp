// street market — the kicks engine: a simulated sneaker resale exchange.

#include "commands.hpp"
#include "street/core/cli.hpp"
#include "street/core/rng.hpp"
#include "street/core/stopwatch.hpp"
#include "street/core/term.hpp"
#include "street/core/text.hpp"
#include "street/kicks/kicks.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <iostream>
#include <string>
#include <vector>

namespace street::app {
namespace {

using namespace street::kicks;

constexpr term::Rgb kAccent = term::palette::cyan;
constexpr std::uint64_t kDefaultBenchOrders = 1'000'000;
constexpr int kMaxDays = 2000;

cli::Command market_command() {
    cli::Command cmd{"market", "Trade sneakers on a simulated order-book resale exchange"};
    cmd.option("days", "N", "Trading days to simulate", "60")
        .option("sku", "SKU", "Sneaker to feature in the chart and ladder (default: biggest mover)")
        .option("seed", "N", "Random seed (default: random; always printed)")
        .flag("catalog", "List the sneaker catalog and exit")
        .flag("depth", "Also show the featured sneaker's order-book ladder")
        .option("bench", "N",
                "Benchmark the matching engine with N orders and exit (bare --bench: 1M)")
        .example("street market", "60 trading days of the classic catalog")
        .example("street market --days 120 --sku AJ11-CON --depth --seed 7",
                 "feature the Concord 11 and show its order book")
        .example("street market --catalog", "the sneakers on the exchange")
        .example("street market --bench", "matching-engine throughput and latency");
    return cmd;
}

/// Lets `--bench` stand alone: a missing value becomes the default order count.
std::vector<std::string> with_bench_default(Args args) {
    std::vector<std::string> out(args.begin(), args.end());
    for (std::size_t i = 0; i < out.size(); ++i) {
        const bool needs_value = i + 1 == out.size() || out[i + 1].starts_with("--");
        if (out[i] == "--bench" && needs_value)
            out.insert(out.begin() + static_cast<std::ptrdiff_t>(i) + 1,
                       std::to_string(kDefaultBenchOrders));
    }
    return out;
}

std::string section(std::string_view title) {
    return '\n' +
           term::rule(static_cast<std::size_t>(std::min(term::columns(), 100) - 2), title,
                      term::palette::dark) +
           '\n';
}

/// Close-to-close change over the whole run (open of day 1 to the last close).
double run_change(const Market& market, const std::string& sku) {
    const auto& candles = market.candles(sku);
    if (candles.empty() || candles.front().open <= 0) return 0.0;
    return static_cast<double>(candles.back().close) / static_cast<double>(candles.front().open) -
           1.0;
}

std::string biggest_mover(const Market& market) {
    const auto skus = market.skus();
    return *std::max_element(skus.begin(), skus.end(), [&](const auto& a, const auto& b) {
        return std::abs(run_change(market, a)) < std::abs(run_change(market, b));
    });
}

std::string hottest(const Market& market) {
    const auto skus = market.skus();
    return *std::max_element(skus.begin(), skus.end(), [&](const auto& a, const auto& b) {
        return market.hype(a) < market.hype(b);
    });
}

void print_header(const Market& market, std::uint64_t seed, double seconds) {
    using namespace term;
    std::int64_t pairs = 0;
    for (const std::string& sku : market.skus())
        for (const Candle& c : market.candles(sku))
            pairs += c.volume;

    const std::string mover = biggest_mover(market);
    const std::string hot = hottest(market);
    const double move = run_change(market, mover);
    const auto label = [](std::string_view s) { return paint(pad_right(s, 15), palette::gray); };

    const std::vector<std::string> info = {
        "",
        gradient("STREET KICKS EXCHANGE", palette::red, palette::white),
        paint("sneaker resale  ·  price-time priority order books", palette::gray),
        "",
        label("listed") + std::to_string(market.skus().size()) + " SKUs",
        label("simulated") +
            std::format("{} trading days in {:.0f} ms", market.day(), seconds * 1e3),
        label("volume") + text::thousands(pairs) + " pairs",
        label("biggest mover") + paint_bold(mover, kAccent) + "  " +
            paint(text::signed_percent(move), move >= 0 ? palette::green : palette::red),
        label("hottest") + paint_bold(hot, kAccent) + "  hype " +
            std::to_string(std::lround(market.hype(hot) * 100)),
        label("seed") + emphasize(std::to_string(seed)),
    };
    const auto art = text::split_lines(ascii_sneaker());
    const std::size_t art_width = 42;
    for (std::size_t i = 0; i < std::max(art.size(), info.size()); ++i) {
        const std::string left = i < art.size() ? art[i] : std::string();
        const std::string right = i < info.size() ? info[i] : std::string();
        std::string line = " " + pad_right(left, art_width) + "   " + right;
        line.erase(line.find_last_not_of(' ') + 1);
        std::cout << line << '\n';
    }
}

std::string resolve_sku(std::string_view requested) {
    const Sneaker* sneaker = find_sneaker(classic_catalog(), requested);
    if (sneaker == nullptr) {
        std::vector<std::string> skus;
        for (const Sneaker& s : classic_catalog())
            skus.push_back(s.sku);
        throw cli::UsageError(
            std::format("unknown SKU '{}' (try one of: {})", requested, text::join(skus, ", ")));
    }
    return sneaker->sku;
}

int run_bench(std::uint64_t orders, std::uint64_t seed) {
    if (orders < 1'000 || orders > 50'000'000)
        throw cli::UsageError("--bench expects between 1,000 and 50,000,000 orders");
    std::cout << section("MATCHING ENGINE") << term::paint("  replaying ", term::palette::gray)
              << text::thousands(static_cast<std::int64_t>(orders))
              << term::paint(" synthetic orders (seed " + std::to_string(seed) + ")...\n",
                             term::palette::gray);
    const BenchResult result = benchmark(static_cast<std::size_t>(orders), seed);
    std::cout << render_bench(result);
    return 0;
}

} // namespace

int cmd_market(Args args) {
    const cli::Command cmd = market_command();
    const std::vector<std::string> tokens = with_bench_default(args);
    const cli::Parsed parsed = cmd.parse(tokens);
    if (parsed.flag("help")) {
        std::cout << cmd.help();
        return 0;
    }
    if (!parsed.positional().empty())
        throw cli::UsageError("unexpected argument '" + parsed.positional().front() + "'");

    const std::uint64_t seed =
        parsed.given("seed") ? parsed.unsigned_integer("seed") : street::random_seed();
    const long long days = parsed.integer("days");
    if (days < 1 || days > kMaxDays)
        throw cli::UsageError(std::format("--days must be between 1 and {}", kMaxDays));
    const std::string featured_request = parsed.given("sku") ? resolve_sku(parsed.str("sku")) : "";

    if (parsed.flag("catalog") || parsed.given("bench")) {
        if (parsed.flag("catalog"))
            std::cout << section("THE CATALOG") << render_catalog(classic_catalog());
        if (parsed.given("bench")) run_bench(parsed.unsigned_integer("bench"), seed);
        return 0;
    }

    const Stopwatch watch;
    Market market(classic_catalog(), seed);
    market.run(static_cast<int>(days));
    const double seconds = watch.seconds();

    const std::string featured =
        featured_request.empty() ? biggest_mover(market) : featured_request;
    const Sneaker& shoe = market.sneaker(featured);

    std::cout << '\n';
    print_header(market, seed, seconds);

    std::cout << section(std::format("MARKET BOARD  ·  day {}", market.day()))
              << render_ticker(market);

    ChartOptions chart;
    chart.width = std::clamp(term::columns() - 24, 30, 76);
    chart.height = 14;
    chart.title = featured + "  " + shoe.display_name() + "  ·  retail $" +
                  text::thousands(shoe.retail / 100);
    std::cout << section("FEATURED") << render_candles(market.candles(featured), chart);

    std::cout << section("HEADLINES THAT MOVED THE MARKET") << render_news(market, 6);
    std::cout << section("THE FLOOR") << render_floor(market);

    if (parsed.flag("depth"))
        std::cout << section("ORDER BOOK  ·  " + featured)
                  << render_depth(market.book(featured), 8);

    std::cout << '\n'
              << term::paint("  seed " + std::to_string(seed) + "  ·  replay: ",
                             term::palette::gray)
              << std::format("street market --seed {} --days {}", seed, days)
              << (featured_request.empty() ? "" : " --sku " + featured) << "\n\n";
    return 0;
}

} // namespace street::app
