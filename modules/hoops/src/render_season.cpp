// Season renderers: standings, stat leaders, MVP race, playoff bracket, title odds.

#include <algorithm>
#include <format>

#include "render_util.hpp"
#include "street/core/table.hpp"
#include "street/core/text.hpp"
#include "street/hoops/render.hpp"

namespace street::hoops {
namespace {

using detail::team_tag;

std::string leader_table(const Season& season, Stat stat, std::string_view title,
                         std::string_view unit, std::size_t count) {
    Table table({"PLAYER", "TEAM", std::string(unit)});
    table.title(std::string(title)).align(2, Align::Right).accent(term::palette::gray);
    for (const PlayerSeason& s : season.leaders(stat, count)) {
        const double value = stat == Stat::Points     ? s.ppg()
                             : stat == Stat::Rebounds ? s.rpg()
                             : stat == Stat::Assists  ? s.apg()
                             : stat == Stat::Steals   ? s.spg()
                             : stat == Stat::Blocks   ? s.bpg()
                                                      : s.game_score();
        table.add_row({season.player(s.player_id).short_name(), team_tag(season.team(s.team_id)),
                       term::emphasize(text::fixed(value, 1))});
    }
    return table.render();
}

/// Glyph canvas for the bracket: one display cell per entry, painted per cell.
class Canvas {
public:
    Canvas(std::size_t rows, std::size_t cols)
        : cells_(rows, std::vector<Cell>(cols)) {}

    void put(std::size_t row, std::size_t col, std::string_view glyph, term::Rgb color, bool bold = false) {
        if (row >= cells_.size() || col >= cells_[row].size()) return;
        cells_[row][col] = {std::string(glyph), color, bold, true};
    }

    void text(std::size_t row, std::size_t col, std::string_view s, term::Rgb color, bool bold = false) {
        for (char32_t cp : street::text::utf8_decode(s)) {
            std::string glyph;
            street::text::utf8_append(glyph, cp);
            put(row, col++, glyph, color, bold);
        }
    }

