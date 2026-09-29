add_executable(street
    main.cpp
    banner.cpp
    cmd_bars.cpp
    cmd_beat.cpp
    cmd_culture.cpp
    cmd_game.cpp
    cmd_legends.cpp
    cmd_market.cpp
    cmd_season.cpp
    cmd_teams.cpp)

target_include_directories(street PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(street PRIVATE street::core street::beats street::bars street::hoops street::kicks street::culture)
street_apply_warnings(street)

if(STREET_BUILD_TESTS)
    add_test(NAME street_cli_version COMMAND street --version)
    add_test(NAME street_cli_banner COMMAND street --no-color)
    add_test(NAME street_cli_unknown COMMAND street dunk)
    set_tests_properties(street_cli_unknown PROPERTIES WILL_FAIL TRUE)
endif()
