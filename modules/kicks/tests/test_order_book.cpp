#include "street/core/rng.hpp"
#include "street/kicks/order_book.hpp"

#include <doctest/doctest.h>
#include <map>
#include <stdexcept>
#include <vector>

using namespace street::kicks;

namespace {

constexpr TraderId kAlice = 1, kBob = 2, kCarol = 3, kDave = 4;

std::vector<Trade> copy(std::span<const Trade> trades) {
    return {trades.begin(), trades.end()};
}

} // namespace

TEST_CASE("an empty book has no prices, levels or orders") {
    const OrderBook book;
    CHECK_FALSE(book.best_bid());
    CHECK_FALSE(book.best_ask());
    CHECK_FALSE(book.spread());
    CHECK(book.depth(Side::Buy, 5).empty());
    CHECK(book.depth(Side::Sell, 5).empty());
    CHECK(book.resting_orders() == 0);
    CHECK(book.empty());
}

TEST_CASE("resting limit orders set the touch and aggregate into L2 levels") {
    OrderBook book;
    book.limit(kAlice, Side::Buy, dollars(100), 2);
    book.limit(kBob, Side::Buy, dollars(100), 3);
    book.limit(kCarol, Side::Buy, dollars(99), 1);
    book.limit(kDave, Side::Sell, dollars(103), 4);
    book.limit(kDave, Side::Sell, dollars(102), 1);

    CHECK(*book.best_bid() == dollars(100));
    CHECK(*book.best_ask() == dollars(102));
    CHECK(*book.spread() == dollars(2));
    CHECK(book.resting_orders() == 5);
    CHECK(book.levels(Side::Buy) == 2);
    CHECK(book.depth(Side::Buy, 10) ==
          std::vector<Level>{{dollars(100), 5, 2}, {dollars(99), 1, 1}});
    CHECK(book.depth(Side::Sell, 10) ==
          std::vector<Level>{{dollars(102), 1, 1}, {dollars(103), 4, 1}});
    CHECK(book.depth(Side::Buy, 1).size() == 1); // the level limit is honoured
}

TEST_CASE("a crossing order trades at the resting order's price") {
    OrderBook book;
    book.limit(kAlice, Side::Sell, dollars(150), 1);
    const OrderResult result = book.limit(kBob, Side::Buy, dollars(170), 1);
    REQUIRE(result.trades.size() == 1);
    const Trade& trade = result.trades.front();
    CHECK(trade.price == dollars(150)); // the buyer gets the price improvement
    CHECK(trade.buyer == kBob);
    CHECK(trade.seller == kAlice);
    CHECK(trade.buy_order == result.id);
    CHECK(trade.aggressor == Side::Buy);
    CHECK(result.filled == 1);
    CHECK(result.resting == 0);
    CHECK(book.empty());
}

TEST_CASE("orders at the same price fill first-in, first-out") {
    OrderBook book;
    const OrderId first = book.limit(kAlice, Side::Sell, dollars(200), 1).id;
    const OrderId second = book.limit(kBob, Side::Sell, dollars(200), 1).id;
    const OrderId third = book.limit(kCarol, Side::Sell, dollars(200), 1).id;

    const auto trades = copy(book.limit(kDave, Side::Buy, dollars(200), 2).trades);
    REQUIRE(trades.size() == 2);
    CHECK(trades[0].sell_order == first);
    CHECK(trades[1].sell_order == second);
    CHECK_FALSE(book.order(first));
    CHECK_FALSE(book.order(second));
    REQUIRE(book.order(third));
    CHECK(book.order(third)->remaining == 1);
}

TEST_CASE("better prices fill before earlier orders") {
    OrderBook book;
    book.limit(kAlice, Side::Buy, dollars(100), 1);                         // earlier, worse
    const OrderId better = book.limit(kBob, Side::Buy, dollars(101), 1).id; // later, better
    const auto trades = copy(book.limit(kCarol, Side::Sell, dollars(100), 1).trades);
    REQUIRE(trades.size() == 1);
    CHECK(trades[0].buy_order == better);
    CHECK(trades[0].price == dollars(101));
}

