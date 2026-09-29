#include "street/kicks/hype.hpp"

#include "street/core/stats.hpp"

#include <algorithm>
#include <cmath>

namespace street::kicks {

HypeProcess::HypeProcess(double base_hype, HypeParams params) noexcept
    : base_(std::clamp(base_hype, 0.01, 0.99)), params_(params) {}

void HypeProcess::advance(double dt_days, Rng& rng) noexcept {
    if (dt_days <= 0.0) return;
    const double theta = std::log(2.0) / std::max(params_.half_life_days, 1e-6);
    const double decay = std::exp(-theta * dt_days);
    // Conditional std-dev of X(t + dt) given X(t): sigma * sqrt((1 - e^{-2 theta dt}) / 2 theta).
    const double spread = params_.volatility * std::sqrt((1.0 - decay * decay) / (2.0 * theta));
    x_ = x_ * decay + spread * rng.normal();
}

void HypeProcess::shock(double magnitude) noexcept {
    x_ += std::log1p(std::max(magnitude, -0.95));
}

double HypeProcess::multiplier() const noexcept {
    return std::exp(x_);
}

double HypeProcess::level() const noexcept {
    const double logit = std::log(base_ / (1.0 - base_));
    return stats::logistic(logit + x_);
}

} // namespace street::kicks