    std::string render() const {
        std::string out;
        for (const auto& row : cells_) {
            std::string line;
            for (const auto& c : row) {
                if (!c.set)
                    line += ' ';
                else
                    line += c.bold ? term::paint_bold(c.glyph, c.color) : term::paint(c.glyph, c.color);
            }
            while (!line.empty() && line.back() == ' ') line.pop_back();
            out += line + '\n';
        }
        return out;
    }

private:
    struct Cell {
        std::string glyph = " ";
        term::Rgb color;
        bool bold = false;
        bool set = false;
    };
    std::vector<std::vector<Cell>> cells_;
};

std::string round_name(int round, int rounds) {
    if (round == rounds - 1) return "FINALS";
    if (round == rounds - 2) return "SEMIFINALS";
    return std::format("ROUND {}", round + 1);
}

} // namespace

std::string render_standings(const Season& season, int playoff_teams) {
    using namespace term;
    const auto table_rows = season.standings();
    Table table({"#", "TEAM", "W", "L", "PCT", "GB", "DIFF", "HOME", "L10", "STRK", "ELO"});
    table.align(0, Align::Right).align_right_from(2).accent(palette::gray);
    table.title(season.regular_season_done()
                    ? "FINAL STANDINGS"
                    : std::format("STANDINGS · day {} of {}", season.day(), season.total_days()));
    if (table_rows.empty()) return table.render();
    const Standing& leader = table_rows.front();
    for (std::size_t i = 0; i < table_rows.size(); ++i) {
        const Standing& s = table_rows[i];
        const Team& t = season.team(s.team_id);
        if (static_cast<int>(i) == playoff_teams && playoff_teams > 0) table.add_separator();
        const double gb = ((leader.wins - s.wins) + (s.losses - leader.losses)) / 2.0;
        const double diff = s.games() > 0 ? static_cast<double>(s.diff()) / s.games() : 0.0;
        const int l10_wins = static_cast<int>(std::count(s.last10.begin(), s.last10.end(), 'W'));
        const int l10_losses = static_cast<int>(s.last10.size()) - l10_wins;
        const Rgb streak_color = s.streak.starts_with("W") ? palette::green
                                 : s.streak.starts_with("L") ? palette::red
                                                             : palette::gray;
        const Rgb diff_color = diff > 0.05 ? palette::green : diff < -0.05 ? palette::red : palette::gray;
        table.add_row({std::to_string(i + 1), team_tag(t) + " " + detail::team_label(t),
                       std::to_string(s.wins), std::to_string(s.losses),
                       emphasize(detail::win_pct(s.pct())),
                       i == 0 ? paint("—", palette::gray) : text::fixed(gb, 1),
                       paint(detail::signed_fixed(diff, 1), diff_color),
                       std::format("{}-{}", s.home_wins, s.home_losses),
                       std::format("{}-{}", l10_wins, l10_losses), paint(s.streak, streak_color),
                       std::to_string(static_cast<int>(s.elo + 0.5))});
    }
    std::string out = table.render();
    if (playoff_teams > 0 && playoff_teams < static_cast<int>(table_rows.size()))
        out += paint(std::format("  top {} make the playoffs · DIFF = point differential per game",
                                 playoff_teams),
                     palette::gray) +
               '\n';
    return out;
}

std::string render_leaders(const Season& season, std::size_t count) {
    return detail::hstack({leader_table(season, Stat::Points, "POINTS", "PPG", count),
                           leader_table(season, Stat::Rebounds, "REBOUNDS", "RPG", count),
                           leader_table(season, Stat::Assists, "ASSISTS", "APG", count)},
                          2);
}

std::string render_mvp_race(const Season& season, std::size_t count) {
    using namespace term;
    const auto race = season.mvp_race(count);
    Table table({"#", "PLAYER", "TEAM", "PPG", "RPG", "APG", "TS%", "GmSc", "W%", "MVP SCORE"});
    table.title("MVP RACE").align(0, Align::Right).align_right_from(3).align(9, Align::Left)
        .accent(palette::gold);
    const double top = race.empty() ? 1.0 : std::max(race.front().score, 1e-9);
    for (std::size_t i = 0; i < race.size(); ++i) {
        const MvpCandidate& c = race[i];
        const Player& p = season.player(c.player_id);
        const std::string label = p.short_name() + " \"" + p.nickname + "\"";
        const std::string name = i == 0 ? paint_bold(label, palette::gold) : label;
        table.add_row({std::to_string(i + 1), name, team_tag(season.team(c.stats.team_id)),
                       text::fixed(c.stats.ppg(), 1), text::fixed(c.stats.rpg(), 1),
                       text::fixed(c.stats.apg(), 1), text::percent(c.stats.ts_pct()),
                       text::fixed(c.stats.game_score(), 1), detail::win_pct(c.team_pct),
                       paint(meter(c.score / top, 8), palette::gold) + " " + text::fixed(c.score, 1)});
    }
    return table.render();
}

std::string render_bracket(const PlayoffResult& playoffs, const std::vector<Team>& teams) {
    using namespace term;
    const int rounds = playoffs.rounds();
    if (rounds == 0) return {};
    const auto first = playoffs.round(0);
    const std::size_t leaves = first.size() * 2;

    // Column layout: seeds, then one result column per round.
    constexpr std::size_t kNode = 9;      // "QNS 4-1" plus breathing room
    constexpr std::size_t kConnector = 4; // "─┐" / " ├─"
    std::size_t seed_width = 0;
    for (const Series* s : first)
        for (std::uint32_t id : {s->high_id, s->low_id})
            seed_width = std::max(seed_width, width(detail::team_by_id(teams, id).full_name()) + 9);
    std::vector<std::size_t> col_start(static_cast<std::size_t>(rounds) + 1);
    col_start[0] = 0;
    col_start[1] = seed_width + kConnector;
    for (int r = 2; r <= rounds; ++r)
        col_start[static_cast<std::size_t>(r)] = col_start[static_cast<std::size_t>(r) - 1] + kNode + kConnector;

    const std::size_t top = 2; // header + blank line
    const std::size_t rows = top + 2 * leaves - 1;
    const Team& champion = detail::team_by_id(teams, playoffs.champion_id);
    const std::size_t cols = col_start.back() + kNode + 4 + width(champion.full_name()) + 14;
    Canvas canvas(rows, cols);

    canvas.text(0, 0, "SEEDS", palette::gray, true);
    for (int r = 0; r < rounds; ++r)
        canvas.text(0, col_start[static_cast<std::size_t>(r) + 1], round_name(r, rounds), palette::gray, true);

    // Leaf rows: every other line, in bracket order.
    std::vector<std::size_t> node_rows;
    std::vector<std::size_t> node_ends; // column right after each node's label
    for (std::size_t i = 0; i < first.size(); ++i) {
        const Series* s = first[i];
        // Keep the bracket order: the better seed of the pair is listed first.
        const std::array<std::pair<std::uint32_t, int>, 2> pair = {
            std::pair{s->high_id, s->high_seed}, std::pair{s->low_id, s->low_seed}};
        for (std::size_t k = 0; k < 2; ++k) {
            const std::size_t row = top + (2 * i + k) * 2;
            const Team& t = detail::team_by_id(teams, pair[k].first);
            const bool lost = t.id == s->loser_id();
            const Rgb tone = lost ? palette::dark : detail::readable(t.color);
            canvas.text(row, 0, std::format("({})", pair[k].second), palette::gray);
            canvas.text(row, 4, t.abbrev, tone, !lost);
            canvas.text(row, 8, t.full_name(), lost ? palette::dark : palette::white);
            node_rows.push_back(row);
            node_ends.push_back(8 + width(t.full_name()));
        }
    }

    for (int r = 0; r < rounds; ++r) {
        const auto series = playoffs.round(r);
        std::vector<std::size_t> next_rows, next_ends;
        const std::size_t label_col = col_start[static_cast<std::size_t>(r) + 1];
        const std::size_t joint = label_col - 2;
        for (std::size_t i = 0; i < series.size(); ++i) {
            const Series* s = series[i];
            const std::size_t a = node_rows[2 * i], b = node_rows[2 * i + 1];
            const std::size_t mid = (a + b) / 2;
            for (std::size_t c = node_ends[2 * i] + 1; c < joint; ++c) canvas.put(a, c, "─", palette::gray);
            for (std::size_t c = node_ends[2 * i + 1] + 1; c < joint; ++c) canvas.put(b, c, "─", palette::gray);
            canvas.put(a, joint, "┐", palette::gray);
            canvas.put(b, joint, "┘", palette::gray);
            for (std::size_t row = a + 1; row < b; ++row) canvas.put(row, joint, "│", palette::gray);
            canvas.put(mid, joint, "├", palette::gray);
            canvas.put(mid, joint + 1, "─", palette::gray);

            const Team& w = detail::team_by_id(teams, s->winner_id());
            const int wins = std::max(s->high_wins, s->low_wins);
            const int losses = std::min(s->high_wins, s->low_wins);
            const bool final_round = r == rounds - 1;
            canvas.text(mid, label_col, w.abbrev, detail::readable(w.color), true);
            canvas.text(mid, label_col + 4, std::format("{}-{}", wins, losses), palette::white);
            if (final_round) {
                canvas.text(mid, label_col + kNode, "► CHAMPION: ", palette::gold, true);
                canvas.text(mid, label_col + kNode + 12, w.full_name(), palette::gold, true);
            }
            next_rows.push_back(mid);
            next_ends.push_back(label_col + 7);
        }
        node_rows = std::move(next_rows);
        node_ends = std::move(next_ends);
    }
    return canvas.render();
}

std::string render_title_odds(const std::vector<TitleOdds>& odds, const std::vector<Team>& teams) {
    using namespace term;
    Table table({"#", "TEAM", "TITLE", "", "FINALS", "PLAYOFFS", "AVG W"});
    table.title("CHAMPIONSHIP ODDS").align(0, Align::Right).align(3, Align::Right)
        .align_right_from(4).accent(palette::gold);
    double best = 1e-9;
    for (const auto& o : odds) best = std::max(best, o.title_probability);
    for (std::size_t i = 0; i < odds.size(); ++i) {
        const TitleOdds& o = odds[i];
        const Team& t = detail::team_by_id(teams, o.team_id);
        const Rgb tone = detail::readable(t.color);
        table.add_row({std::to_string(i + 1), team_tag(t) + " " + detail::team_label(t),
                       paint(meter(o.title_probability / best, 16), tone),
                       emphasize(text::percent(o.title_probability)),
                       text::percent(o.finals_probability), text::percent(o.playoff_probability),
                       text::fixed(o.average_wins, 1)});
    }
    return table.render() + paint("  title meter scaled to the favourite\n", palette::gray);
}

} // namespace street::hoops
