#include "cycle_clock.hpp"
#include "format.hpp"
#include "street/core/rng.hpp"
#include "street/core/stopwatch.hpp"
#include "street/core/term.hpp"
#include "street/core/text.hpp"
#include "street/kicks/bench.hpp"
#include "street/kicks/order_book.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <format>
#include <stdexcept>
#include <vector>

namespace street::kicks {
namespace {

enum class OpKind : std::uint8_t { Limit, Market, Cancel };

struct Op {
    OpKind kind = OpKind::Limit;
    Side side = Side::Buy;
    std::int32_t quantity = 0;
    TraderId trader = 0;
    std::uint32_t target = 0; ///< cancels: index of the op whose order is pulled
    Price price = 0;
};

constexpr Price kCenter = 20'000;           // $200.00 on a one-cent grid
constexpr std::uint32_t kRecentRing = 4096; // cancels aim at the last few thousand limits

std::int32_t order_size(Rng& rng) {
    // Mostly small clips with an occasional block, like retail-dominated flow.
    return rng.chance(0.9) ? rng.range(1, 10) : rng.range(11, 60);
}

std::vector<Op> make_flow(std::size_t count, std::uint64_t seed) {
    Rng rng(seed, 0xB3AC4ull);
    std::vector<Op> ops(count);
    std::vector<std::uint32_t> recent(kRecentRing);
    std::uint32_t recent_count = 0;
    double mid = static_cast<double>(kCenter);

    for (std::size_t i = 0; i < count; ++i) {
        Op& op = ops[i];
        // The mid wanders as a slow mean-reverting walk: the book drifts but stays bounded.
        mid += rng.normal(0.0, 1.5) - 0.002 * (mid - static_cast<double>(kCenter));
        const auto center = static_cast<Price>(std::llround(mid));
        op.trader = rng.below(1024);
        op.side = rng.chance(0.5) ? Side::Buy : Side::Sell;
        const bool buy = op.side == Side::Buy;
        const double roll = rng.uniform();

        if (roll < 0.40 && recent_count > 0) { // cancel a recent order (it may already be gone)
            op.kind = OpKind::Cancel;
            op.target = recent[rng.below(std::min(recent_count, kRecentRing))];
            continue;
        }
        if (roll < 0.95) {
            op.kind = OpKind::Limit;
            op.quantity = order_size(rng);
            if (roll < 0.85) { // passive: a geometric number of cents behind the mid
                const auto away = 1 + static_cast<Price>(-std::log(1.0 - rng.uniform()) * 4.0);
                op.price = buy ? center - away : center + away;
            } else { // marketable: reaches a few cents through the mid
                const Price through = rng.range(0, 4);
                op.price = buy ? center + through : center - through;
            }
            recent[recent_count % kRecentRing] = static_cast<std::uint32_t>(i);
            ++recent_count;
            continue;
        }
        op.kind = OpKind::Market;
        op.quantity = order_size(rng);
    }
    return ops;
}

/// Replays the flow; when Timed, stores the cycle count of every operation in `samples`.
template <bool Timed>
std::size_t replay(const std::vector<Op>& ops, OrderBook& book, std::vector<OrderId>& ids,
                   [[maybe_unused]] std::vector<std::uint64_t>& samples) {
    std::size_t trades = 0;
    for (std::size_t i = 0; i < ops.size(); ++i) {
        const Op& op = ops[i];
        [[maybe_unused]] std::uint64_t start = 0;
        if constexpr (Timed) start = detail::cycle_stamp();
        switch (op.kind) {
        case OpKind::Limit: {
            const OrderResult result = book.limit(op.trader, op.side, op.price, op.quantity);
            ids[i] = result.id;
            trades += result.trades.size();
            break;
        }
        case OpKind::Market:
            trades += book.market(op.trader, op.side, op.quantity).trades.size();
            break;
        case OpKind::Cancel:
            book.cancel(ids[op.target]);
            break;
        }
        if constexpr (Timed) samples[i] = detail::cycle_stamp() - start;
    }
    return trades;
}

/// Smallest back-to-back timestamp difference: the cost of the measurement itself.
std::uint64_t timer_overhead() {
    std::uint64_t best = ~0ull;
    for (int i = 0; i < 2000; ++i) {
        const std::uint64_t a = detail::cycle_stamp();
        const std::uint64_t b = detail::cycle_stamp();
        best = std::min(best, b - a);
    }
    return best;
}

/// k-th smallest (nth_element beats a full sort on a million samples).
double percentile(std::vector<double>& values, double p) {
    const auto k = static_cast<std::size_t>(p / 100.0 * static_cast<double>(values.size() - 1));
    std::nth_element(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(k), values.end());
    return values[k];
}

} // namespace

BenchResult benchmark(std::size_t orders, std::uint64_t seed) {
    if (orders == 0) throw std::invalid_argument("benchmark needs at least one order");
    if (orders > 0xFFFFFFFFu) throw std::invalid_argument("benchmark: too many orders");
    const std::vector<Op> ops = make_flow(orders, seed);

    BenchResult result;
    result.orders = orders;
    for (const Op& op : ops) {
        result.limits += op.kind == OpKind::Limit ? 1 : 0;
        result.markets += op.kind == OpKind::Market ? 1 : 0;
        result.cancels += op.kind == OpKind::Cancel ? 1 : 0;
    }

    std::vector<OrderId> ids(orders, 0);
    std::vector<std::uint64_t> samples(orders, 0);

    // Pass 1: throughput, no per-operation instrumentation.
    {
        OrderBook book;
        book.reserve(1u << 16);
        const Stopwatch watch;
        result.trades = replay<false>(ops, book, ids, samples);
        result.seconds = watch.seconds();
        result.resting = book.resting_orders();
    }
    result.orders_per_second = static_cast<double>(orders) / std::max(result.seconds, 1e-9);

    // Pass 2: latency. Timestamps are calibrated to nanoseconds against steady_clock.
    {
        OrderBook book;
        book.reserve(1u << 16);
        std::fill(ids.begin(), ids.end(), OrderId{0});
        const std::uint64_t overhead = timer_overhead();
        const auto wall_start = std::chrono::steady_clock::now();
        const std::uint64_t stamp_start = detail::cycle_stamp();
        replay<true>(ops, book, ids, samples);
        const std::uint64_t stamp_end = detail::cycle_stamp();
        const auto wall_ns =
            std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - wall_start)
                .count();
        const double ns_per_stamp =
            wall_ns / static_cast<double>(std::max<std::uint64_t>(stamp_end - stamp_start, 1));

        std::vector<double> latency(orders);
        for (std::size_t i = 0; i < orders; ++i) {
            const std::uint64_t cycles = samples[i] > overhead ? samples[i] - overhead : 0;
            latency[i] = static_cast<double>(cycles) * ns_per_stamp;
        }
        result.p50_ns = percentile(latency, 50.0);
        result.p99_ns = percentile(latency, 99.0);
        result.p999_ns = percentile(latency, 99.9);
    }
    return result;
}

