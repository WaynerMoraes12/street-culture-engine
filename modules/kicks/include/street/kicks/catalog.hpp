#pragma once
// The sneaker catalog: what trades on the exchange.

#include "street/kicks/types.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace street::kicks {

struct Sneaker {
    std::string sku;      ///< short ticker symbol, e.g. "AJ1-CHI"
    std::string name;     ///< model, e.g. "Air Jordan 1 High OG"
    std::string colorway; ///< e.g. "Chicago"
    int year = 0;         ///< release year of this edition
    Price retail = 0;     ///< retail price in cents
    int pairs = 0;        ///< approximate production run
    double base_hype = 0; ///< long-run hype in [0, 1]; sets the resale premium
    std::string story;    ///< one factual line of history, e.g. the game that made it famous

    /// `Air Jordan 1 High OG "Chicago"`
    std::string display_name() const;
};

/// Sixteen iconic silhouettes, from grails to general releases, including the signature shoes
/// of Michael Jordan, Kobe Bryant, LeBron James and Allen Iverson. Retail prices are the retro
/// MSRPs; pair counts are rough public estimates (brands do not publish them), used only to
/// size how many pairs float on the simulated secondary market.
const std::vector<Sneaker>& classic_catalog();

/// Case-insensitive SKU lookup; nullptr when absent.
const Sneaker* find_sneaker(const std::vector<Sneaker>& catalog, std::string_view sku) noexcept;

/// Long-run resale price as a multiple of retail for a given hype. Convex on purpose: general
/// releases (hype ~0.1) trade a little under retail, mid-tier retros at 1.2-2.5x, and true
/// grails (hype > 0.9) at 8-10x, which is roughly how the real resale market is shaped.
double resale_multiplier(double hype) noexcept;

} // namespace street::kicks
