#pragma once
// Renderers for the culture feed: opening night, weekly reports and the season recap.

#include <string>

#include "street/culture/universe.hpp"

namespace street::culture {

/// The signature drops that open the season.
std::string render_opening(const Universe& universe);
/// One culture week: big nights, market moves, Player of the Week, verse and anthem.
std::string render_week(const WeekReport& report, const Universe& universe);
/// Standings, playoffs, the Culture Index (shoe vs. athlete) and the mixtape.
std::string render_recap(const Recap& recap, const Universe& universe);

} // namespace street::culture
