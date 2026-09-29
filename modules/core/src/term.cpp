#include "street/core/term.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <format>
#include <optional>

#include "street/core/text.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <io.h>
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace street::term {
namespace {

bool g_color = false;
bool g_initialized = false;

std::optional<std::string> env_var(const char* name) {
#ifdef _WIN32
    char* buffer = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&buffer, &length, name) != 0 || buffer == nullptr) return std::nullopt;
    std::string value(buffer);
    std::free(buffer);
    return value;
#else
    const char* value = std::getenv(name);
    if (value == nullptr) return std::nullopt;
    return std::string(value);
#endif
}

bool env_flag(const char* name) {
    const auto value = env_var(name);
    return value && !value->empty() && *value != "0";
}

bool stdout_is_tty() {
#ifdef _WIN32
    return _isatty(_fileno(stdout)) != 0;
#else
    return isatty(fileno(stdout)) != 0;
#endif
}

constexpr std::array<std::string_view, 8> kSpark = {"▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"};
constexpr std::array<std::string_view, 8> kEighths = {"", "▏", "▎", "▍", "▌", "▋", "▊", "▉"};

bool is_escape_final(char c) { return c >= '@' && c <= '~'; }

} // namespace

void init() {
    if (g_initialized) return;
    g_initialized = true;

    bool vt_ok = true;
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    const HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    vt_ok = false;
    if (out != INVALID_HANDLE_VALUE && GetConsoleMode(out, &mode))
        vt_ok = SetConsoleMode(out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
#endif

    bool enable = stdout_is_tty() && vt_ok;
    if (env_var("NO_COLOR")) enable = false;
    if (env_flag("FORCE_COLOR") || env_flag("CLICOLOR_FORCE")) enable = true;
    g_color = enable;
}

bool color_enabled() noexcept { return g_color; }
void set_color_enabled(bool enabled) noexcept { g_color = enabled; }

std::string fg(Rgb c) {
    if (!g_color) return {};
    return std::format("\x1b[38;2;{};{};{}m", int{c.r}, int{c.g}, int{c.b});
}

std::string bg(Rgb c) {
    if (!g_color) return {};
    return std::format("\x1b[48;2;{};{};{}m", int{c.r}, int{c.g}, int{c.b});
}

std::string_view reset() noexcept { return g_color ? "\x1b[0m" : ""; }
std::string_view bold() noexcept { return g_color ? "\x1b[1m" : ""; }
std::string_view dim() noexcept { return g_color ? "\x1b[2m" : ""; }
std::string_view italic() noexcept { return g_color ? "\x1b[3m" : ""; }
std::string_view underline() noexcept { return g_color ? "\x1b[4m" : ""; }

std::string paint(std::string_view text, Rgb color) {
    if (!g_color) return std::string(text);
    return fg(color) + std::string(text) + std::string(reset());
}

std::string paint_bold(std::string_view text, Rgb color) {
    if (!g_color) return std::string(text);
    return std::string(bold()) + fg(color) + std::string(text) + std::string(reset());
}

std::string paint_bg(std::string_view text, Rgb foreground, Rgb background) {
    if (!g_color) return std::string(text);
    return fg(foreground) + bg(background) + std::string(text) + std::string(reset());
}

std::string emphasize(std::string_view text) {
    if (!g_color) return std::string(text);
    return std::string(bold()) + std::string(text) + std::string(reset());
}

Rgb lerp(Rgb a, Rgb b, double t) noexcept {
    t = std::clamp(t, 0.0, 1.0);
    const auto mix = [t](std::uint8_t x, std::uint8_t y) {
        return static_cast<std::uint8_t>(std::lround(x + (y - x) * t));
    };
    return {mix(a.r, b.r), mix(a.g, b.g), mix(a.b, b.b)};
}

std::string gradient(std::string_view text, Rgb from, Rgb to) {
    const std::array<Rgb, 2> stops{from, to};
    return gradient(text, stops);
}

std::string gradient(std::string_view text, std::span<const Rgb> stops) {
    if (!g_color || stops.empty()) return std::string(text);
    const std::u32string cps = text::utf8_decode(text);
    std::string out;
    const std::size_t n = cps.size();
    for (std::size_t i = 0; i < n; ++i) {
        if (cps[i] == U' ') {
            out += ' ';
            continue;
        }
        const double t = n > 1 ? static_cast<double>(i) / static_cast<double>(n - 1) : 0.0;
        Rgb color = stops.front();
        if (stops.size() > 1) {
            const double scaled = t * static_cast<double>(stops.size() - 1);
            const auto seg = std::min(static_cast<std::size_t>(scaled), stops.size() - 2);
            color = lerp(stops[seg], stops[seg + 1], scaled - static_cast<double>(seg));
        }
        out += fg(color);
        text::utf8_append(out, cps[i]);
    }
    out += reset();
    return out;
}

std::string strip_ansi(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\x1b' && i + 1 < s.size() && s[i + 1] == '[') {
            i += 2;
            while (i < s.size() && !is_escape_final(s[i])) ++i;
            continue;
        }
        out += s[i];
    }
    return out;
}

