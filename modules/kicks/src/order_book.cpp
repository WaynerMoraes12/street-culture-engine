#include "street/kicks/order_book.hpp"

#include <algorithm>
#include <stdexcept>

namespace street::kicks {
namespace {

constexpr bool crosses(Side incoming, Price limit, Price resting) noexcept {
    return incoming == Side::Buy ? limit >= resting : limit <= resting;
}

} // namespace

// ── order entry ──────────────────────────────────────────────────────────────

OrderResult OrderBook::limit(TraderId trader, Side side, Price price, std::int32_t quantity) {
    if (price <= 0) throw std::invalid_argument("OrderBook::limit: price must be positive");
    if (quantity <= 0) throw std::invalid_argument("OrderBook::limit: quantity must be positive");
    return submit(trader, side, price, quantity, false);
}

OrderResult OrderBook::market(TraderId trader, Side side, std::int32_t quantity) {
    if (quantity <= 0) throw std::invalid_argument("OrderBook::market: quantity must be positive");
    return submit(trader, side, 0, quantity, true);
}

OrderResult OrderBook::submit(TraderId trader, Side side, Price price, std::int32_t quantity,
                              bool is_market) {
    fills_.clear();

    // Every order gets a pool slot (and therefore an id) up front, so trades can reference the
    // taker even when it never rests. Matching only releases slots, so `order` stays valid.
    const std::uint32_t slot = acquire_node();
    Node& order = nodes_[slot];
    order.trader = trader;
    order.side = side;
    order.remaining = quantity;
    order.sequence = ++sequence_;
    const OrderId id = make_id(slot, order.generation);

    auto& contra = ladders_[index(opposite(side))];
    while (order.remaining > 0 && !contra.empty()) {
        const std::uint32_t level_slot = contra.back().level;
        LevelSlot& level = levels_[level_slot];
        if (!is_market && !crosses(side, price, level.price)) break;
        match_level(level, order, id);
        if (level.orders == 0) {
            contra.pop_back();
            release_level(level_slot);
        }
    }

    OrderResult result;
    result.id = id;
    result.filled = quantity - order.remaining;
    if (order.remaining > 0 && !is_market) {
        result.resting = order.remaining;
        rest(slot, price);
    } else {
        result.cancelled = order.remaining;
        release_node(slot);
    }
    result.trades = fills_;
    return result;
}

void OrderBook::match_level(LevelSlot& level, Node& taker, OrderId taker_id) {
    while (taker.remaining > 0 && level.head != kNil) {
        const std::uint32_t maker_slot = level.head;
        Node& maker = nodes_[maker_slot];
        const std::int32_t quantity = std::min(taker.remaining, maker.remaining);
        const OrderId maker_id = make_id(maker_slot, maker.generation);

        Trade trade;
        if (taker.side == Side::Buy) {
            trade.buy_order = taker_id;
            trade.buyer = taker.trader;
            trade.sell_order = maker_id;
            trade.seller = maker.trader;
        } else {
            trade.buy_order = maker_id;
            trade.buyer = maker.trader;
            trade.sell_order = taker_id;
            trade.seller = taker.trader;
        }
        trade.price = level.price;
        trade.quantity = quantity;
        trade.sequence = ++sequence_;
        trade.aggressor = taker.side;
        fills_.push_back(trade);

        taker.remaining -= quantity;
        maker.remaining -= quantity;
        level.quantity -= quantity;
        ++trade_count_;
        traded_volume_ += quantity;

        if (maker.remaining == 0) {
            level.head = maker.next;
            if (level.head == kNil)
                level.tail = kNil;
            else
                nodes_[level.head].prev = kNil;
            --level.orders;
            --resting_;
            release_node(maker_slot);
        }
    }
}

void OrderBook::rest(std::uint32_t slot, Price price) {
    const Side side = nodes_[slot].side;
    const std::uint32_t level_slot = find_or_add_level(side, price);
    LevelSlot& level = levels_[level_slot];
    Node& order = nodes_[slot];
    order.level = level_slot;
    order.prev = level.tail;
    order.next = kNil;
    order.resting = true;
    if (level.tail != kNil)
        nodes_[level.tail].next = slot;
    else
        level.head = slot;
    level.tail = slot;
    ++level.orders;
    level.quantity += order.remaining;
    ++resting_;
}

bool OrderBook::cancel(OrderId id) noexcept {
    const Node* found = find(id);
    if (found == nullptr) return false;
    const auto slot = static_cast<std::uint32_t>(id & 0xFFFFFFFFu);
    Node& order = nodes_[slot];
    const std::uint32_t level_slot = order.level;
    LevelSlot& level = levels_[level_slot];

    if (order.prev != kNil)
        nodes_[order.prev].next = order.next;
    else
        level.head = order.next;
    if (order.next != kNil)
        nodes_[order.next].prev = order.prev;
    else
        level.tail = order.prev;

    level.quantity -= order.remaining;
    --level.orders;
    --resting_;
    const Side side = order.side;
    release_node(slot);
    if (level.orders == 0) remove_level(side, level_slot);
    return true;
}

void OrderBook::reserve(std::size_t orders) {
    nodes_.reserve(orders);
    fills_.reserve(64);
}

// ── queries ──────────────────────────────────────────────────────────────────

std::optional<Price> OrderBook::best(Side side) const noexcept {
    const auto& ladder = ladders_[index(side)];
    if (ladder.empty()) return std::nullopt;
    return levels_[ladder.back().level].price;
}

std::optional<Price> OrderBook::spread() const noexcept {
    const auto bid = best_bid();
    const auto ask = best_ask();
    if (!bid || !ask) return std::nullopt;
    return *ask - *bid;
}

std::vector<Level> OrderBook::depth(Side side, std::size_t levels) const {
    const auto& ladder = ladders_[index(side)];
    std::vector<Level> out;
    out.reserve(std::min(levels, ladder.size()));
    for (auto it = ladder.rbegin(); it != ladder.rend() && out.size() < levels; ++it) {
        const LevelSlot& level = levels_[it->level];
        out.push_back({level.price, level.quantity, level.orders});
    }
    return out;
}

const OrderBook::Node* OrderBook::find(OrderId id) const noexcept {
    const auto slot = static_cast<std::uint32_t>(id & 0xFFFFFFFFu);
    const auto generation = static_cast<std::uint32_t>(id >> 32);
    if (slot >= nodes_.size()) return nullptr;
    const Node& node = nodes_[slot];
    return node.resting && node.generation == generation ? &node : nullptr;
}

std::optional<OrderView> OrderBook::order(OrderId id) const noexcept {
    const Node* node = find(id);
    if (node == nullptr) return std::nullopt;
    return OrderView{
        id, node->trader, node->side, levels_[node->level].price, node->remaining, node->sequence};
}

// ── pools ────────────────────────────────────────────────────────────────────

std::uint32_t OrderBook::acquire_node() {
    if (free_node_ != kNil) {
        const std::uint32_t slot = free_node_;
        free_node_ = nodes_[slot].next;
        return slot;
    }
    if (nodes_.size() >= kNil) throw std::length_error("OrderBook: order pool exhausted");
    nodes_.emplace_back();
    return static_cast<std::uint32_t>(nodes_.size() - 1);
}

void OrderBook::release_node(std::uint32_t slot) noexcept {
    Node& node = nodes_[slot];
    node.resting = false;
    node.level = kNil;
    node.prev = kNil;
    // Bumping the generation invalidates every id ever handed out for this slot.
    if (++node.generation == 0) node.generation = 1;
    node.next = free_node_;
    free_node_ = slot;
}

std::uint32_t OrderBook::find_or_add_level(Side side, Price price) {
    auto& ladder = ladders_[index(side)];
    const Price key = key_of(side, price);

    // Fast paths for the common cases: a new best price, or joining the best level.
    if (ladder.empty() || key > ladder.back().key) {
        const std::uint32_t slot = acquire_level(price);
        ladder.push_back({key, slot});
        return slot;
    }
    if (key == ladder.back().key) return ladder.back().level;

    const auto it = std::lower_bound(ladder.begin(), ladder.end(), key,
                                     [](const Rung& rung, Price k) { return rung.key < k; });
    if (it->key == key) return it->level; // it != end(): key < back().key
    const std::uint32_t slot = acquire_level(price);
    ladder.insert(it, {key, slot});
    return slot;
}

void OrderBook::remove_level(Side side, std::uint32_t slot) noexcept {
    auto& ladder = ladders_[index(side)];
    if (ladder.back().level == slot) {
        ladder.pop_back();
    } else {
        const Price key = key_of(side, levels_[slot].price);
        const auto it = std::lower_bound(ladder.begin(), ladder.end(), key,
                                         [](const Rung& rung, Price k) { return rung.key < k; });
        ladder.erase(it);
    }
    release_level(slot);
}

std::uint32_t OrderBook::acquire_level(Price price) {
    std::uint32_t slot = free_level_;
    if (slot != kNil) {
        free_level_ = levels_[slot].head;
    } else {
        if (levels_.size() >= kNil) throw std::length_error("OrderBook: level pool exhausted");
        levels_.emplace_back();
        slot = static_cast<std::uint32_t>(levels_.size() - 1);
    }
    levels_[slot] = LevelSlot{price, 0, 0, kNil, kNil};
    return slot;
}

void OrderBook::release_level(std::uint32_t slot) noexcept {
    levels_[slot].head = free_level_;
    free_level_ = slot;
}

} // namespace street::kicks
