#pragma once
// Internal: procedurally generated sneaker headlines and their hype impact.

#include "street/core/rng.hpp"
#include "street/kicks/catalog.hpp"

#include <string>

namespace street::kicks::detail {

struct Headline {
    std::string text;
    double impact = 0.0; ///< relative fair-value shock
};

/// A random news event for one sneaker (celebrity sighting, restock rumour, collab teaser,
/// "sold out in seconds", fakes scare, ...). Every draw consumes the same amount of
/// randomness, so the news stream never depends on how the market traded.
Headline draw_headline(const Sneaker& sneaker, Rng& rng);

/// The launch headline logged when a sneaker drops.
std::string drop_headline(const Sneaker& sneaker, int pairs);

} // namespace street::kicks::detail
