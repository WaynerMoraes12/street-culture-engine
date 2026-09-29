#pragma once
// Terminal rendering toolkit: truecolor ANSI styling, UTF-8 width math,
// sparklines, meters, rules and boxed panels.
//
// Every styling function returns plain text when colors are disabled
// (NO_COLOR, output redirected to a file, or set_color_enabled(false)),
// so renderers can be written once and still produce clean logs.

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace street::term {

struct Rgb {
    std::uint8_t r = 0, g = 0, b = 0;
    friend constexpr bool operator==(Rgb, Rgb) = default;
};

namespace palette {
inline constexpr Rgb white{236, 236, 236};
inline constexpr Rgb gray{140, 140, 150};
inline constexpr Rgb dark{88, 88, 100};
inline constexpr Rgb red{239, 68, 68};
inline constexpr Rgb green{34, 197, 94};
inline constexpr Rgb gold{255, 196, 0};
inline constexpr Rgb orange{255, 128, 0};
inline constexpr Rgb blue{59, 130, 246};
inline constexpr Rgb purple{139, 92, 246};
inline constexpr Rgb cyan{34, 211, 238};
inline constexpr Rgb pink{255, 64, 129};
inline constexpr Rgb teal{20, 184, 166};
inline constexpr Rgb hardwood{205, 142, 80};
inline constexpr Rgb cement{160, 160, 160};
} // namespace palette

/// Prepares the console (UTF-8 output + VT sequences on Windows) and detects
/// whether colors should be used. Idempotent; call once at startup.
void init();
bool color_enabled() noexcept;
void set_color_enabled(bool enabled) noexcept;

std::string fg(Rgb c);
std::string bg(Rgb c);
std::string_view reset() noexcept;
std::string_view bold() noexcept;
std::string_view dim() noexcept;
std::string_view italic() noexcept;
std::string_view underline() noexcept;

std::string paint(std::string_view text, Rgb color);
std::string paint_bold(std::string_view text, Rgb color);
std::string paint_bg(std::string_view text, Rgb foreground, Rgb background);
std::string emphasize(std::string_view text); ///< bold, no color change

Rgb lerp(Rgb a, Rgb b, double t) noexcept;
/// Colors each code point along a gradient — great for banners.
std::string gradient(std::string_view text, Rgb from, Rgb to);
std::string gradient(std::string_view text, std::span<const Rgb> stops);

std::string strip_ansi(std::string_view s);
/// Display width in terminal cells: code points, ignoring ANSI escape sequences.
std::size_t width(std::string_view s) noexcept;
std::string pad_right(std::string_view s, std::size_t w);
std::string pad_left(std::string_view s, std::size_t w);
std::string center(std::string_view s, std::size_t w);
/// Cuts to at most w cells, appending "…" when truncated. ANSI-safe for plain text.
std::string truncate(std::string_view s, std::size_t w);
std::string repeat(std::string_view unit, std::size_t n);

/// "▁▂▄▆█" style sparkline scaled between the series min and max.
std::string sparkline(std::span<const double> values);
/// Smooth horizontal bar using 1/8 block characters. fraction is clamped to [0, 1].
std::string meter(double fraction, std::size_t width);
/// A horizontal rule, optionally with an embedded title: "── Title ──────".
std::string rule(std::size_t width, std::string_view title = {}, Rgb color = palette::gray);
/// Rounded box around lines of (possibly colored) text.
std::string panel(std::string_view title, std::span<const std::string> lines,
                  Rgb border = palette::gray, std::size_t min_width = 0);

/// Current terminal width in columns (COLUMNS env or console query; defaults to 100).
int columns();

} // namespace street::term
