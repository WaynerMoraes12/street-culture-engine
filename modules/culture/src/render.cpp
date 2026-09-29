#include "street/culture/render.hpp"

#include <algorithm>
#include <cmath>
#include <format>

#include "street/core/table.hpp"
#include "street/core/term.hpp"
#include "street/core/text.hpp"
#include "street/hoops/render.hpp"

namespace street::culture {
namespace {

using namespace term;

constexpr std::size_t kWidth = 96;

std::string section(std::string_view title, Rgb color) {
    return paint_bold(title, color) + '\n';
}

std::string team_tag(const hoops::Team& team) { return paint_bold(team.abbrev, team.color); }

std::string athlete(const hoops::Season& season, std::uint32_t player_id) {
    const hoops::Player& player = season.player(player_id);
    return player.short_name() + " " + team_tag(season.team_of(player_id));
}

std::string change_text(double change) {
    const std::string body = text::signed_percent(change);
    return paint(body, change >= 0.0 ? palette::green : palette::red);
}

std::string arrow(double change) {
    return change >= 0.0 ? paint("▲", palette::green) : paint("▼", palette::red);
}

// Sneaker prices read better in whole dollars ("$1,225"); cents stay inside the engine.
std::string usd(kicks::Price cents) { return "$" + text::thousands((cents + 50) / 100); }

// Averages a long price series down to at most `points` buckets for a compact sparkline.
std::vector<double> downsample(const std::vector<double>& values, std::size_t points) {
    if (values.size() <= points) return values;
    std::vector<double> out;
    for (std::size_t b = 0; b < points; ++b) {
        const std::size_t lo = b * values.size() / points;
        const std::size_t hi = (b + 1) * values.size() / points;
        double sum = 0.0;
        for (std::size_t i = lo; i < hi; ++i) sum += values[i];
        out.push_back(sum / static_cast<double>(hi - lo));
    }
    return out;
}

// Closing prices of the last `days` candles of a SKU, for sparklines.
std::vector<double> recent_closes(const kicks::Market& market, const std::string& sku, std::size_t days) {
    const auto& candles = market.candles(sku);
    const std::size_t first = candles.size() > days ? candles.size() - days : 0;
    std::vector<double> closes;
    for (std::size_t i = first; i < candles.size(); ++i) closes.push_back(static_cast<double>(candles[i].close));
    return closes;
}

} // namespace

std::string render_opening(const Universe& universe) {
    const auto& season = universe.season();
    const auto& market = universe.market();

    std::string out = rule(kWidth, "OPENING NIGHT · SIGNATURE DROPS", palette::gold) + "\n\n";
    Table table({"SKU", "Sneaker", "Athlete", "Retail", "Pairs", "Hype"});
    table.accent(palette::dark).align(3, Align::Right).align(4, Align::Right);
    for (const auto& shoe : universe.signatures()) {
        const hoops::Player& player = season.player(shoe.player_id);
        const hoops::Team& team = season.team(shoe.team_id);
        table.add_row({paint_bold(shoe.sneaker.sku, palette::cyan),
                       shoe.sneaker.display_name(),
                       player.short_name() + " " + team_tag(team),
                       usd(shoe.sneaker.retail),
                       text::thousands(shoe.sneaker.pairs),
                       paint(meter(market.hype(shoe.sneaker.sku), 10), palette::pink)});
    }
    out += table.render();
    out += paint(std::format("  {} stars, {} shoes, raffled at retail tonight. Every bucket moves the "
                             "market from here on.\n",
                             universe.signatures().size(), universe.signatures().size()),
                 palette::gray);
    return out;
}

std::string render_week(const WeekReport& report, const Universe& universe) {
    const auto& season = universe.season();
    const auto& market = universe.market();

    const std::string days = report.first_day == report.last_day
                                 ? std::format("game day {}", report.first_day)
                                 : std::format("game days {}–{}", report.first_day, report.last_day);
    std::string out = '\n' + rule(kWidth, std::format("WEEK {} · {} · {} games", report.week, days,
                                                      report.games.size()),
                                  palette::gold) +
                      "\n\n";

    // Scoreboard: the week's closest game and its biggest blowout.
    if (!report.games.empty()) {
        const auto [closest, widest] = std::minmax_element(
            report.games.begin(), report.games.end(),
            [](const hoops::GameResult& a, const hoops::GameResult& b) { return a.margin() < b.margin(); });
        const auto line = [&](const hoops::GameResult& g, std::string_view label) {
            const hoops::Team& home = season.team(g.home_id);
            const hoops::Team& away = season.team(g.away_id);
            return std::format("  {} {} {:>3}  @  {} {:>3}{}\n", paint(pad_right(label, 9), palette::gray),
                               team_tag(away), g.away_score, team_tag(home), g.home_score,
                               g.overtimes ? paint(std::format("  ({}OT)", g.overtimes), palette::orange) : "");
        };
        out += section("SCOREBOARD", palette::orange);
        out += line(*closest, "thriller");
        out += line(*widest, "blowout");
        out += '\n';
    }

    if (!report.big_nights.empty()) {
        out += section("BIG NIGHTS", palette::orange);
        auto nights = report.big_nights;
        std::stable_sort(nights.begin(), nights.end(), [](const BigNight& a, const BigNight& b) {
            return a.line.game_score() > b.line.game_score();
        });
        for (std::size_t i = 0; i < std::min<std::size_t>(nights.size(), 4); ++i) {
            const auto& n = nights[i];
            out += std::format("  {} {}  {}\n", paint("●", season.team(n.team_id).color),
                               paint(std::format("day {:>2}", n.day), palette::gray), n.headline);
        }
        out += '\n';
    }

    if (!report.hype_moves.empty()) {
        out += section("THE MARKET REACTS", palette::cyan);
        auto moves = report.hype_moves;
        std::stable_sort(moves.begin(), moves.end(), [](const HypeMove& a, const HypeMove& b) {
            return std::abs(a.magnitude) > std::abs(b.magnitude);
        });
        for (std::size_t i = 0; i < std::min<std::size_t>(moves.size(), 5); ++i) {
            const auto& m = moves[i];
            out += std::format("  {} {}  {}  {}\n", arrow(m.magnitude),
                               pad_left(change_text(m.magnitude), 7),
                               paint_bold(pad_right(m.sku, 10), palette::cyan), m.headline);
        }
        out += '\n';
    }

    // Signature board: where every signature shoe closed this week.
    out += section("SIGNATURE BOARD", palette::cyan);
    Table board({"SKU", "Athlete", "Close", "Week", "vs retail", "Last 10 days"});
    board.accent(palette::dark).align_right_from(2).align(5, Align::Left);
    const int week_days = report.last_day - report.first_day + 1;
    for (const auto& shoe : universe.signatures()) {
        const auto& candles = market.candles(shoe.sneaker.sku);
        if (candles.empty()) continue;
        const std::size_t back = std::min<std::size_t>(candles.size(), static_cast<std::size_t>(week_days) + 1);
        const double start = static_cast<double>(candles[candles.size() - back].close);
        const double close = static_cast<double>(candles.back().close);
        const auto spark = recent_closes(market, shoe.sneaker.sku, 10);
        board.add_row({paint_bold(shoe.sneaker.sku, palette::cyan), athlete(season, shoe.player_id),
                       usd(candles.back().close),
                       change_text(start > 0 ? close / start - 1.0 : 0.0),
                       change_text(close / static_cast<double>(shoe.sneaker.retail) - 1.0),
                       paint(sparkline(spark), palette::cyan)});
    }
    out += board.render() + '\n';

    if (report.player_of_the_week) {
        const auto& p = *report.player_of_the_week;
        const hoops::Player& player = season.player(p.player_id);
        const hoops::Team& team = season.team(p.team_id);
        std::vector<std::string> lines;
        lines.push_back(paint_bold(player.display_name(), palette::white) + "  " +
                        paint(team.full_name(), team.color));
        lines.push_back(std::format("{} games · {} ppg · {} rpg · {} apg · Game Score {}", p.games,
                                    text::fixed(p.ppg, 1), text::fixed(p.rpg, 1),
                                    text::fixed(p.apg, 1), text::fixed(p.game_score, 1)));
        if (report.verse) {
            lines.emplace_back();
            lines.push_back(paint("THE BOOTH", palette::pink) +
                            paint(std::format("  a ghostwritten verse, analyzed by the bars engine · "
                                              "rhyme density {}",
                                              text::percent(report.verse->rhyme_density, 0)),
                                  palette::gray));
            for (const auto& l : text::split_lines(report.verse->rendered))
                if (!text::trim(strip_ansi(l)).empty()) lines.push_back(l);
        }
        if (report.anthem) {
            const auto& a = *report.anthem;
            lines.emplace_back();
            lines.push_back(paint("THE STUDIO", palette::purple) +
                            paint(std::format("  \"{}\" · {} · {} BPM · {} · {}s", a.title, a.style,
                                              a.bpm, a.key, text::fixed(a.seconds, 1)),
                                  palette::gray));
            for (const auto& l : text::split_lines(a.waveform)) lines.push_back(paint(l, palette::purple));
            if (a.file)
                lines.push_back(paint("saved to " + a.file->generic_string(), palette::gray));
        }
        out += panel("PLAYER OF THE WEEK", lines, palette::gold, kWidth - 4);
    }
    return out;
}

std::string render_recap(const Recap& recap, const Universe& universe) {
    const auto& season = universe.season();
    std::string out = '\n' + rule(kWidth, recap.season_complete ? "SEASON RECAP" : "RECAP SO FAR",
                                  palette::gold) +
                      "\n\n";
    out += paint(std::format("  {} weeks · {} game days · {} games\n\n", recap.weeks_played,
                             recap.days_played, text::thousands(static_cast<std::int64_t>(season.games_played()))),
                 palette::gray);

    out += hoops::render_standings(season, universe.config().playoff_teams) + '\n';
    if (recap.playoffs) {
        out += hoops::render_bracket(*recap.playoffs, season.teams()) + '\n';
        const hoops::Team& champion = season.team(recap.playoffs->champion_id);
        const hoops::Player& mvp = season.player(recap.playoffs->finals_mvp_id);
        out += "  " + paint_bold("CHAMPIONS  ", palette::gold) + paint_bold(champion.full_name(), champion.color) +
               paint("   finals MVP: ", palette::gray) + mvp.display_name() + "\n\n";
    }
    out += hoops::render_mvp_race(season, 3) + '\n';

    out += section("THE CULTURE INDEX", palette::cyan);
    out += paint("  Did the shoe follow the athlete?  r = correlation of nightly Game Score vs. "
                 "same-day return\n",
                 palette::gray);
    Table index({"SKU", "Athlete", "Retail", "Open", "Close", "Season", "Premium", "Trend", "r"});
    index.accent(palette::dark).align_right_from(2).align(7, Align::Left);
    const SignatureReport* strongest = nullptr;
    for (const auto& s : recap.signatures) {
        if (s.games >= 5 && (strongest == nullptr || s.correlation > strongest->correlation)) strongest = &s;
        const double r = s.correlation;
        index.add_row({paint_bold(s.shoe.sneaker.sku, palette::cyan), athlete(season, s.shoe.player_id),
                       usd(s.shoe.sneaker.retail), usd(s.open), usd(s.close),
                       change_text(s.change), change_text(s.premium),
                       paint(sparkline(downsample(s.closes, 16)), palette::cyan),
                       s.games >= 5 ? paint(text::fixed(r, 2), r >= 0.3 ? palette::green : palette::gray)
                                    : paint("n/a", palette::dark)});
    }
    out += index.render();
    if (strongest != nullptr) {
        const hoops::Player& p = season.player(strongest->shoe.player_id);
        out += paint(std::format("  Strongest link: {} and the {} (r = {}). When they cooked, the "
                                 "resale market noticed.\n",
                                 p.display_name(), strongest->shoe.sneaker.name,
                                 text::fixed(strongest->correlation, 2)),
                     palette::gray);
    }

    if (!recap.mixtape.empty()) {
        out += '\n' + section("THE MIXTAPE", palette::purple);
        Table tracks({"#", "Track", "Player of the Week", "Style", "BPM", "Key", "Length"});
        tracks.accent(palette::dark).align(0, Align::Right).align(4, Align::Right).align(6, Align::Right);
        int n = 0;
        for (const auto& a : recap.mixtape) {
            tracks.add_row({std::to_string(++n), paint_bold(a.title, palette::purple),
                            std::format("week {} · {}", a.week, athlete(season, a.player_id)), a.style,
                            std::to_string(a.bpm), a.key,
                            std::format("{}:{:02}", static_cast<int>(a.seconds) / 60,
                                        static_cast<int>(a.seconds) % 60)});
        }
        out += tracks.render();
        if (!recap.mixtape.empty() && recap.mixtape.front().file)
            out += paint("  WAV files in " + recap.mixtape.front().file->parent_path().generic_string() + "\n",
                         palette::gray);
    }
    return out;
}

} // namespace street::culture
