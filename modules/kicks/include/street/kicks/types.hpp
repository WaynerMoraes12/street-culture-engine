#pragma once
// Vocabulary types shared by the matching engine and the market simulation.

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace street::kicks {

/// Money in integer cents. Floating point never enters the matching engine, so price levels
/// compare exactly and there is no rounding drift across millions of fills.
using Price = std::int64_t;
/// Opaque order handle issued by an OrderBook. 0 is never a valid id.
using OrderId = std::uint64_t;
using TraderId = std::uint32_t;

enum class Side : std::uint8_t { Buy, Sell };

constexpr Side opposite(Side side) noexcept {
    return side == Side::Buy ? Side::Sell : Side::Buy;
}
constexpr std::string_view side_name(Side side) noexcept {
    return side == Side::Buy ? "buy" : "sell";
}

/// Whole dollars to cents: dollars(170) == 17000.
constexpr Price dollars(std::int64_t whole) noexcept {
    return whole * 100;
}

/// One execution. A trade always prints at the resting (maker) order's price, so the incoming
/// order gets any price improvement the book offers.
struct Trade {
    OrderId buy_order = 0;
    OrderId sell_order = 0;
    TraderId buyer = 0;
    TraderId seller = 0;
    Price price = 0;
    std::int32_t quantity = 0;
    std::uint64_t sequence = 0; ///< position in the book's event stream (see OrderBook)
    Side aggressor = Side::Buy; ///< side of the incoming (taker) order
};

/// Aggregated view of one price level: the L2 market-data unit.
struct Level {
    Price price = 0;
    std::int64_t quantity = 0;
    std::size_t orders = 0;

    friend bool operator==(const Level&, const Level&) = default;
};

} // namespace street::kicks
