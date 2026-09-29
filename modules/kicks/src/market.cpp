#include "street/kicks/market.hpp"

#include "agents.hpp"
#include "floor.hpp"
#include "news.hpp"
#include "street/core/text.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <stdexcept>

namespace street::kicks {
namespace {

using detail::Floor;
using detail::Holding;
using detail::Listing;
using detail::Trader;

/// The non-maker personas, in the order of Market::Impl::crowd.
constexpr std::array<Persona, 3> kCrowd = {Persona::Collector, Persona::Reseller,
                                           Persona::Hypebeast};

constexpr double kFastAlpha = 2.0 / (20.0 + 1.0);  // ~20-tick EMA
constexpr double kSlowAlpha = 2.0 / (240.0 + 1.0); // ~one-day EMA
constexpr double kBuzzHalfLifeDays = 1.4;

/// Knuth's multiplicative Poisson sampler; plenty fast for the small per-tick rates used here.
int poisson(double lambda, Rng& rng) {
    const double limit = std::exp(-lambda);
    int k = 0;
    for (double p = rng.uniform(); p > limit; p *= rng.uniform())
        ++k;
    return k;
}

HypeParams hype_params(const Sneaker& sneaker) {
    // Hot silhouettes swing harder; everything forgets a headline with the same half-life.
    return {8.0, 0.03 + 0.04 * std::clamp(sneaker.base_hype, 0.0, 1.0)};
}

void grant(Trader& trader, const Listing& listing, std::int32_t pairs, Price cost) {
    Holding& holding = trader.holdings[listing.index];
    holding.cost = (holding.cost * holding.pairs + cost * pairs) / (holding.pairs + pairs);
    holding.pairs += pairs;
}

void validate(const Sneaker& sneaker) {
    if (sneaker.sku.empty()) throw std::invalid_argument("sneaker needs a SKU");
    if (sneaker.retail <= 0)
        throw std::invalid_argument(std::format("{}: retail price must be positive", sneaker.sku));
    if (sneaker.pairs <= 0)
        throw std::invalid_argument(std::format("{}: pair count must be positive", sneaker.sku));
}

} // namespace

std::string_view persona_name(Persona persona) noexcept {
    switch (persona) {
    case Persona::Collector:
        return "collector";
    case Persona::Reseller:
        return "reseller";
    case Persona::Hypebeast:
        return "hypebeast";
    case Persona::MarketMaker:
        return "market maker";
    }
    return "trader";
}

// ── state ────────────────────────────────────────────────────────────────────

struct Market::Impl {
    MarketConfig config;
    std::uint64_t seed;
    Rng rng; ///< trader setup and raffles; per-SKU streams live in each Listing
    int day = 0;
    Floor floor;
    std::array<std::vector<std::uint32_t>, 3> crowd; ///< trader indices, per kCrowd persona
    std::vector<News> news;

    Impl(const std::vector<Sneaker>& catalog, std::uint64_t market_seed, MarketConfig cfg);

    const Listing* find(std::string_view sku) const noexcept;
    Listing& get(std::string_view sku);
    const Listing& get(std::string_view sku) const;

