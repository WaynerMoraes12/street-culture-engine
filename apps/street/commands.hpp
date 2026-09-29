#pragma once
// Entry points for every `street <command>`.
//
// Conventions: a command receives the tokens after its name, returns the
// process exit code, and throws street::cli::UsageError for bad input (main()
// prints it with a hint and exits with status 2).

#include <span>
#include <string>

namespace street::app {

using Args = std::span<const std::string>;

int cmd_beat(Args args);    // beats  — procedural hip-hop beat → WAV
int cmd_bars(Args args);    // bars   — lyric / flow analyzer
int cmd_game(Args args);    // hoops  — single game with play-by-play
int cmd_season(Args args);  // hoops  — season, playoffs, Monte Carlo odds
int cmd_teams(Args args);   // hoops  — league teams and rosters
int cmd_market(Args args);  // kicks  — sneaker resale exchange
int cmd_culture(Args args); // culture — everything, connected
int cmd_legends(Args args); // culture — the real history, 1973 to today

void print_banner();

} // namespace street::app