std::string render_bench(const BenchResult& r) {
    using namespace term;
    using detail::colors::muted;
    const auto share = [&](std::size_t part) {
        return text::percent(
            static_cast<double>(part) / static_cast<double>(std::max<std::size_t>(r.orders, 1)), 0);
    };
    const auto row = [](std::string_view name, const std::string& value) {
        return paint(pad_right(name, 14), muted) + value;
    };
    const double rate = r.orders_per_second;
    const std::string throughput = rate >= 1e6 ? text::fixed(rate / 1e6, 2) + " M orders/s"
                                               : text::fixed(rate / 1e3, 1) + " K orders/s";
    const double scale = std::max(r.p999_ns, 1.0);
    const auto latency = [&](std::string_view name, double ns) {
        return row(name, pad_left(text::fixed(ns, 0) + " ns", 8) + "  " +
                             paint(meter(ns / scale, 24), detail::colors::accent));
    };

    const std::vector<std::string> lines = {
        row("orders", text::thousands(static_cast<std::int64_t>(r.orders))) +
            paint(std::format("   limit {} · market {} · cancel {}", share(r.limits),
                              share(r.markets), share(r.cancels)),
                  muted),
        row("trades", text::thousands(static_cast<std::int64_t>(r.trades))),
        row("elapsed", text::fixed(r.seconds * 1e3, 1) + " ms"),
        row("throughput", paint_bold(throughput, palette::gold)),
        latency("latency p50", r.p50_ns),
        latency("        p99", r.p99_ns),
        latency("        p99.9", r.p999_ns),
        row("book at end",
            text::thousands(static_cast<std::int64_t>(r.resting)) + " resting orders"),
    };
    return panel("MATCHING ENGINE BENCHMARK", lines, detail::colors::accent);
}

} // namespace street::kicks
