#include "street/core/rng.hpp"
#include "street/kicks/catalog.hpp"
#include "street/kicks/hype.hpp"
#include "street/kicks/market.hpp"

#include <algorithm>
#include <cmath>
#include <doctest/doctest.h>
#include <set>
#include <stdexcept>
#include <string>

using namespace street::kicks;

namespace {

Sneaker signature_shoe() {
    Sneaker shoe{"ST-SIG1", "Street Signature 1", "Home", 2026, dollars(180), 30'000, 0.6, {}};
    shoe.story = "A fictional star's first signature shoe";
    return shoe;
}

double premium(const Market& market, const std::string& sku) {
    return static_cast<double>(market.last_price(sku)) /
               static_cast<double>(market.sneaker(sku).retail) -
           1.0;
}

} // namespace

TEST_CASE("the classic catalog is iconic, unique and well-formed") {
    const auto& catalog = classic_catalog();
    CHECK(catalog.size() >= 10);
    CHECK(catalog.size() <= 16);
    std::set<std::string> skus;
    for (const Sneaker& s : catalog) {
        CHECK(skus.insert(s.sku).second);
        CHECK(s.sku.size() <= 8);
        CHECK(s.retail >= dollars(60));
        CHECK(s.retail <= dollars(300));
        CHECK(s.pairs > 0);
        CHECK(s.base_hype >= 0.0);
        CHECK(s.base_hype <= 1.0);
        CHECK(s.year >= 1960);
        CHECK_FALSE(s.story.empty());
        CHECK(s.story.size() <= 85);
    }
    // The legends' signature shoes are all listed.
    for (const char* sku : {"AJ12-FLU", "KB6-GRN", "AZG-WHT", "QSTN-RED"})
        CHECK(find_sneaker(catalog, sku) != nullptr);
    REQUIRE(find_sneaker(catalog, "aj1-chi") != nullptr);
    CHECK(find_sneaker(catalog, "AJ1-CHI")->display_name() == "Air Jordan 1 High OG \"Chicago\"");
    CHECK(find_sneaker(catalog, "NOPE") == nullptr);
}

TEST_CASE("resale premium grows with hype: GRs under retail, grails far above") {
    CHECK(resale_multiplier(0.1) < 1.0);
    CHECK(resale_multiplier(0.95) > 7.0);
    double previous = 0.0;
    for (double h = 0.0; h <= 1.0; h += 0.05) {
        CHECK(resale_multiplier(h) > previous);
        previous = resale_multiplier(h);
    }
}

TEST_CASE("the hype process mean-reverts with the configured half-life") {
    HypeProcess hype(0.5, {5.0, 0.0}); // no noise: the path is deterministic
    street::Rng rng(1);
    hype.shock(0.25);
    CHECK(hype.multiplier() == doctest::Approx(1.25));
    for (int i = 0; i < 100; ++i)
        hype.advance(0.05, rng); // five days in small steps
    CHECK(hype.deviation() == doctest::Approx(std::log(1.25) / 2.0).epsilon(1e-9));
    hype.advance(1000.0, rng);
    CHECK(hype.multiplier() == doctest::Approx(1.0));

    HypeProcess crash(0.5, {5.0, 0.0});
    crash.shock(-5.0); // clamped: fair value cannot go negative
    CHECK(crash.multiplier() > 0.0);
}

TEST_CASE("the hype index stays in (0, 1) and follows the shock direction") {
    HypeProcess hype(0.9, {8.0, 0.2});
    street::Rng rng(7);
    for (int i = 0; i < 2000; ++i) {
        hype.advance(0.25, rng);
        CHECK(hype.level() > 0.0);
        CHECK(hype.level() < 1.0);
    }
    HypeProcess calm(0.4, {8.0, 0.0});
    const double before = calm.level();
    calm.shock(0.3);
    CHECK(calm.level() > before);
}

TEST_CASE("same seed, same market history; different seed, different history") {
    Market a(classic_catalog(), 99);
    Market b(classic_catalog(), 99);
    Market c(classic_catalog(), 100);
    a.run(15);
    b.run(15);
    c.run(15);
    bool any_difference = false;
    for (const std::string& sku : a.skus()) {
        CHECK(a.candles(sku) == b.candles(sku));
        CHECK(a.last_price(sku) == b.last_price(sku));
        any_difference = any_difference || a.candles(sku) != c.candles(sku);
    }
    CHECK(a.news() == b.news());
    CHECK(any_difference);
}

TEST_CASE("candles are well-formed and one per SKU per day") {
    Market market(classic_catalog(), 5);
    market.run(20);
    CHECK(market.day() == 20);
    std::int64_t volume = 0;
    for (const std::string& sku : market.skus()) {
        const auto& candles = market.candles(sku);
        REQUIRE(candles.size() == 20);
        for (std::size_t i = 0; i < candles.size(); ++i) {
            const Candle& c = candles[i];
            CHECK(c.day == static_cast<int>(i) + 1);
            CHECK(c.low <= std::min(c.open, c.close));
            CHECK(c.high >= std::max(c.open, c.close));
            CHECK(c.low > 0);
            CHECK(c.volume >= 0);
            volume += c.volume;
        }
        CHECK(candles.back().close == market.last_price(sku));
    }
    CHECK(volume > 1000); // the floor actually trades
}

TEST_CASE("prices look like the resale market: grails far above retail, GRs near it") {
    Market market(classic_catalog(), 11);
    market.run(30);
    CHECK(premium(market, "AJ1-CHI") > 3.0);            // a grail trades at 4x+ retail
    CHECK(std::abs(premium(market, "AF1-WHT")) < 0.40); // a GR hovers around retail
    CHECK(premium(market, "AJ1-CHI") > premium(market, "AJ4-BRD"));
    CHECK(premium(market, "AJ4-BRD") > premium(market, "SSTR-WB"));
    for (const std::string& sku : market.skus()) {
        const auto fair = static_cast<double>(market.fair_value(sku));
        const auto last = static_cast<double>(market.last_price(sku));
        CHECK(last == doctest::Approx(fair).epsilon(0.30)); // the tape tracks fundamentals
    }
}

TEST_CASE("a hype shock shows up in prices within a few days") {
    Market control(classic_catalog(), 42);
    Market shocked(classic_catalog(), 42);
    control.run(5);
    shocked.run(5);
    shocked.shock("AJ11-CON", 0.40, "Spotted on the MVP at the parade");
    control.run(3);
    shocked.run(3);
    const double lift = static_cast<double>(shocked.last_price("AJ11-CON")) /
                        static_cast<double>(control.last_price("AJ11-CON"));
    CHECK(lift > 1.15);

    const auto logged = std::find_if(shocked.news().begin(), shocked.news().end(),
                                     [](const News& n) { return n.impact == 0.40; });
    REQUIRE(logged != shocked.news().end());
    const News& item = *logged;
    CHECK(item.sku == "AJ11-CON");
    CHECK(item.headline == "Spotted on the MVP at the parade");
    CHECK(item.impact == doctest::Approx(0.40));
    CHECK(item.day == 6);

    Market crashed(classic_catalog(), 42);
    crashed.run(5);
    crashed.shock("aj11-con", -0.35, "Restock confirmed"); // SKUs match case-insensitively
    crashed.run(3);
    CHECK(static_cast<double>(crashed.last_price("AJ11-CON")) <
          0.9 * static_cast<double>(control.last_price("AJ11-CON")));
}

TEST_CASE("shock and lookups validate their input") {
    Market market;
    CHECK_THROWS_AS(market.shock("NOPE", 0.1, "x"), std::out_of_range);
    CHECK_THROWS_AS(market.shock("AJ1-CHI", -1.0, "x"), std::invalid_argument);
    CHECK_THROWS_AS(market.shock("AJ1-CHI", std::nan(""), "x"), std::invalid_argument);
    CHECK_THROWS_AS(market.candles("NOPE"), std::out_of_range);
    CHECK_THROWS_AS(market.book("NOPE"), std::out_of_range);
    CHECK_FALSE(market.has("NOPE"));
    CHECK(market.has("aj1-chi"));
}

TEST_CASE("pairs and cash are conserved by trading") {
    Market market(classic_catalog(), 3);
    const auto totals = [&] {
        std::int64_t pairs = 0;
        Price cash = 0;
        for (const PersonaStats& stats : market.persona_stats()) {
            pairs += stats.pairs;
            cash += stats.cash;
        }
        return std::pair{pairs, cash};
    };
    std::int64_t supply = 0;
    for (const std::string& sku : market.skus())
        supply += market.supply(sku);
    const auto before = totals();
    CHECK(before.first == supply);
    market.run(10);
    CHECK(totals() == before);
}

TEST_CASE("a release raffles pairs at retail and opens a secondary market") {
    Market market(classic_catalog(), 8);
    market.run(2);
    const Sneaker shoe = signature_shoe();
    market.release(shoe);
    CHECK(market.has("ST-SIG1"));
    CHECK(market.skus().back() == "ST-SIG1");
    CHECK(market.supply("ST-SIG1") > 0);
    CHECK(market.candles("ST-SIG1").empty());
    CHECK(market.news().back().sku == "ST-SIG1");
    CHECK(market.news().back().headline.find("DROP") != std::string::npos);
    CHECK_THROWS_AS(market.release(shoe), std::invalid_argument);

    Sneaker broken = shoe;
    broken.sku = "ST-BAD";
    broken.retail = 0;
    CHECK_THROWS_AS(market.release(broken), std::invalid_argument);

    const std::int64_t supply = market.supply("ST-SIG1");
    market.run(5);
    CHECK(market.candles("ST-SIG1").size() == 5);
    CHECK(market.candles("ST-SIG1").front().day == 3);
    CHECK(market.supply("ST-SIG1") == supply);
    std::int64_t volume = 0;
    for (const Candle& c : market.candles("ST-SIG1"))
        volume += c.volume;
    CHECK(volume > 0);
    CHECK(!market.recent_trades("ST-SIG1").empty());
    CHECK(market.recent_trades("ST-SIG1").size() <= 64);
}

TEST_CASE("daily news is generated and logged") {
    Market market(classic_catalog(), 21);
    market.run(60);
    CHECK(market.news().size() >= 5);
    for (const News& item : market.news()) {
        CHECK(market.has(item.sku));
        CHECK_FALSE(item.headline.empty());
        CHECK(item.day >= 1);
        CHECK(item.day <= 60);
        CHECK(item.impact != 0.0);
    }
}

TEST_CASE("persona stats cover every persona") {
    Market market;
    market.run(3);
    const auto stats = market.persona_stats();
    REQUIRE(stats.size() == 4);
    CHECK(stats[0].persona == Persona::Collector);
    CHECK(stats[3].persona == Persona::MarketMaker);
    CHECK(stats[3].traders == market.skus().size()); // one maker per SKU
    CHECK(std::string(persona_name(Persona::Hypebeast)) == "hypebeast");
}
