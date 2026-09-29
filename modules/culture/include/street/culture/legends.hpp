#pragma once
// The real history behind the simulation: fifty years of moments where hip-hop,
// basketball, sneakers and heavy metal shaped each other, each one linked to the
// place it lives on in the engines (a beat style, a catalog SKU, a league team).
//
// These are facts about public figures and records, written in our own words;
// no lyrics are quoted.

#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace street::culture {

enum class Pillar { Music, Metal, Tech, Hoops, Kicks, Film };

std::string_view pillar_name(Pillar pillar) noexcept; ///< "MUSIC", "HOOPS", ...
std::optional<Pillar> parse_pillar(std::string_view name) noexcept;

struct Moment {
    int year = 0;
    std::string_view date;   ///< "Aug 11", or empty when only the year is certain
    Pillar pillar = Pillar::Music;
    std::string_view title;
    std::string_view story;  ///< one sentence of context
    std::string_view engine; ///< where it lives in the Street Culture Engine
};

/// Every moment in chronological order.
std::span<const Moment> moments() noexcept;

struct TimelineOptions {
    int from = 0;                        ///< first year shown (0 = the beginning)
    int to = 9999;                       ///< last year shown
    std::optional<Pillar> pillar;        ///< show only one pillar
};

/// The timeline as a cassette: side A (1973–1989) and side B (1990 on), each decade
/// under a header in its era's palette — 70s funk browns and oranges, 80s neon.
std::string render_timeline(const TimelineOptions& options = {});

} // namespace street::culture
