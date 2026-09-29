#include "agents.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

namespace street::kicks::detail {
namespace {

double scaled(Price price, double factor) noexcept {
    return static_cast<double>(price) * factor;
}

/// Replaces the trader's working order in this listing with a new limit order.
void post(Floor& floor, Listing& listing, Trader& trader, Side side, Price price,
          std::int32_t quantity) {
    Holding& holding = trader.holdings[listing.index];
    floor.cancel(listing, trader, holding.order);
    if (quantity <= 0) return;
    if (side == Side::Buy && trader.free_cash() < price * quantity) return;
    if (side == Side::Sell && holding.free_pairs() < quantity) return;
    const OrderResult result = floor.submit(listing, trader, side, price, quantity);
    if (result.resting > 0) {
        holding.order = result.id;
        holding.posted_at = floor.clock;
    }
}

/// Crosses the spread for up to `quantity` pairs at `limit` or better; nothing rests.
void hit(Floor& floor, Listing& listing, Trader& trader, Side side, Price limit,
         std::int32_t quantity) {
    Holding& holding = trader.holdings[listing.index];
    floor.cancel(listing, trader, holding.order);
    const std::int64_t affordable =
        side == Side::Buy ? trader.free_cash() / limit : holding.free_pairs();
    quantity = static_cast<std::int32_t>(std::min<std::int64_t>(quantity, affordable));
    if (quantity > 0) floor.take(listing, trader, side, limit, quantity);
}

// Collectors buy below what a pair is worth to them and hold. They value a shoe mostly on its
// fundamentals and only half-believe the hype (square root of the hype multiplier), so a hype
// spike tempts them to cash out into rich bids and a crash brings them in as dip buyers. They
// also rotate the odd duplicate out of the collection.
void collector(Floor& floor, Listing& listing, Trader& trader) {
    Rng& rng = listing.flow_rng;
    const Holding& holding = trader.holdings[listing.index];
    const double value = static_cast<double>(listing.anchor) *
                         std::sqrt(listing.hype.multiplier()) *
                         (0.98 + 0.12 * taste(trader.id, listing.index));
    const auto bid = listing.book.best_bid();
    const auto ask = listing.book.best_ask();
    const auto working = listing.book.order(holding.order);
    const auto at_least = [](std::optional<Price> price, double level) {
        return price && static_cast<double>(*price) >= level;
    };

    if (holding.pairs > 0 && at_least(bid, value * 1.15)) {
        hit(floor, listing, trader, Side::Sell, *bid, 1);
        return;
    }
    const bool rotate = holding.pairs > trader.appetite || (holding.pairs > 0 && rng.chance(0.05));
    if (rotate) {
        if (at_least(bid, value * 0.97))
            hit(floor, listing, trader, Side::Sell, *bid, 1);
        else if (!working || working->side == Side::Buy)
            post(floor, listing, trader, Side::Sell, to_tick(value * rng.uniform(1.04, 1.18)), 1);
        return;
    }
    if (holding.pairs >= trader.appetite) return; // collection complete: hold

    if (ask && static_cast<double>(*ask) <= value) {
        hit(floor, listing, trader, Side::Buy, *ask, 1);
        return;
    }
    // Work a patient bid below value, refreshing it now and then.
    if (working && working->side == Side::Buy && !rng.chance(0.25)) return;
    post(floor, listing, trader, Side::Buy, to_tick(value * rng.uniform(0.88, 0.97)), 1);
}

// Resellers list what they hold at a markup and cut the price when it does not move; when
// flat, they hunt for underpriced asks to flip.
void reseller(Floor& floor, Listing& listing, Trader& trader) {
    Rng& rng = listing.flow_rng;
    const Holding& holding = trader.holdings[listing.index];
    const double fair = listing.fair_value();
    const auto working = listing.book.order(holding.order);

    if (holding.pairs > 0) {
        const auto lot = std::min<std::int32_t>(holding.pairs, 3);
        const Price floor_price = to_tick(std::max(scaled(holding.cost, 0.85), fair * 0.90));
        if (working && working->side == Side::Sell) {
            const std::int64_t waited = floor.clock - holding.posted_at;
            if (waited < trader.patience) return;
            // Long unsold: dump into the bid if it clears their floor.
            const auto bid = listing.book.best_bid();
            if (waited > 3 * trader.patience && bid && *bid >= floor_price) {
                hit(floor, listing, trader, Side::Sell, *bid, lot);
                return;
            }
            // Otherwise cut the ask, never below the floor they can live with.
            const Price cut = to_tick(scaled(working->price, 0.96 - 0.03 * trader.temperament));
            const Price price = std::max(cut, floor_price);
            if (price < working->price) post(floor, listing, trader, Side::Sell, price, lot);
            return;
        }
        const double markup = 0.10 + 0.25 * trader.temperament;
        const double target =
            std::max(scaled(holding.cost, 1.0 + markup), fair * (1.0 + markup / 2));
        post(floor, listing, trader, Side::Sell, to_tick(target), lot);
        return;
    }

    const auto ask = listing.book.best_ask();
    if (ask && static_cast<double>(*ask) < fair * 0.90) {
        hit(floor, listing, trader, Side::Buy, *ask, 1);
        return;
    }
    if (!working && rng.chance(0.3))
        post(floor, listing, trader, Side::Buy, to_tick(fair * rng.uniform(0.80, 0.90)), 1);
}

// Hypebeasts ride heat (momentum + buzz): they pay up when it runs hot, dump when it turns,
// and flip for a quick profit once the pair is up.
void hypebeast(Floor& floor, Listing& listing, Trader& trader) {
    Rng& rng = listing.flow_rng;
    const Holding& holding = trader.holdings[listing.index];
    const double fair = listing.fair_value();
    const double heat = listing.heat();
    const double threshold = 0.05 + 0.25 * trader.temperament;

    const bool fomo = heat > threshold || rng.chance(0.2 * listing.hype.level());
    if (fomo && holding.pairs < 2) {
        const double premium = 0.03 + 0.3 * std::clamp(heat, 0.0, 0.8);
        hit(floor, listing, trader, Side::Buy, to_tick(fair * (1.0 + premium)), 1);
        return;
    }
    if (holding.pairs == 0) return;
    if (heat < -threshold) { // the hype turned: get out (hit() pulls any working ask first)
        hit(floor, listing, trader, Side::Sell, to_tick(fair * 0.90), holding.pairs);
        return;
    }
    const auto bid = listing.book.best_bid();
    const Price exit = to_tick(scaled(holding.cost, 1.12 + 0.25 * trader.temperament));
    const bool bored = rng.chance(0.04) && bid && static_cast<double>(*bid) >= fair * 0.95;
    if ((bid && *bid >= exit) || bored) {
        hit(floor, listing, trader, Side::Sell, *bid, 1); // bank the flip, or just move on
        return;
    }
    // List once the pair is up, judged against fair value too: after a hype jump the last print
    // is stale until someone offers pairs at the new level.
    const double street = std::max(fair, static_cast<double>(listing.last));
    const auto working = listing.book.order(holding.order);
    if (!working && street > static_cast<double>(exit))
        post(floor, listing, trader, Side::Sell, to_tick(street * 1.02), 1);
}

} // namespace

double taste(TraderId trader, std::size_t listing) noexcept {
    const std::uint64_t h = mix64((std::uint64_t{trader} << 32) ^ listing ^ 0x7A57EULL);
    return static_cast<double>(h >> 11) * 0x1.0p-52 - 1.0;
}

void act(Floor& floor, Listing& listing, Trader& trader) {
    switch (trader.persona) {
    case Persona::Collector:
        collector(floor, listing, trader);
        break;
    case Persona::Reseller:
        reseller(floor, listing, trader);
        break;
    case Persona::Hypebeast:
        hypebeast(floor, listing, trader);
        break;
    case Persona::MarketMaker:
        break; // makers only quote, see make_market()
    }
}

void make_market(Floor& floor, Listing& listing) {
    Maker& maker = listing.maker;
    Trader& trader = floor.traders[maker.trader];
    const Holding& holding = trader.holdings[listing.index];
    const double fair = listing.fair_value();

    // Requote when a side that should be quoted got filled, fair value moved, or on a timer.
    const bool wants_bid = holding.pairs < maker.target * 3;
    const bool wants_ask = holding.pairs > 0;
    const bool missing = (wants_bid && !listing.book.order(maker.bid)) ||
                         (wants_ask && !listing.book.order(maker.ask));
    const bool moved = std::abs(fair - maker.quoted_fair) > 0.006 * fair;
    if (!missing && !moved && floor.clock < maker.refresh_at) return;

    floor.cancel(listing, trader, maker.bid);
    floor.cancel(listing, trader, maker.ask);

    // Lean against inventory: long makers shade both quotes down to sell, short ones up.
    constexpr double half_spread = 0.02;
    const double excess = static_cast<double>(holding.pairs - maker.target) /
                          static_cast<double>(std::max(maker.target, 4));
    const double mid = fair * (1.0 - 0.8 * half_spread * std::clamp(excess, -1.0, 1.0));
    const Price bid = to_tick(mid * (1.0 - half_spread));
    const Price ask = std::max(to_tick(mid * (1.0 + half_spread)), bid + kTick);

    if (wants_bid && trader.free_cash() >= bid * 3) {
        const OrderResult result = floor.submit(listing, trader, Side::Buy, bid, 3);
        maker.bid = result.resting > 0 ? result.id : 0;
    }
    const std::int32_t lot = std::min<std::int32_t>(holding.free_pairs(), 3);
    if (lot > 0) {
        const OrderResult result = floor.submit(listing, trader, Side::Sell, ask, lot);
        maker.ask = result.resting > 0 ? result.id : 0;
    }
    maker.quoted_fair = fair;
    maker.refresh_at = floor.clock + 12 + static_cast<std::int64_t>(listing.flow_rng.below(12));
}

} // namespace street::kicks::detail
