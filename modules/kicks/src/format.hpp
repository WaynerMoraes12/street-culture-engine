#pragma once
// Internal: price and change formatting shared by the renderers.

#include "street/core/term.hpp"
#include "street/kicks/types.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace street::kicks::detail {

namespace colors {
inline constexpr term::Rgb up{34, 197, 94};   ///< bids, gains
inline constexpr term::Rgb down{239, 68, 68}; ///< asks, losses
inline constexpr term::Rgb up_dim{21, 128, 61};
inline constexpr term::Rgb down_dim{153, 27, 27};
inline constexpr term::Rgb accent{34, 211, 238}; ///< the kicks pillar colour (cyan)
inline constexpr term::Rgb muted{140, 140, 150};
inline constexpr term::Rgb faint{88, 88, 100};
} // namespace colors

/// "$1,473": rounded to whole dollars (sneakers trade on a $1 grid).
std::string dollar_label(Price price);
/// "$1,473" when `cents` is false, "$1,472.50" otherwise.
std::string price_label(Price price, bool cents);

/// Relative change b/a - 1, or nullopt when a is not positive.
std::optional<double> change(Price from, Price to) noexcept;
/// "+12.4%" in green / "-3.1%" in red / "--" in grey when there is no value.
std::string paint_change(std::optional<double> fraction, int decimals = 1);

/// Removes trailing spaces from every line (renderers pad cells; logs should not carry that).
std::string trim_line_ends(std::string_view text);

} // namespace street::kicks::detail
