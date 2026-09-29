// Game renderers: scoreboard, box score and play-by-play lines.

#include <algorithm>
#include <format>

#include "render_util.hpp"
#include "street/core/table.hpp"
#include "street/core/text.hpp"
#include "street/hoops/render.hpp"

namespace street::hoops {
namespace {

using detail::readable;
using detail::team_tag;

std::string period_label(int period) {
    return period <= 4 ? std::format("Q{}", period) : std::format("OT{}", period - 4);
}

std::string final_label(const GameResult& r) {
    if (r.overtimes <= 0) return "FINAL";
    return r.overtimes == 1 ? "FINAL/OT" : std::format("FINAL/{}OT", r.overtimes);
}

std::string minutes_cell(double minutes) {
    const int total = static_cast<int>(minutes * 60.0 + 0.5);
    return std::format("{}:{:02}", total / 60, total % 60);
}

std::string team_box(const GameResult& r, int side, const Team& team, std::uint32_t top_id) {
    using namespace term;
    const Rgb accent = readable(team.color);
    Table table({"PLAYER", "MIN", "PTS", "REB", "AST", "STL", "BLK", "TO", "FG", "3P", "FT", "+/-"});
    table.align_right_from(1).accent(accent);
    table.title(std::format("{}  {}", text::to_upper(team.full_name()), r.score(side)));

    const auto& box = r.box[static_cast<std::size_t>(side)];
    bool bench_started = false;
    for (std::size_t i = 0; i < box.size(); ++i) {
        const PlayerLine& l = box[i];
        const Player* p = team.find_player(l.player_id);
        if (p == nullptr) continue;
        if (!l.starter && !bench_started) {
            table.add_separator();
            bench_started = true;
        }
        const bool top = l.player_id == top_id;
        std::string name = (top ? "► " : "  ") + p->short_name();
        name = pad_right(name, 16) + paint(std::string(position_name(p->position)), palette::gray);
        if (top) name = paint_bold(strip_ansi(name), palette::gold);
        if (l.minutes <= 0.0) {
            table.add_row({paint(strip_ansi(name), palette::dark), paint("DNP", palette::dark)});
            continue;
        }
        const std::string pts = top ? paint_bold(std::to_string(l.pts), palette::gold)
                                    : std::to_string(l.pts);
        table.add_row({name, minutes_cell(l.minutes), pts, std::to_string(l.reb()),
                       std::to_string(l.ast), std::to_string(l.stl), std::to_string(l.blk),
                       std::to_string(l.tov), detail::made_of(l.fgm, l.fga),
                       detail::made_of(l.tpm, l.tpa), detail::made_of(l.ftm, l.fta),
                       detail::signed_fixed(l.plus_minus, 0)});
    }
    const TeamTotals t = team_totals(r, side);
    table.add_separator();
    table.add_row({emphasize("  TOTALS"), std::to_string(static_cast<int>(t.minutes + 0.5)),
                   emphasize(std::to_string(t.pts)), std::to_string(t.reb()),
                   std::to_string(t.ast), std::to_string(t.stl), std::to_string(t.blk),
                   std::to_string(t.tov), detail::made_of(t.fgm, t.fga),
                   detail::made_of(t.tpm, t.tpa), detail::made_of(t.ftm, t.fta), ""});
    table.add_row({paint("  shooting", palette::gray), "", "", "", "", "", "", "",
                   paint(text::percent(t.fg_pct()), palette::gray),
                   paint(text::percent(t.tp_pct()), palette::gray),
                   paint(text::percent(t.ft_pct()), palette::gray), ""});
    return table.render();
}

} // namespace

std::string render_scoreboard(const GameResult& r, const Team& home, const Team& away) {
    using namespace term;
    const int winner = r.winner_side();
    const auto score = [&](int side) {
        const std::string s = std::to_string(r.score(side));
        return side == winner ? paint_bold(s, palette::white) : paint(s, palette::gray);
    };
    const std::size_t name_width = std::max(width(away.full_name()), width(home.full_name()));
    const auto team_line = [&](const Team& t, int side) {
        std::string line = "  " + team_tag(t) + "  " +
                           pad_right(detail::team_label(t), name_width + 2) +
                           pad_left(score(side), 4);
        if (side == winner) line += paint("  ◄", palette::gold);
        return line;
    };

    // Quarter-by-quarter line (away first, as on every scoreboard).
    std::string header = "       ";
    const auto periods = static_cast<std::size_t>(r.periods());
    for (std::size_t p = 0; p < periods; ++p)
        header += pad_left(period_label(static_cast<int>(p) + 1), 5);
    header += pad_left("T", 6);
    const auto quarter_line = [&](const Team& t, int side) {
        std::string line = "  " + team_tag(t) + "  ";
        const auto& scores = r.period_scores[static_cast<std::size_t>(side)];
        for (int s : scores) line += pad_left(std::to_string(s), 5);
        return line + pad_left(score(side), 6);
    };

    std::vector<std::string> lines = {
        team_line(away, kAway),
        team_line(home, kHome),
        "",
        paint(header, palette::gray),
        quarter_line(away, kAway),
        quarter_line(home, kHome),
    };
    if (!home.arena.empty())
        lines.push_back(paint("  at " + home.arena + ", " + home.city, palette::dark));
    return panel(final_label(r), lines, palette::gray);
}

std::string render_box_score(const GameResult& r, const Team& home, const Team& away) {
    const std::uint32_t top = r.top_performer().player_id;
    std::string out = team_box(r, kAway, away, top);
    out += '\n';
    out += team_box(r, kHome, home, top);
    const PlayerLine& best = r.top_performer();
    const Team& best_team = r.side_of(best.player_id) == kHome ? home : away;
    if (const Player* p = best_team.find_player(best.player_id)) {
        out += term::paint("► top performer: ", term::palette::gold) + term::emphasize(p->display_name()) +
               term::paint(std::format(" ({})  {} pts · {} reb · {} ast · game score {}",
                                       best_team.abbrev, best.pts, best.reb(), best.ast,
                                       text::fixed(best.game_score(), 1)),
                           term::palette::gray) +
               '\n';
    }
    return out;
}

std::string render_play(const Play& play, const Team& home, const Team& away) {
    using namespace term;
    constexpr std::size_t kTextWidth = 70;
    const std::string clock = pad_right(format_clock(play.period, play.clock), 11);
    std::string score = pad_left(std::to_string(play.away_score), 3) + "-" +
                        pad_right(std::to_string(play.home_score), 3);
    score = play.points > 0 ? emphasize(score) : paint(score, palette::gray);

    std::string tag = "   ";
    if (play.side == kHome) tag = team_tag(home);
    if (play.side == kAway) tag = team_tag(away);

    const std::string prefix = paint(clock, palette::gray) + score + "  " + tag + "  ";
    const std::string indent(11 + 7 + 2 + 3 + 2, ' ');
    const auto style = [&](const std::string& s) {
        if (play.kind == PlayKind::Info) return paint_bold(s, palette::gold);
        if (play.kind == PlayKind::Substitution) return paint(s, palette::dark);
        if (play.kind == PlayKind::Streak) return paint_bold(s, palette::orange);
        if (play.highlight) return emphasize(s);
        if (play.kind == PlayKind::Rebound || play.kind == PlayKind::Foul)
            return paint(s, palette::gray);
        return s;
    };
    std::string out;
    const auto lines = detail::wrap(play.text, kTextWidth);
    for (std::size_t i = 0; i < lines.size(); ++i)
        out += (i == 0 ? prefix : indent) + style(lines[i]) + (i + 1 < lines.size() ? "\n" : "");
    return out;
}

} // namespace street::hoops
