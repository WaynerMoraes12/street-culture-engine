#include "format.hpp"

#include "street/core/text.hpp"

namespace street::kicks::detail {

std::string dollar_label(Price price) {
    // Round half away from zero to whole dollars without leaving integer arithmetic.
    const Price whole = (price >= 0 ? price + 50 : price - 50) / 100;
    return (whole < 0 ? "-$" : "$") + text::thousands(whole < 0 ? -whole : whole);
}

std::string price_label(Price price, bool cents) {
    return cents ? text::money(price) : dollar_label(price);
}

std::optional<double> change(Price from, Price to) noexcept {
    if (from <= 0) return std::nullopt;
    return static_cast<double>(to) / static_cast<double>(from) - 1.0;
}

std::string paint_change(std::optional<double> fraction, int decimals) {
    if (!fraction) return term::paint("--", colors::faint);
    const std::string label = text::signed_percent(*fraction, decimals);
    if (label.find_first_not_of("+-0.%") == std::string::npos) // rounds to zero
        return term::paint(text::fixed(0.0, decimals) + "%", colors::muted);
    return term::paint(label, *fraction >= 0.0 ? colors::up : colors::down);
}

std::string trim_line_ends(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    std::size_t start = 0;
    while (start < text.size()) {
        std::size_t end = text.find('\n', start);
        const bool newline = end != std::string_view::npos;
        if (!newline) end = text.size();
        std::string_view line = text.substr(start, end - start);
        while (!line.empty() && line.back() == ' ')
            line.remove_suffix(1);
        out += line;
        if (newline) out += '\n';
        start = end + 1;
    }
    return out;
}

} // namespace street::kicks::detail
