#pragma once
// Internal helpers shared by the hoops renderers.

#include <string>
#include <string_view>
#include <vector>

#include "street/core/term.hpp"
#include "street/hoops/model.hpp"

namespace street::hoops::detail {

/// Team color, lifted a little when it is too dark to read on a black terminal.
term::Rgb readable(term::Rgb color);
/// "QNS" in bold team color.
std::string team_tag(const Team& team);
/// "Queens Bridge Poets" in team color.
std::string team_label(const Team& team);
/// Throws std::out_of_range when the id is unknown.
const Team& team_by_id(const std::vector<Team>& teams, std::uint32_t id);

/// ".682" (basketball-style win percentage; "1.000" for perfect).
std::string win_pct(double pct);
/// "+6.1" / "-2.0".
std::string signed_fixed(double value, int decimals);
/// "12-21" made-attempted.
std::string made_of(int made, int attempts);
/// A 1–99 rating colored by tier (elite gold, good green, weak gray).
std::string rating_cell(int rating);

/// Places multi-line blocks side by side, padding every line to its block width.
std::string hstack(const std::vector<std::string>& blocks, std::size_t gap = 2);
/// Greedy word wrap of plain text to lines of at most `width` cells.
std::vector<std::string> wrap(std::string_view text, std::size_t width);

} // namespace street::hoops::detail
