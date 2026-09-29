#include "street/core/stats.hpp"

#include <algorithm>
#include <vector>

namespace street::stats {

double mean(std::span<const double> v) noexcept {
    if (v.empty()) return 0.0;
    double sum = 0.0;
    for (double x : v) sum += x;
    return sum / static_cast<double>(v.size());
}

double stddev(std::span<const double> v) noexcept {
    if (v.size() < 2) return 0.0;
    const double m = mean(v);
    double acc = 0.0;
    for (double x : v) acc += (x - m) * (x - m);
    return std::sqrt(acc / static_cast<double>(v.size() - 1));
}

double percentile(std::span<const double> v, double p) {
    if (v.empty()) return 0.0;
    std::vector<double> sorted(v.begin(), v.end());
    std::sort(sorted.begin(), sorted.end());
    const double rank = clamp(p, 0.0, 100.0) / 100.0 * static_cast<double>(sorted.size() - 1);
    const auto lo = static_cast<std::size_t>(rank);
    const std::size_t hi = std::min(lo + 1, sorted.size() - 1);
    return lerp(sorted[lo], sorted[hi], rank - static_cast<double>(lo));
}

double median(std::span<const double> v) { return percentile(v, 50.0); }

double pearson(std::span<const double> a, std::span<const double> b) noexcept {
    const std::size_t n = std::min(a.size(), b.size());
    if (n < 2) return 0.0;
    const double ma = mean(a.first(n));
    const double mb = mean(b.first(n));
    double cov = 0.0, va = 0.0, vb = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double da = a[i] - ma;
        const double db = b[i] - mb;
        cov += da * db;
        va += da * da;
        vb += db * db;
    }
    if (va <= 0.0 || vb <= 0.0) return 0.0;
    return cov / std::sqrt(va * vb);
}

double min(std::span<const double> v) noexcept {
    return v.empty() ? 0.0 : *std::min_element(v.begin(), v.end());
}

double max(std::span<const double> v) noexcept {
    return v.empty() ? 0.0 : *std::max_element(v.begin(), v.end());
}

} // namespace street::stats
