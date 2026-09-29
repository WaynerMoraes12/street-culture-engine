#pragma once
// Price-time priority limit order book for a single instrument.
//
// Matching rules
//   An incoming order trades against the opposite side while prices cross: best price first,
//   oldest order first within a price. Every fill prints at the resting order's price. The
//   unfilled part of a limit order rests on the book; the unfilled part of a market order is
//   cancelled (market orders never rest). Every accepted order and every trade draws a number
//   from one monotonically increasing sequence, so book events are totally ordered.
//
// Data layout (built for an allocation-free steady state)
//   * Orders live in a pooled array and are chained into per-level FIFO queues through 32-bit
//     indices. Once the pool is warm, resting an order never calls the allocator.
//   * An OrderId is a generational handle: the low 32 bits index the pool, the high 32 bits
//     hold the slot's generation. A lookup is one bounds check plus one compare, so cancel()
//     is O(1) with no hashing. Ids of filled or cancelled orders are rejected because their
//     slot's generation moved on when it was recycled.
//   * Each side keeps its price levels in a contiguous array sorted so that the best price is
//     at the back. Nearly all activity happens at or near the touch, so matching pops from the
//     back and inserts/erases shift only a handful of entries. That is much friendlier to the
//     cache than a node-based std::map, and finding an arbitrary price is still O(log levels).
//   * Per-level state (total quantity, order count, FIFO ends) lives in a second pool whose
//     slots never move. A cancel updates its level in O(1) through the order's level slot;
//     only a cancel that empties a level touches the sorted array.

#include "street/kicks/types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace street::kicks {

/// What happened to a submitted order.
struct OrderResult {
    OrderId id = 0;                ///< handle for cancel(); goes stale once the order is gone
    std::int32_t filled = 0;       ///< quantity executed on arrival
    std::int32_t resting = 0;      ///< quantity left on the book (limit orders only)
    std::int32_t cancelled = 0;    ///< unfilled market-order remainder
    std::span<const Trade> trades; ///< fills in execution order; valid until the next mutation
};

/// Snapshot of a live resting order.
struct OrderView {
    OrderId id = 0;
    TraderId trader = 0;
    Side side = Side::Buy;
    Price price = 0;
    std::int32_t remaining = 0;
    std::uint64_t sequence = 0; ///< arrival sequence: lower means earlier in the queue
};

class OrderBook {
public:
    /// Submits a limit order. Throws std::invalid_argument unless price > 0 and quantity > 0.
    OrderResult limit(TraderId trader, Side side, Price price, std::int32_t quantity);
    /// Submits a market order that sweeps the opposite side until it is filled or the side
    /// runs dry. Throws std::invalid_argument unless quantity > 0.
    OrderResult market(TraderId trader, Side side, std::int32_t quantity);
    /// Removes a resting order. Returns false for unknown, filled or already cancelled ids.
    bool cancel(OrderId id) noexcept;
    /// Pre-sizes the order pool, e.g. before a benchmark.
    void reserve(std::size_t orders);

    std::optional<Price> best_bid() const noexcept { return best(Side::Buy); }
    std::optional<Price> best_ask() const noexcept { return best(Side::Sell); }
    std::optional<Price> best(Side side) const noexcept;
    /// best_ask - best_bid when both sides are quoted.
    std::optional<Price> spread() const noexcept;

    /// L2 view of one side, best price first, at most `levels` entries.
    std::vector<Level> depth(Side side, std::size_t levels) const;
    /// Number of distinct price levels on one side.
    std::size_t levels(Side side) const noexcept { return ladders_[index(side)].size(); }
    std::size_t resting_orders() const noexcept { return resting_; }
    bool empty() const noexcept { return resting_ == 0; }
    /// The live order behind an id, if it is still resting.
    std::optional<OrderView> order(OrderId id) const noexcept;

    /// Last sequence number handed out (orders and trades share the counter).
    std::uint64_t last_sequence() const noexcept { return sequence_; }
    std::uint64_t trade_count() const noexcept { return trade_count_; }
    std::int64_t traded_volume() const noexcept { return traded_volume_; }

private:
    static constexpr std::uint32_t kNil = 0xFFFFFFFFu;

    struct Node {
        std::uint64_t sequence = 0;
        TraderId trader = 0;
        std::int32_t remaining = 0;
        std::uint32_t generation = 1;
        std::uint32_t level = kNil; ///< slot in levels_ while resting
        std::uint32_t prev = kNil;  ///< FIFO neighbours within the level
        std::uint32_t next = kNil;  ///< doubles as the free-list link
        Side side = Side::Buy;
        bool resting = false;
    };

    struct LevelSlot {
        Price price = 0;
        std::int64_t quantity = 0;
        std::uint32_t orders = 0;
        std::uint32_t head = kNil; ///< oldest order; doubles as the free-list link
        std::uint32_t tail = kNil; ///< newest order
    };

    /// Entry of a side's sorted price array. key is +price for bids and -price for asks, so on
    /// both sides a larger key is a better price and the best level is always at the back.
    struct Rung {
        Price key = 0;
        std::uint32_t level = kNil;
    };

    static constexpr std::size_t index(Side side) noexcept { return side == Side::Buy ? 0 : 1; }
    static constexpr Price key_of(Side side, Price price) noexcept {
        return side == Side::Buy ? price : -price;
    }
    static constexpr OrderId make_id(std::uint32_t slot, std::uint32_t generation) noexcept {
        return (static_cast<OrderId>(generation) << 32) | slot;
    }

    OrderResult submit(TraderId trader, Side side, Price price, std::int32_t quantity,
                       bool is_market);
    void match_level(LevelSlot& level, Node& taker, OrderId taker_id);
    void rest(std::uint32_t slot, Price price);
    const Node* find(OrderId id) const noexcept;

    std::uint32_t acquire_node();
    void release_node(std::uint32_t slot) noexcept;
    std::uint32_t find_or_add_level(Side side, Price price);
    void remove_level(Side side, std::uint32_t slot) noexcept;
    std::uint32_t acquire_level(Price price);
    void release_level(std::uint32_t slot) noexcept;

    std::vector<Node> nodes_;
    std::uint32_t free_node_ = kNil;
    std::vector<LevelSlot> levels_;
    std::uint32_t free_level_ = kNil;
    std::array<std::vector<Rung>, 2> ladders_; ///< [bids, asks], best price at the back
    std::vector<Trade> fills_;                 ///< scratch buffer behind OrderResult::trades
    std::uint64_t sequence_ = 0;
    std::size_t resting_ = 0;
    std::uint64_t trade_count_ = 0;
    std::int64_t traded_volume_ = 0;
};

} // namespace street::kicks
