#pragma once
// Internal: how each persona behaves when it shows up at a listing.
//
// Behaviour is intentionally simple and readable: a handful of rules per persona whose
// interaction produces the interesting dynamics (premiums, momentum overshoots after a
// headline, reversion as resellers cut prices and market makers lean against inventory).

#include "floor.hpp"

#include <cstddef>

namespace street::kicks::detail {

/// One visit by a collector, reseller or hypebeast. Randomness comes from listing.flow_rng.
void act(Floor& floor, Listing& listing, Trader& trader);

/// Refreshes the listing's market-maker quotes when they are stale or fair value has moved.
void make_market(Floor& floor, Listing& listing);

/// Stable per-(trader, SKU) taste in [-1, 1): how much more or less than fair value this
/// trader thinks a pair is worth. Derived by hashing, so it costs no storage or randomness.
double taste(TraderId trader, std::size_t listing) noexcept;

} // namespace street::kicks::detail
