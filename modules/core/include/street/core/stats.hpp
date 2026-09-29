#pragma once
// Small descriptive-statistics toolbox shared by every module.

#include <cmath>
#include <span>

namespace street::stats {

double mean(std::span<const double> v) noexcept;              ///< 0 for empty input
double stddev(std::span<const double> v) noexcept;            ///< sample std-dev; 0 if n < 2
double median(std::span<const double> v);
double percentile(std::span<const double> v, double p);       ///< p in [0, 100], linear interpolation
double pearson(std::span<const double> a, std::span<const double> b) noexcept; ///< 0 if undefined
double min(std::span<const double> v) noexcept;
double max(std::span<const double> v) noexcept;

constexpr double clamp(double x, double lo, double hi) noexcept {
    return x < lo ? lo : (x > hi ? hi : x);
}
constexpr double lerp(double a, double b, double t) noexcept { return a + (b - a) * t; }
inline double logistic(double x) noexcept { return 1.0 / (1.0 + std::exp(-x)); }

} // namespace street::stats
