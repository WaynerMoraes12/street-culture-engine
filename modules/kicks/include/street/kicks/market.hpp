#pragma once
// An agent-based sneaker resale exchange.
//
// Every SKU has its own OrderBook, a HypeProcess that sets its fair value, and a daily OHLCV
// candle history. A floor of simulated traders works the books:
//
//   collectors    value buyers: bid below what a pair is worth to them, hold for the long run
//   resellers     win raffles, list at a markup and cut the price when a pair will not move
//   hypebeasts    chase momentum and headlines; they overpay when hype spikes, panic on dips
//   market makers quote both sides around fair value, skewing quotes against their inventory
//
// Pairs and cash are conserved: every trade moves them between traders, only a release
// brings new pairs in. The simulation is fully deterministic for a given seed and sequence of
// calls; each SKU draws hype and order flow from its own random streams, so a shock to one
// sneaker leaves the fundamentals of the others untouched.

#include "street/kicks/catalog.hpp"
#include "street/kicks/order_book.hpp"
#include "street/kicks/types.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace street::kicks {

/// One trading day of one SKU. A day without trades is flat at the previous close.
struct Candle {
    Price open = 0, high = 0, low = 0, close = 0;
    std::int64_t volume = 0; ///< pairs traded
    int day = 0;             ///< trading day, 1-based

    friend bool operator==(const Candle&, const Candle&) = default;
};

/// A headline that moved (or was meant to move) a sneaker's hype.
struct News {
    int day = 0; ///< first trading day whose session reflects the news
    std::string sku;
    std::string headline;
    double impact = 0.0; ///< relative fair-value shock, e.g. +0.25

    friend bool operator==(const News&, const News&) = default;
};

enum class Persona : std::uint8_t { Collector, Reseller, Hypebeast, MarketMaker };
std::string_view persona_name(Persona persona) noexcept;

/// Aggregate standing of one persona on the floor, marked to the last traded prices.
struct PersonaStats {
    Persona persona = Persona::Collector;
    std::size_t traders = 0;
    std::int64_t pairs = 0; ///< pairs held across every SKU
    Price cash = 0;
    Price equity = 0; ///< cash + pairs at last price
    Price pnl = 0;    ///< equity minus starting equity (raffle purchases included)
};

struct MarketConfig {
    int ticks_per_day = 240; ///< trading rounds per day; every SKU is visited each tick
    int collectors = 70;
    int resellers = 50;
    int hypebeasts = 80;   ///< plus one market maker per SKU
    double activity = 1.0; ///< scales how often traders show up
};

class Market {
public:
    /// Lists every catalog sneaker as an already-released shoe with an open secondary market.
    explicit Market(const std::vector<Sneaker>& catalog = classic_catalog(), std::uint64_t seed = 1,
                    MarketConfig config = {});
    Market(Market&&) noexcept;
    Market& operator=(Market&&) noexcept;
    Market(const Market&) = delete;
    Market& operator=(const Market&) = delete;
    ~Market();

    /// A drop: the pairs are raffled at retail (resellers enter with more tickets), then the
    /// secondary market opens with launch-day buzz. Throws std::invalid_argument for a duplicate
    /// SKU, an empty SKU or a non-positive retail price / pair count.
    void release(const Sneaker& sneaker);
    /// Hype shock of relative magnitude (+0.25: fair value jumps ~25% before mean reversion;
    /// must be > -1). Logged in news(). Throws std::out_of_range for an unknown SKU and
    /// std::invalid_argument for a magnitude <= -1.
    void shock(std::string_view sku, double magnitude, std::string headline);
    /// Runs one trading day (news, ticks_per_day rounds of agent activity across every SKU)
    /// and closes one candle per SKU.
    void step_day();
    void run(int days);

    int day() const noexcept; ///< trading days completed
    std::uint64_t seed() const noexcept;
    std::vector<std::string> skus() const;
    bool has(std::string_view sku) const noexcept;

    // Per-SKU queries. SKUs match case-insensitively; unknown SKUs throw std::out_of_range.
    const Sneaker& sneaker(std::string_view sku) const;
    const std::vector<Candle>& candles(std::string_view sku) const;
    Price last_price(std::string_view sku) const; ///< last trade (opening reference before)
    Price fair_value(std::string_view sku) const;
    double hype(std::string_view sku) const; ///< hype index in (0, 1)
    const OrderBook& book(std::string_view sku) const;
    std::int64_t supply(std::string_view sku) const; ///< pairs held by all traders
    /// The most recent trades (up to 64), oldest first.
    std::vector<Trade> recent_trades(std::string_view sku) const;

    const std::vector<News>& news() const noexcept;
    std::vector<PersonaStats> persona_stats() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace street::kicks
