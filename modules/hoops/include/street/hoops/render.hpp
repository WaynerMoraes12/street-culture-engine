#pragma once
// Text renderers for games, seasons and the league.
//
// All renderers are pure functions returning a string that fits ~100
// columns. With colors enabled they use team colors and highlights; with
// term::set_color_enabled(false) they emit clean, aligned plain text.

#include <cstddef>
#include <string>
#include <vector>

#include "street/hoops/game.hpp"
#include "street/hoops/model.hpp"
#include "street/hoops/monte_carlo.hpp"
#include "street/hoops/season.hpp"

namespace street::hoops {

// ── games ────────────────────────────────────────────────────────────────────
/// Final score banner with the quarter-by-quarter line.
std::string render_scoreboard(const GameResult& result, const Team& home, const Team& away);
/// Both teams' box scores (starters, bench, totals, shooting splits); the night's
/// top performer is highlighted.
std::string render_box_score(const GameResult& result, const Team& home, const Team& away);
/// ASCII half court with every attempt of one side plus zone shooting splits.
std::string render_shot_chart(const GameResult& result, int side, const Team& team);
/// One play-by-play line: "Q4 01:12  QNS 101-99 BRX  Greene buries the step-back three!"
std::string render_play(const Play& play, const Team& home, const Team& away);

// ── seasons ──────────────────────────────────────────────────────────────────
/// Standings: rank, team, W, L, PCT, GB, DIFF, STRK, ELO (with a playoff cut line).
std::string render_standings(const Season& season, int playoff_teams = 8);
/// Points / rebounds / assists leaders side by side.
std::string render_leaders(const Season& season, std::size_t count = 5);
std::string render_mvp_race(const Season& season, std::size_t count = 5);
/// ASCII playoff bracket from the first round to the champion.
std::string render_bracket(const PlayoffResult& playoffs, const std::vector<Team>& teams);
/// Title / finals / playoff odds with meters.
std::string render_title_odds(const std::vector<TitleOdds>& odds, const std::vector<Team>& teams);

// ── league ───────────────────────────────────────────────────────────────────
/// One row per team: identity, pace, stars, overall.
std::string render_league(const std::vector<Team>& teams);
/// A team card: identity and full roster with all twelve ratings.
std::string render_roster(const Team& team);

} // namespace street::hoops
