#pragma once
// The five styles the composer knows, and their defaults.
//
// A style is a whole production aesthetic: tempo range, swing, key, chord language,
// drum grammar (see composer), and a sound palette plus mix and bus effects (see
// the renderer's kit). The table below is the public, data-only face of it.

#include "street/core/term.hpp"

#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace street::beats {

enum class Style { BoomBap, Trap, LoFi, Drill, Funk, Electro, Gallop };

struct StyleInfo {
    Style style;
    std::string_view id, name, description;
    int default_bpm;      ///< typical tempo; unspecified BPMs are drawn from [bpm_min, bpm_max]
    double default_swing; ///< 0.5 straight … 0.667 triplet
    std::string_view default_key; ///< typical key; unspecified keys are drawn from `keys`
    int bpm_min, bpm_max;
    std::span<const std::string_view> keys; ///< seeded key pool (first = default_key)
    term::Rgb color;                        ///< accent color for renders
    std::string_view tagline;               ///< a few words for tables
};

std::span<const StyleInfo> styles();
const StyleInfo& info(Style style);
/// Accepts ids ("boombap", "lofi"), names ("Boom Bap") and dashed forms ("lo-fi", "boom-bap").
std::optional<Style> parse_style(std::string_view id);
/// "boombap, trap, lofi, …": every style id, for help and error messages.
std::string style_list(std::string_view separator = ", ");

} // namespace street::beats
