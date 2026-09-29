#pragma once
// street::hoops — possession-level basketball simulation.
//
// Umbrella header: the league model, the game engine, seasons and playoffs,
// Monte Carlo title odds and the text renderers.
//
//   auto league = street::hoops::default_league();
//   street::Rng rng(42);
//   auto game = street::hoops::simulate_game(league[0], league[1], rng);
//   std::cout << street::hoops::render_box_score(game, league[0], league[1]);

#include "street/hoops/game.hpp"
#include "street/hoops/league.hpp"
#include "street/hoops/model.hpp"
#include "street/hoops/monte_carlo.hpp"
#include "street/hoops/render.hpp"
#include "street/hoops/season.hpp"
