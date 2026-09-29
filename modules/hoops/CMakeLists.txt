find_package(Threads REQUIRED)

street_add_module(hoops
    SOURCES
        src/commentary.cpp
        src/engine.cpp
        src/game_result.cpp
        src/league.cpp
        src/model.cpp
        src/monte_carlo.cpp
        src/playoffs.cpp
        src/render_game.cpp
        src/render_league.cpp
        src/render_season.cpp
        src/render_shot_chart.cpp
        src/render_util.cpp
        src/season.cpp
        src/sim_team.cpp
    DEPS street::core Threads::Threads)

street_add_tests(street_hoops_tests
    SOURCES
        tests/test_game.cpp
        tests/test_league.cpp
        tests/test_render.cpp
        tests/test_season.cpp
    LIBS street::hoops)
