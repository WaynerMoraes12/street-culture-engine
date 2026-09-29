#pragma once
// Matching-engine benchmark.
//
// Replays a synthetic but realistic order flow against a fresh OrderBook: passive limit
// orders clustered near a wandering mid price, marketable limits and market orders that take
// liquidity, and a high share of cancels aimed at recently placed orders (as on real venues,
// most limit orders are cancelled). The flow is generated up front from the seed, so the work
// is identical run to run and generation cost is excluded from the timings.
//
// Two passes are made over the same flow: one timed as a whole for throughput, and one that
// timestamps every operation for the latency distribution (on x86 the timestamps come from
// the calibrated time-stamp counter; steady_clock is too coarse on some platforms).

#include <cstddef>
#include <cstdint>
#include <string>

namespace street::kicks {

struct BenchResult {
    std::size_t orders = 0; ///< operations replayed (limits + markets + cancels)
    std::size_t trades = 0;
    double seconds = 0.0; ///< wall time of the throughput pass
    double orders_per_second = 0.0;
    double p50_ns = 0.0; ///< per-operation latency percentiles
    double p99_ns = 0.0;
    double p999_ns = 0.0;
    std::size_t limits = 0; ///< flow mix
    std::size_t markets = 0;
    std::size_t cancels = 0;
    std::size_t resting = 0; ///< orders left on the book at the end
};

/// Runs the benchmark with `orders` operations (at least 1). Deterministic apart from timings.
BenchResult benchmark(std::size_t orders, std::uint64_t seed);

/// Results panel for the terminal.
std::string render_bench(const BenchResult& result);

} // namespace street::kicks
