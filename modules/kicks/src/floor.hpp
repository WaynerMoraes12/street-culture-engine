#pragma once
// Internal: the trading floor. Traders, per-SKU listings, and settlement of fills.
//
// Traders may only commit what they own: a resting ask reserves pairs, a resting bid reserves
// cash, and fills release exactly what they consumed. Because trades print at the resting
// order's price, a filled bid always releases price * quantity of its own reservation, which
// keeps the bookkeeping exact without ever re-scanning the book.

#include "street/core/rng.hpp"
#include "street/kicks/catalog.hpp"
#include "street/kicks/hype.hpp"
#include "street/kicks/market.hpp"
#include "street/kicks/order_book.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace street::kicks::detail {

inline constexpr Price kTick = 100;          ///< sneakers trade in whole dollars
inline constexpr std::size_t kTapeSize = 64; ///< trades kept per SKU for recent_trades()

/// Rounds (fractional) cents to the $1 grid, never below $1.
Price to_tick(double cents) noexcept;

/// Pairs floating on the simulated secondary market for a production run: sub-linear, so a
/// ten-million-pair general release does not drown the floor.
std::int32_t simulated_float(int pairs) noexcept;

struct Holding {
    std::int32_t pairs = 0;     ///< owned
    std::int32_t listed = 0;    ///< reserved by resting asks
    Price cost = 0;             ///< average cost basis per pair
    OrderId order = 0;          ///< the trader's working order in this SKU (0 = none)
    std::int64_t posted_at = 0; ///< floor clock when `order` was posted

    std::int32_t free_pairs() const noexcept { return pairs - listed; }
};

struct Trader {
    TraderId id = 0;
    Persona persona = Persona::Collector;
    Price cash = 0;
    Price committed = 0; ///< cash reserved by resting bids
    Price start_equity = 0;
    double temperament = 0.5;      ///< persona knob in [0, 1): markup, FOMO threshold, ...
    std::int32_t patience = 0;     ///< ticks a reseller waits before cutting an unsold ask
    std::int32_t appetite = 1;     ///< pairs per SKU a collector wants to keep
    std::vector<Holding> holdings; ///< one per listing

    Price free_cash() const noexcept { return cash - committed; }
};

struct Maker {
    std::uint32_t trader = 0; ///< index into Floor::traders
    OrderId bid = 0;
    OrderId ask = 0;
    double quoted_fair = 0.0; ///< fair value the live quotes were built around
    std::int32_t target = 0;  ///< inventory the maker hovers around
    std::int64_t refresh_at = 0;
};

/// Today's running OHLCV tally.
struct Session {
    Price open = 0, high = 0, low = 0, close = 0;
    std::int64_t volume = 0;
    bool traded = false;
};

struct Listing {
    Sneaker sneaker;
    std::size_t index = 0;
    OrderBook book;
    HypeProcess hype;
    Price anchor = 0;  ///< long-run fair value: retail * resale_multiplier(base_hype)
    Price last = 0;    ///< last trade (opening reference before the first one)
    double fast = 0.0; ///< short EMA of the last price, for momentum
    double slow = 0.0; ///< long EMA of the last price
    double buzz = 0.0; ///< headline excitement in [-1, 1]; fades day by day
    Session session;
    std::vector<Candle> candles;
    std::vector<Trade> tape; ///< recent trades (bounded, see record())
    Maker maker;
    Rng hype_rng; ///< fundamentals: hype noise and news, independent of trading
    Rng flow_rng; ///< order flow: arrivals and agent decisions

    double fair_value() const noexcept { return static_cast<double>(anchor) * hype.multiplier(); }
    double momentum() const noexcept { return slow > 0.0 ? fast / slow - 1.0 : 0.0; }
    /// What hypebeasts feel: price momentum plus headline buzz.
    double heat() const noexcept { return 6.0 * momentum() + buzz; }

    void open_session() noexcept;
    Candle close_session(int day);
    void record(const Trade& trade);
};

class Floor {
public:
    std::vector<Trader> traders;
    std::vector<Listing> listings;
    std::int64_t clock = 0; ///< ticks since the market opened

    /// Limit order on behalf of a trader. Fills settle immediately; any remainder rests and
    /// reserves the trader's cash (bids) or pairs (asks). The caller must not over-commit.
    OrderResult submit(Listing& listing, Trader& trader, Side side, Price price,
                       std::int32_t quantity);
    /// Immediate-or-cancel: trades up to `quantity` at `limit` or better, pulls the rest.
    std::int32_t take(Listing& listing, Trader& trader, Side side, Price limit,
                      std::int32_t quantity);
    /// Cancels `id` if it is still resting, releases its reservation, and zeroes `id`.
    void cancel(Listing& listing, Trader& trader, OrderId& id);

private:
    void settle(Listing& listing, std::span<const Trade> trades);
};

} // namespace street::kicks::detail
