street_add_module(culture
    SOURCES
        src/legends.cpp
        src/render.cpp
        src/signature.cpp
        src/universe.cpp
    DEPS street::core street::beats street::bars street::hoops street::kicks)

street_add_tests(street_culture_tests
    SOURCES
        tests/test_legends.cpp
        tests/test_universe.cpp
    LIBS street::culture)
