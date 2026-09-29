// League renderers: the team table and a team's roster card.

#include <format>

#include "render_util.hpp"
#include "street/core/table.hpp"
#include "street/core/text.hpp"
#include "street/hoops/render.hpp"

namespace street::hoops {
namespace {

std::string role_of(const Player& p, std::size_t slot) {
    if (p.star) return term::paint_bold("Star", term::palette::gold);
    if (slot < 5) return "Starter";
    return slot == 5 ? std::string("6th Man") : term::paint("Bench", term::palette::gray);
}

} // namespace

std::string render_league(const std::vector<Team>& teams) {
    using namespace term;
    Table table({"TEAM", "", "IDENTITY", "PACE", "STARS", "OVR"});
    table.title("THE LEAGUE").align(3, Align::Right).align(5, Align::Right).accent(palette::orange);
    for (const Team& t : teams) {
        std::vector<std::string> stars;
        for (const Player* p : t.stars()) stars.push_back(p->short_name());
        const PlayStyle& style = play_style(t.identity);
        table.add_row({detail::team_tag(t), detail::team_label(t), t.identity,
                       text::fixed(style.pace, 0), text::join(stars, ", "),
                       detail::rating_cell(t.overall())});
    }
    return table.render() +
           paint("  street teams <ABR> shows a roster · PACE = possessions per 48 minutes\n",
                 palette::gray);
}

std::string render_roster(const Team& team) {
    using namespace term;
    const Rgb accent = detail::readable(team.color);
    const PlayStyle& style = play_style(team.identity);
    std::vector<std::string> card = {
        paint(team.city, palette::gray) + paint("  ·  home: ", palette::dark) + team.arena +
            paint("  ·  sound: ", palette::dark) + team.sound,
        paint_bold(team.identity, accent) + paint(" — " + std::string(style.description), palette::gray),
        std::format("overall {}   pace {}", detail::rating_cell(team.overall()), text::fixed(style.pace, 0)),
    };
    std::string out = panel(team.abbrev + "  " + text::to_upper(team.full_name()), card, accent);

    Table roster({"#", "PLAYER", "POS", "AGE", "ARCHETYPE", "ROLE", "OVR"});
    roster.align(0, Align::Right).align(3, Align::Right).align(6, Align::Right).accent(accent);
    for (std::size_t i = 0; i < team.roster.size(); ++i) {
        const Player& p = team.roster[i];
        if (i == 5) roster.add_separator();
        roster.add_row({std::to_string(p.jersey),
                        p.star ? paint_bold(p.display_name(), palette::gold) : p.display_name(),
                        std::string(position_name(p.position)), std::to_string(p.age), p.archetype,
                        role_of(p, i), detail::rating_cell(p.ratings.overall())});
    }
    out += roster.render();

    Table ratings({"RATINGS", "INS", "MID", "3PT", "FT", "PAS", "HDL", "PER", "INT", "REB", "ATH",
                   "STA", "CLU"});
    ratings.align_right_from(1).accent(accent);
    for (const Player& p : team.roster) {
        const Ratings& r = p.ratings;
        ratings.add_row({p.short_name(), detail::rating_cell(r.inside), detail::rating_cell(r.mid),
                         detail::rating_cell(r.three), detail::rating_cell(r.free_throw),
                         detail::rating_cell(r.passing), detail::rating_cell(r.handling),
                         detail::rating_cell(r.perimeter_d), detail::rating_cell(r.interior_d),
                         detail::rating_cell(r.rebounding), detail::rating_cell(r.athleticism),
                         detail::rating_cell(r.stamina), detail::rating_cell(r.clutch)});
    }
    out += ratings.render();
    out += paint("  INS inside · MID mid-range · 3PT three · FT free throw · PAS passing · HDL handle\n"
                 "  PER/INT perimeter/interior D · REB rebounding · ATH athleticism · STA stamina · "
                 "CLU clutch\n",
                 palette::gray);
    return out;
}

} // namespace street::hoops
