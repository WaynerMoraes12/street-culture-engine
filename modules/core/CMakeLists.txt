street_add_module(core
    SOURCES
        src/cli.cpp
        src/rng.cpp
        src/stats.cpp
        src/table.cpp
        src/term.cpp
        src/text.cpp)

street_add_tests(street_core_tests
    SOURCES
        tests/test_cli.cpp
        tests/test_rng.cpp
        tests/test_support.cpp
        tests/test_text_term.cpp
    LIBS street::core)
