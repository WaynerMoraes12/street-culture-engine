#pragma once
// Hype: the latent driver of a sneaker's fair value.
//
// Each SKU carries a log-deviation X(t) from its long-run fair value that follows an
// Ornstein-Uhlenbeck process,
//
//     dX = -theta * X dt + sigma dW,          theta = ln 2 / half-life,
//
// plus discrete jumps from news. Fair value is anchor * exp(X), so a shock of relative
// magnitude m (X += log(1 + m)) moves fair value by exactly m, then decays back with the
// configured half-life. The user-facing hype index maps X onto (0, 1) around the sneaker's
// base hype through the logit, so hot silhouettes saturate instead of exceeding 100%.

#include "street/core/rng.hpp"

namespace street::kicks {

struct HypeParams {
    double half_life_days = 8.0; ///< how fast shocks fade
    double volatility = 0.05;    ///< sigma, in log-fair-value per sqrt(day)
};

class HypeProcess {
public:
    explicit HypeProcess(double base_hype = 0.5, HypeParams params = {}) noexcept;

    /// Exact OU transition over dt_days (no discretisation bias for any step size).
    void advance(double dt_days, Rng& rng) noexcept;
    /// Relative fair-value jump; magnitudes at or below -1 are clamped to -0.95.
    void shock(double magnitude) noexcept;

    double deviation() const noexcept { return x_; } ///< X, the log-deviation
    double multiplier() const noexcept;              ///< exp(X): fair value / anchor
    double level() const noexcept;                   ///< hype index in (0, 1)
    double base() const noexcept { return base_; }
    const HypeParams& params() const noexcept { return params_; }

private:
    double base_;
    HypeParams params_;
    double x_ = 0.0;
};

} // namespace street::kicks
