#include "news.hpp"

#include "format.hpp"
#include "street/core/text.hpp"

#include <array>
#include <format>
#include <string_view>

namespace street::kicks::detail {
namespace {

struct Template {
    std::string_view text; ///< {cw} = colorway in quotes, {who} = a personality, {partner}
    double low, high;      ///< impact range
};

// Personalities stay generic on purpose: the culture layer brings its own named stars.
constexpr std::array<std::string_view, 8> kWho = {
    "a platinum rapper",      "an All-Star point guard",
    "a Grammy-winning MC",    "the league MVP",
    "a drill star",           "a Harlem streetwear icon",
    "a Hall of Fame shooter", "a chart-topping producer",
};

constexpr std::array<std::string_view, 6> kPartner = {
    "Harlem boutique", "Tokyo streetwear label", "Paris fashion house",
    "skate brand",     "Sao Paulo street crew",  "luxury watchmaker",
};

constexpr std::array<Template, 13> kTemplates = {{
    {"{cw} spotted courtside on {who}", 0.08, 0.20},
    {"{who} laces up {cw} in a music video", 0.08, 0.20},
    {"{cw} name-dropped in the verse of the summer", 0.06, 0.16},
    {"Collab teaser: {partner} mood board features {cw}", 0.12, 0.28},
    {"Sister colorway sold out in seconds; bots blamed", 0.08, 0.18},
    {"Deadstock OG {cw} sets an auction record", 0.05, 0.12},
    {"{cw} on the cover of a streetwear glossy", 0.05, 0.12},
    {"{who} wears {cw} to the playoff opener", 0.06, 0.15},
    {"Restock rumour: more {cw} pairs said to be coming", -0.18, -0.07},
    {"Retro of {cw} confirmed for next holiday season", -0.20, -0.08},
    {"Flood of high-tier {cw} fakes spooks buyers", -0.14, -0.05},
    {"Quality-control complaints about {cw} go viral", -0.12, -0.04},
    {"Timeline declares {cw} played out", -0.10, -0.04},
}};

template <class T, std::size_t N> const T& pick(Rng& rng, const std::array<T, N>& items) {
    return items[rng.below(static_cast<std::uint32_t>(N))];
}

void replace_all(std::string& s, std::string_view token, std::string_view value) {
    for (std::size_t at = s.find(token); at != std::string::npos; at = s.find(token, at)) {
        s.replace(at, token.size(), value);
        at += value.size();
    }
}

} // namespace

Headline draw_headline(const Sneaker& sneaker, Rng& rng) {
    // Draw everything up front so each event consumes a fixed amount of randomness.
    const Template& tpl = pick(rng, kTemplates);
    const std::string_view who = pick(rng, kWho);
    const std::string_view partner = pick(rng, kPartner);
    const double impact = rng.uniform(tpl.low, tpl.high);

    std::string text{tpl.text};
    const std::string colorway =
        sneaker.colorway.empty() ? sneaker.name : '"' + sneaker.colorway + '"';
    replace_all(text, "{cw}", colorway);
    replace_all(text, "{who}", who);
    replace_all(text, "{partner}", partner);
    if (!text.empty() && text.front() >= 'a' && text.front() <= 'z')
        text.front() = static_cast<char>(text.front() - 'a' + 'A');
    return {std::move(text), impact};
}

std::string drop_headline(const Sneaker& sneaker, int pairs) {
    const std::string colorway = sneaker.colorway.empty() ? "" : " \"" + sneaker.colorway + '"';
    return std::format("DROP: {}{} - {} pairs raffled at {}", sneaker.name, colorway,
                       text::thousands(pairs), dollar_label(sneaker.retail));
}

} // namespace street::kicks::detail
