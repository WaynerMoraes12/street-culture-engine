#include "floor.hpp"

#include <algorithm>
#include <cmath>

namespace street::kicks::detail {

Price to_tick(double cents) noexcept {
    if (!(cents > 0.0)) return kTick; // also catches NaN
    const auto ticks = static_cast<Price>(std::llround(cents / static_cast<double>(kTick)));
    return std::max<Price>(ticks, 1) * kTick;
}

std::int32_t simulated_float(int pairs) noexcept {
    const double scaled = std::sqrt(static_cast<double>(std::max(pairs, 0))) / 6.0;
    return static_cast<std::int32_t>(std::lround(std::clamp(scaled, 40.0, 180.0)));
}

// ── Listing ──────────────────────────────────────────────────────────────────

void Listing::open_session() noexcept {
    session = Session{last, last, last, last, 0, false};
}

Candle Listing::close_session(int day) {
    const Candle candle{session.open,  session.high,   session.low,
                        session.close, session.volume, day};
    candles.push_back(candle);
    return candle;
}

void Listing::record(const Trade& trade) {
    last = trade.price;
    if (!session.traded) {
        session.open = session.high = session.low = trade.price;
        session.traded = true;
    }
    session.high = std::max(session.high, trade.price);
    session.low = std::min(session.low, trade.price);
    session.close = trade.price;
    session.volume += trade.quantity;

    // Bounded tape with amortised O(1) trimming: drop the oldest half once it doubles.
    tape.push_back(trade);
    if (tape.size() >= 2 * kTapeSize)
        tape.erase(tape.begin(), tape.begin() + static_cast<std::ptrdiff_t>(kTapeSize));
}

// ── Floor ────────────────────────────────────────────────────────────────────

OrderResult Floor::submit(Listing& listing, Trader& trader, Side side, Price price,
                          std::int32_t quantity) {
    const OrderResult result = listing.book.limit(trader.id, side, price, quantity);
    settle(listing, result.trades);
    if (result.resting > 0) {
        if (side == Side::Buy)
            trader.committed += price * result.resting;
        else
            trader.holdings[listing.index].listed += result.resting;
    }
    return result;
}

std::int32_t Floor::take(Listing& listing, Trader& trader, Side side, Price limit,
                         std::int32_t quantity) {
    const OrderResult result = submit(listing, trader, side, limit, quantity);
    const std::int32_t filled = result.filled;
    if (result.resting > 0) {
        OrderId id = result.id;
        cancel(listing, trader, id);
    }
    return filled;
}

void Floor::cancel(Listing& listing, Trader& trader, OrderId& id) {
    if (id == 0) return;
    if (const auto order = listing.book.order(id)) {
        if (order->side == Side::Buy)
            trader.committed -= order->price * order->remaining;
        else
            trader.holdings[listing.index].listed -= order->remaining;
        listing.book.cancel(id);
    }
    id = 0;
}

void Floor::settle(Listing& listing, std::span<const Trade> trades) {
    for (const Trade& trade : trades) {
        Trader& buyer = traders[trade.buyer];
        Trader& seller = traders[trade.seller];
        Holding& bought = buyer.holdings[listing.index];
        Holding& sold = seller.holdings[listing.index];
        const Price notional = trade.price * trade.quantity;

        bought.cost = (bought.cost * bought.pairs + notional) / (bought.pairs + trade.quantity);
        bought.pairs += trade.quantity;
        buyer.cash -= notional;
        sold.pairs -= trade.quantity;
        seller.cash += notional;

        // Release the resting side's reservation. The maker's price is the trade price.
        if (trade.aggressor == Side::Buy)
            sold.listed -= trade.quantity;
        else
            buyer.committed -= notional;

        listing.record(trade);
    }
}

} // namespace street::kicks::detail