    std::uint32_t add_trader(Persona persona);
    Listing& add_listing(const Sneaker& sneaker);
    void open_float(Listing& listing);
    std::int32_t raffle(Listing& listing);
    void morning_news(Listing& listing);
    void tick(Listing& listing);
    void apply_shock(Listing& listing, int news_day, std::string headline, double impact);
    Price equity(const Trader& trader) const;
};

Market::Impl::Impl(const std::vector<Sneaker>& catalog, std::uint64_t market_seed, MarketConfig cfg)
    : config(cfg), seed(market_seed), rng(market_seed, 0x6B69636B73ull /* "kicks" */) {
    if (config.ticks_per_day < 1) throw std::invalid_argument("ticks_per_day must be >= 1");
    if (config.collectors < 0 || config.resellers < 0 || config.hypebeasts < 0)
        throw std::invalid_argument("trader counts must be non-negative");

    const std::array<int, 3> counts = {config.collectors, config.resellers, config.hypebeasts};
    for (std::size_t group = 0; group < kCrowd.size(); ++group)
        for (int i = 0; i < counts[group]; ++i)
            crowd[group].push_back(add_trader(kCrowd[group]));

    for (const Sneaker& sneaker : catalog) {
        validate(sneaker);
        if (find(sneaker.sku) != nullptr)
            throw std::invalid_argument(std::format("duplicate SKU '{}' in catalog", sneaker.sku));
        open_float(add_listing(sneaker));
    }

    // A short pre-market session builds depth on every book before day 1 opens.
    for (int t = 0; t < config.ticks_per_day / 4; ++t) {
        ++floor.clock;
        for (Listing& listing : floor.listings)
            tick(listing);
    }
    for (Listing& listing : floor.listings)
        listing.open_session();
    for (Trader& trader : floor.traders)
        trader.start_equity = equity(trader);
}

const Listing* Market::Impl::find(std::string_view sku) const noexcept {
    for (const Listing& listing : floor.listings)
        if (text::iequals(listing.sneaker.sku, sku)) return &listing;
    return nullptr;
}

const Listing& Market::Impl::get(std::string_view sku) const {
    if (const Listing* listing = find(sku)) return *listing;
    throw std::out_of_range(std::format("unknown sneaker SKU '{}'", sku));
}

Listing& Market::Impl::get(std::string_view sku) {
    return const_cast<Listing&>(std::as_const(*this).get(sku));
}

std::uint32_t Market::Impl::add_trader(Persona persona) {
    Trader trader;
    trader.id = static_cast<TraderId>(floor.traders.size());
    trader.persona = persona;
    trader.temperament = rng.uniform();
    switch (persona) {
    case Persona::Collector:
        trader.cash = dollars(rng.range(8'000, 40'000));
        trader.appetite = rng.range(1, 3);
        break;
    case Persona::Reseller:
        trader.cash = dollars(rng.range(10'000, 50'000));
        trader.patience = rng.range(40, 200);
        break;
    case Persona::Hypebeast:
        trader.cash = dollars(rng.range(2'000, 15'000));
        break;
    case Persona::MarketMaker:
        trader.cash = dollars(400'000);
        break;
    }
    trader.holdings.resize(floor.listings.size());
    trader.start_equity = trader.cash;
    floor.traders.push_back(std::move(trader));
    return static_cast<std::uint32_t>(floor.traders.size() - 1);
}

Listing& Market::Impl::add_listing(const Sneaker& sneaker) {
    Listing listing;
    listing.sneaker = sneaker;
    listing.index = floor.listings.size();
    listing.hype = HypeProcess(sneaker.base_hype, hype_params(sneaker));
    listing.anchor =
        detail::to_tick(static_cast<double>(sneaker.retail) * resale_multiplier(sneaker.base_hype));
    // Independent, reproducible streams per SKU: fundamentals never depend on order flow.
    const std::uint64_t base = mix64(seed ^ hash_seed(sneaker.sku));
    listing.hype_rng = Rng(base, 1);
    listing.flow_rng = Rng(base, 2);
    floor.listings.push_back(std::move(listing));

    for (Trader& trader : floor.traders)
        trader.holdings.resize(floor.listings.size());
    const std::uint32_t maker = add_trader(Persona::MarketMaker);
    Listing& added = floor.listings.back();
    added.maker.trader = maker;
    return added;
}

void Market::Impl::open_float(Listing& listing) {
    // An already-released shoe: pairs are spread across the floor as if bought over the years.
    const std::int32_t pairs = detail::simulated_float(listing.sneaker.pairs);
    const std::int32_t maker_pairs = std::max(2, pairs * 12 / 100);
    grant(floor.traders[listing.maker.trader], listing, maker_pairs, listing.anchor);
    listing.maker.target = maker_pairs;

    for (std::int32_t i = maker_pairs; i < pairs; ++i) {
        const std::size_t group =
            rng.weighted({crowd[0].empty() ? 0.0 : 0.45, crowd[1].empty() ? 0.0 : 0.35,
                          crowd[2].empty() ? 0.0 : 0.20});
        if (crowd[group].empty()) break;
        const auto& members = crowd[group];
        Trader& trader =
            floor.traders[members[rng.below(static_cast<std::uint32_t>(members.size()))]];
        const Price cost =
            kCrowd[group] == Persona::Hypebeast ? listing.anchor : listing.sneaker.retail;
        grant(trader, listing, 1, cost);
    }
    listing.last = detail::to_tick(listing.fair_value());
    listing.fast = listing.slow = static_cast<double>(listing.last);
    listing.open_session();
}

std::int32_t Market::Impl::raffle(Listing& listing) {
    const std::int32_t pairs = detail::simulated_float(listing.sneaker.pairs);
    const Price retail = listing.sneaker.retail;

    // The maker takes a consignment allocation so the book is two-sided from the first tick.
    Trader& maker = floor.traders[listing.maker.trader];
    const std::int32_t maker_pairs = std::max(2, pairs / 10);
    grant(maker, listing, maker_pairs, retail);
    maker.cash -= retail * maker_pairs;
    listing.maker.target = maker_pairs;

    // Everyone else enters: resellers run bots (4 tickets), hypebeasts refresh twice (2).
    constexpr std::array<int, 3> kTickets = {1, 4, 2};
    std::vector<std::uint32_t> tickets;
    for (std::size_t group = 0; group < crowd.size(); ++group)
        for (const std::uint32_t member : crowd[group])
            tickets.insert(tickets.end(), static_cast<std::size_t>(kTickets[group]), member);

    std::vector<std::uint8_t> wins(floor.traders.size(), 0);
    std::int32_t awarded = maker_pairs;
    for (std::int32_t attempt = 0; awarded < pairs && !tickets.empty() && attempt < pairs * 20;
         ++attempt) {
        const std::uint32_t winner = tickets[rng.below(static_cast<std::uint32_t>(tickets.size()))];
        Trader& trader = floor.traders[winner];
        if (wins[winner] >= 2 || trader.free_cash() < retail) continue; // two pairs per person
        ++wins[winner];
        trader.cash -= retail;
        grant(trader, listing, 1, retail);
        ++awarded;
    }
    return awarded;
}

void Market::Impl::morning_news(Listing& listing) {
    // Roll and draw unconditionally so the news stream consumes a fixed amount of randomness
    // per day, whatever happened to prices (keeps shocked and unshocked runs aligned).
    const double roll = listing.hype_rng.uniform();
    detail::Headline headline = detail::draw_headline(listing.sneaker, listing.hype_rng);
    const double rate = 0.02 + 0.035 * listing.hype.level();
    if (roll < rate) apply_shock(listing, day, std::move(headline.text), headline.impact);
}

void Market::Impl::apply_shock(Listing& listing, int news_day, std::string headline,
                               double impact) {
    listing.hype.shock(impact);
    listing.buzz = std::clamp(listing.buzz + 2.0 * impact, -1.0, 1.0);
    news.push_back({news_day, listing.sneaker.sku, std::move(headline), impact});
}

void Market::Impl::tick(Listing& listing) {
    const double dt = 1.0 / static_cast<double>(config.ticks_per_day);
    listing.hype.advance(dt, listing.hype_rng);
    listing.buzz *= std::exp2(-dt / kBuzzHalfLifeDays);
    const auto last = static_cast<double>(listing.last);
    listing.fast += kFastAlpha * (last - listing.fast);
    listing.slow += kSlowAlpha * (last - listing.slow);

    detail::make_market(floor, listing);

    Rng& flow = listing.flow_rng;
    const double heat = std::min(std::abs(listing.heat()), 1.0);
    const double rate =
        config.activity * 0.9 * (0.5 + listing.hype.level()) * (1.0 + 1.5 * std::abs(listing.buzz));
    const std::array<double, 3> weights = {crowd[0].empty() ? 0.0 : 1.0,
                                           crowd[1].empty() ? 0.0 : 1.0,
                                           crowd[2].empty() ? 0.0 : 0.8 * (0.4 + 3.0 * heat)};
    for (int arrivals = poisson(rate, flow); arrivals > 0; --arrivals) {
        const auto& members = crowd[flow.weighted(weights)];
        if (members.empty()) continue;
        Trader& trader =
            floor.traders[members[flow.below(static_cast<std::uint32_t>(members.size()))]];
        detail::act(floor, listing, trader);
    }
}

Price Market::Impl::equity(const Trader& trader) const {
    Price total = trader.cash;
    for (std::size_t i = 0; i < trader.holdings.size(); ++i)
        total += trader.holdings[i].pairs * floor.listings[i].last;
    return total;
}

// ── public API ───────────────────────────────────────────────────────────────

Market::Market(const std::vector<Sneaker>& catalog, std::uint64_t seed, MarketConfig config)
    : impl_(std::make_unique<Impl>(catalog, seed, config)) {}

Market::Market(Market&&) noexcept = default;
Market& Market::operator=(Market&&) noexcept = default;
Market::~Market() = default;

void Market::release(const Sneaker& sneaker) {
    Impl& s = *impl_;
    validate(sneaker);
    if (s.find(sneaker.sku) != nullptr)
        throw std::invalid_argument(std::format("SKU '{}' is already listed", sneaker.sku));

    Listing& listing = s.add_listing(sneaker);
    const std::int32_t pairs = s.raffle(listing);
    // Launch-day buzz: fair value opens 20% rich and bleeds back as the hype fades.
    constexpr double kLaunchBuzz = 0.20;
    s.apply_shock(listing, s.day + 1, detail::drop_headline(sneaker, pairs), kLaunchBuzz);
    listing.last = detail::to_tick(listing.fair_value());
    listing.fast = listing.slow = static_cast<double>(listing.last);
    listing.open_session();
    s.floor.traders[listing.maker.trader].start_equity =
        s.equity(s.floor.traders[listing.maker.trader]);
}

void Market::shock(std::string_view sku, double magnitude, std::string headline) {
    if (!(magnitude > -1.0) || !std::isfinite(magnitude))
        throw std::invalid_argument("shock magnitude must be a finite number > -1");
    Impl& s = *impl_;
    Listing& listing = s.get(sku);
    if (headline.empty()) headline = magnitude >= 0.0 ? "Hype spike" : "Hype crash";
    s.apply_shock(listing, s.day + 1, std::move(headline), magnitude);
}

void Market::step_day() {
    Impl& s = *impl_;
    ++s.day;
    for (Listing& listing : s.floor.listings) {
        listing.open_session();
        s.morning_news(listing);
    }
    for (int t = 0; t < s.config.ticks_per_day; ++t) {
        ++s.floor.clock;
        for (Listing& listing : s.floor.listings)
            s.tick(listing);
    }
    for (Listing& listing : s.floor.listings)
        listing.close_session(s.day);
}

void Market::run(int days) {
    for (int d = 0; d < days; ++d)
        step_day();
}

int Market::day() const noexcept {
    return impl_->day;
}
std::uint64_t Market::seed() const noexcept {
    return impl_->seed;
}

std::vector<std::string> Market::skus() const {
    std::vector<std::string> out;
    out.reserve(impl_->floor.listings.size());
    for (const Listing& listing : impl_->floor.listings)
        out.push_back(listing.sneaker.sku);
    return out;
}

bool Market::has(std::string_view sku) const noexcept {
    return impl_->find(sku) != nullptr;
}

const Sneaker& Market::sneaker(std::string_view sku) const {
    return impl_->get(sku).sneaker;
}

const std::vector<Candle>& Market::candles(std::string_view sku) const {
    return impl_->get(sku).candles;
}

Price Market::last_price(std::string_view sku) const {
    return impl_->get(sku).last;
}

Price Market::fair_value(std::string_view sku) const {
    return detail::to_tick(impl_->get(sku).fair_value());
}

double Market::hype(std::string_view sku) const {
    return impl_->get(sku).hype.level();
}

const OrderBook& Market::book(std::string_view sku) const {
    return impl_->get(sku).book;
}

std::int64_t Market::supply(std::string_view sku) const {
    const Listing& listing = impl_->get(sku);
    std::int64_t total = 0;
    for (const Trader& trader : impl_->floor.traders)
        total += trader.holdings[listing.index].pairs;
    return total;
}

std::vector<Trade> Market::recent_trades(std::string_view sku) const {
    const auto& tape = impl_->get(sku).tape;
    const std::size_t n = std::min(tape.size(), detail::kTapeSize);
    return {tape.end() - static_cast<std::ptrdiff_t>(n), tape.end()};
}

const std::vector<News>& Market::news() const noexcept {
    return impl_->news;
}

std::vector<PersonaStats> Market::persona_stats() const {
    std::vector<PersonaStats> out;
    for (const Persona persona :
         {Persona::Collector, Persona::Reseller, Persona::Hypebeast, Persona::MarketMaker}) {
        PersonaStats stats;
        stats.persona = persona;
        for (const Trader& trader : impl_->floor.traders) {
            if (trader.persona != persona) continue;
            ++stats.traders;
            for (const Holding& holding : trader.holdings)
                stats.pairs += holding.pairs;
            stats.cash += trader.cash;
            const Price equity = impl_->equity(trader);
            stats.equity += equity;
            stats.pnl += equity - trader.start_equity;
        }
        out.push_back(stats);
    }
    return out;
}

} // namespace street::kicks
