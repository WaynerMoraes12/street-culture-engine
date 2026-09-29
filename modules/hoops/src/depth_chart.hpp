#pragma once
// Internal: how a coach splits 240 regulation minutes down a depth chart.
// Shared by the rotation system (target minutes) and Team::overall() (weights).

#include <cstddef>
#include <vector>

namespace street::hoops::detail {

/// Planned regulation minutes for each depth-chart slot of a roster of `size`
/// players (size >= 5). Sums to 240 and never exceeds 40 per player.
std::vector<double> depth_minutes(std::size_t size);

} // namespace street::hoops::detail