TEST_CASE("partial fills: remainders rest or stay on the book") {
    SUBCASE("aggressor larger than the book rests its remainder at its limit") {
        OrderBook book;
        book.limit(kAlice, Side::Sell, dollars(50), 3);
        const OrderResult result = book.limit(kBob, Side::Buy, dollars(51), 5);
        CHECK(result.filled == 3);
        CHECK(result.resting == 2);
        CHECK(*book.best_bid() == dollars(51));
        CHECK_FALSE(book.best_ask());
        REQUIRE(book.order(result.id));
        CHECK(book.order(result.id)->remaining == 2);
    }
    SUBCASE("resting order larger than the aggressor keeps the rest of its size") {
        OrderBook book;
        const OrderId maker = book.limit(kAlice, Side::Sell, dollars(50), 10).id;
        book.limit(kBob, Side::Buy, dollars(50), 4);
        REQUIRE(book.order(maker));
        CHECK(book.order(maker)->remaining == 6);
        CHECK(book.depth(Side::Sell, 1).front().quantity == 6);
    }
}

TEST_CASE("a market order sweeps several levels, best price first") {
    OrderBook book;
    book.limit(kAlice, Side::Sell, dollars(10), 1);
    book.limit(kBob, Side::Sell, dollars(11), 2);
    book.limit(kCarol, Side::Sell, dollars(12), 5);

    const OrderResult result = book.market(kDave, Side::Buy, 4);
    const auto trades = copy(result.trades);
    REQUIRE(trades.size() == 3);
    CHECK(trades[0].price == dollars(10));
    CHECK(trades[1].price == dollars(11));
    CHECK(trades[2].price == dollars(12));
    CHECK(trades[2].quantity == 1);
    CHECK(result.filled == 4);
    CHECK(result.cancelled == 0);
    CHECK(*book.best_ask() == dollars(12));
    CHECK(book.depth(Side::Sell, 5).front().quantity == 4);
}

TEST_CASE("market orders never rest") {
    SUBCASE("empty opposite side: nothing happens, everything is cancelled") {
        OrderBook book;
        book.limit(kAlice, Side::Buy, dollars(90), 1); // same side liquidity is irrelevant
        const OrderResult result = book.market(kBob, Side::Buy, 3);
        CHECK(result.trades.empty());
        CHECK(result.filled == 0);
        CHECK(result.resting == 0);
        CHECK(result.cancelled == 3);
        CHECK(book.resting_orders() == 1);
        CHECK_FALSE(book.cancel(result.id));
    }
    SUBCASE("larger than the book: fills what exists, cancels the rest") {
        OrderBook book;
        book.limit(kAlice, Side::Buy, dollars(90), 2);
        const OrderResult result = book.market(kBob, Side::Sell, 5);
        CHECK(result.filled == 2);
        CHECK(result.cancelled == 3);
        CHECK(book.empty());
    }
}

TEST_CASE("cancel is exact: live orders once, never filled or unknown ids") {
    OrderBook book;
    const OrderId a = book.limit(kAlice, Side::Buy, dollars(100), 1).id;
    const OrderId b = book.limit(kBob, Side::Buy, dollars(100), 1).id;
    const OrderId c = book.limit(kCarol, Side::Buy, dollars(99), 1).id;

    CHECK(book.cancel(b)); // middle of a FIFO queue
    CHECK_FALSE(book.cancel(b));
    CHECK(book.depth(Side::Buy, 1).front() == Level{dollars(100), 1, 1});

    book.limit(kDave, Side::Sell, dollars(100), 1); // fills a
    CHECK_FALSE(book.cancel(a));

    CHECK(book.cancel(c)); // last order at the level removes the level
    CHECK(book.levels(Side::Buy) == 0);
    CHECK_FALSE(book.best_bid());

    CHECK_FALSE(book.cancel(0));
    CHECK_FALSE(book.cancel(0xDEADBEEF12345678ull));
}

TEST_CASE("recycled slots never resurrect stale ids") {
    OrderBook book;
    const OrderId old_id = book.limit(kAlice, Side::Buy, dollars(10), 1).id;
    REQUIRE(book.cancel(old_id));
    const OrderId new_id = book.limit(kBob, Side::Buy, dollars(11), 1).id; // reuses the slot
    CHECK(new_id != old_id);
    CHECK_FALSE(book.cancel(old_id));
    CHECK_FALSE(book.order(old_id));
    REQUIRE(book.order(new_id));
    CHECK(book.order(new_id)->trader == kBob);
    CHECK(book.cancel(new_id));
}

TEST_CASE("order() reports a live order's state") {
    OrderBook book;
    const OrderId id = book.limit(kAlice, Side::Sell, dollars(321), 7).id;
    book.limit(kBob, Side::Buy, dollars(321), 2);
    const auto view = book.order(id);
    REQUIRE(view);
    CHECK(view->id == id);
    CHECK(view->trader == kAlice);
    CHECK(view->side == Side::Sell);
    CHECK(view->price == dollars(321));
    CHECK(view->remaining == 5);
}