std::size_t width(std::string_view s) noexcept {
    std::size_t cells = 0;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\x1b' && i + 1 < s.size() && s[i + 1] == '[') {
            i += 2;
            while (i < s.size() && !is_escape_final(s[i])) ++i;
            continue;
        }
        if ((static_cast<unsigned char>(s[i]) & 0xC0) != 0x80) ++cells;
    }
    return cells;
}

std::string pad_right(std::string_view s, std::size_t w) {
    const std::size_t cur = width(s);
    return std::string(s) + std::string(cur < w ? w - cur : 0, ' ');
}

std::string pad_left(std::string_view s, std::size_t w) {
    const std::size_t cur = width(s);
    return std::string(cur < w ? w - cur : 0, ' ') + std::string(s);
}

std::string center(std::string_view s, std::size_t w) {
    const std::size_t cur = width(s);
    if (cur >= w) return std::string(s);
    const std::size_t left = (w - cur) / 2;
    return std::string(left, ' ') + std::string(s) + std::string(w - cur - left, ' ');
}

std::string truncate(std::string_view s, std::size_t w) {
    if (width(s) <= w) return std::string(s);
    if (w == 0) return {};
    std::string out;
    std::size_t cells = 0;
    bool saw_escape = false;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\x1b' && i + 1 < s.size() && s[i + 1] == '[') {
            const std::size_t start = i;
            i += 2;
            while (i < s.size() && !is_escape_final(s[i])) ++i;
            out.append(s.substr(start, i - start + 1));
            saw_escape = true;
            continue;
        }
        const bool lead = (static_cast<unsigned char>(s[i]) & 0xC0) != 0x80;
        if (lead) {
            if (cells == w - 1) break;
            ++cells;
        }
        out += s[i];
    }
    out += "…";
    if (saw_escape) out += "\x1b[0m";
    return out;
}

std::string repeat(std::string_view unit, std::size_t n) {
    std::string out;
    out.reserve(unit.size() * n);
    for (std::size_t i = 0; i < n; ++i) out += unit;
    return out;
}

std::string sparkline(std::span<const double> values) {
    if (values.empty()) return {};
    const auto [lo_it, hi_it] = std::minmax_element(values.begin(), values.end());
    const double lo = *lo_it;
    const double range = *hi_it - lo;
    std::string out;
    for (double v : values) {
        std::size_t level = 3;
        if (range > 1e-12) level = static_cast<std::size_t>(std::lround((v - lo) / range * 7.0));
        out += kSpark[std::min<std::size_t>(level, 7)];
    }
    return out;
}

std::string meter(double fraction, std::size_t w) {
    fraction = std::clamp(fraction, 0.0, 1.0);
    const auto eighths = static_cast<std::size_t>(std::lround(fraction * static_cast<double>(w) * 8.0));
    const std::size_t full = std::min(eighths / 8, w);
    std::string out = repeat("█", full);
    std::size_t used = full;
    if (full < w && eighths % 8 != 0) {
        out += kEighths[eighths % 8];
        ++used;
    }
    out += std::string(w - used, ' ');
    return out;
}

std::string rule(std::size_t w, std::string_view title, Rgb color) {
    if (title.empty()) return paint(repeat("─", w), color);
    const std::size_t tw = width(title);
    const std::size_t rest = w > tw + 4 ? w - tw - 4 : 0;
    return paint("── ", color) + emphasize(title) + paint(" " + repeat("─", rest), color);
}

std::string panel(std::string_view title, std::span<const std::string> lines, Rgb border,
                  std::size_t min_width) {
    std::size_t inner = min_width;
    for (const auto& line : lines) inner = std::max(inner, width(line));
    const std::size_t tw = width(title);
    if (tw > 0) inner = std::max(inner, tw + 2);

    std::string out;
    if (tw > 0) {
        out += paint("╭─ ", border) + emphasize(title) +
               paint(" " + repeat("─", inner + 2 - tw - 3) + "╮", border);
    } else {
        out += paint("╭" + repeat("─", inner + 2) + "╮", border);
    }
    out += '\n';
    for (const auto& line : lines)
        out += paint("│ ", border) + pad_right(line, inner) + paint(" │", border) + '\n';
    out += paint("╰" + repeat("─", inner + 2) + "╯", border);
    out += '\n';
    return out;
}

int columns() {
    if (const auto env = env_var("COLUMNS")) {
        int value = 0;
        const auto [ptr, ec] = std::from_chars(env->data(), env->data() + env->size(), value);
        if (ec == std::errc{} && value > 0) return std::clamp(value, 40, 300);
    }
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info))
        return std::clamp(static_cast<int>(info.srWindow.Right - info.srWindow.Left + 1), 40, 300);
#else
    winsize ws{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0)
        return std::clamp(static_cast<int>(ws.ws_col), 40, 300);
#endif
    return 100;
}

} // namespace street::term