TEST_CASE("orders and trades share one strictly increasing sequence") {
    OrderBook book;
    book.limit(kAlice, Side::Sell, dollars(10), 1);
    book.limit(kBob, Side::Sell, dollars(10), 1);
    const OrderResult result = book.limit(kCarol, Side::Buy, dollars(10), 2);
    REQUIRE(result.trades.size() == 2);
    CHECK(result.trades[0].sequence < result.trades[1].sequence);
    CHECK(book.last_sequence() == result.trades[1].sequence);
    CHECK(book.last_sequence() == 5); // three orders + two trades
    CHECK(book.trade_count() == 2);
    CHECK(book.traded_volume() == 2);
}

TEST_CASE("invalid orders are rejected") {
    OrderBook book;
    CHECK_THROWS_AS(book.limit(kAlice, Side::Buy, 0, 1), std::invalid_argument);
    CHECK_THROWS_AS(book.limit(kAlice, Side::Buy, -100, 1), std::invalid_argument);
    CHECK_THROWS_AS(book.limit(kAlice, Side::Buy, 100, 0), std::invalid_argument);
    CHECK_THROWS_AS(book.market(kAlice, Side::Sell, -1), std::invalid_argument);
    CHECK(book.empty());
}

TEST_CASE("randomised flow keeps every invariant") {
    // A property test: thousands of random limits, market orders and cancels. After every
    // operation the book must be uncrossed, and for every order
    //     filled + resting + cancelled == submitted.
    OrderBook book;
    street::Rng rng(2024);
    struct Tracked {
        std::int32_t submitted = 0, filled = 0;
        bool is_market = false;
        std::int32_t cancelled = 0;
    };
    std::map<OrderId, Tracked> orders;
    std::vector<OrderId> ids;
    std::int64_t bought = 0, sold = 0;
    std::uint64_t last_sequence = 0;

    for (int step = 0; step < 20'000; ++step) {
        const double roll = rng.uniform();
        const Side side = rng.chance(0.5) ? Side::Buy : Side::Sell;
        const auto quantity = static_cast<std::int32_t>(rng.range(1, 20));
        if (roll < 0.3 && !ids.empty()) {
            const OrderId id = ids[rng.below(static_cast<std::uint32_t>(ids.size()))];
            const auto before = book.order(id);
            const bool cancelled = book.cancel(id);
            CHECK(cancelled == before.has_value());
            if (cancelled) orders[id].cancelled = before->remaining;
        } else {
            const bool is_market = roll > 0.9;
            const OrderResult result =
                is_market ? book.market(kAlice + static_cast<TraderId>(step % 7), side, quantity)
                          : book.limit(kAlice + static_cast<TraderId>(step % 7), side,
                                       dollars(rng.range(95, 105)), quantity);
            orders[result.id] = {quantity, 0, is_market, result.cancelled};
            CHECK(result.filled + result.resting + result.cancelled == quantity);
            for (const Trade& t : result.trades) {
                CHECK(t.sequence > last_sequence);
                last_sequence = t.sequence;
                orders[t.buy_order].filled += t.quantity;
                orders[t.sell_order].filled += t.quantity;
                bought += t.quantity;
                sold += t.quantity;
            }
            if (!is_market) ids.push_back(result.id);
        }
        const auto bid = book.best_bid();
        const auto ask = book.best_ask();
        if (bid && ask) REQUIRE(*bid < *ask); // never crossed after matching
    }

    CHECK(bought == sold);
    CHECK(book.traded_volume() == bought);
    std::size_t live = 0;
    for (const auto& [id, info] : orders) {
        const auto view = book.order(id);
        const std::int32_t resting = view ? view->remaining : 0;
        live += view ? 1 : 0;
        CHECK(info.filled + resting + info.cancelled == info.submitted);
    }
    CHECK(live == book.resting_orders());

    // Depth agrees with the individual orders.
    std::int64_t depth_total = 0;
    for (const Side side : {Side::Buy, Side::Sell})
        for (const Level& level : book.depth(side, 1000))
            depth_total += level.quantity;
    std::int64_t order_total = 0;
    for (const auto& [id, info] : orders)
        if (const auto view = book.order(id)) order_total += view->remaining;
    CHECK(depth_total == order_total);
}
