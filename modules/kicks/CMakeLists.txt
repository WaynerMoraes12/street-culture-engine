street_add_module(kicks
    SOURCES
        src/agents.cpp
        src/benchmark.cpp
        src/catalog.cpp
        src/floor.cpp
        src/format.cpp
        src/hype.cpp
        src/market.cpp
        src/news.cpp
        src/order_book.cpp
        src/render_board.cpp
        src/render_candles.cpp
        src/render_depth.cpp
        src/sneaker_art.cpp
    DEPS street::core)

street_add_tests(street_kicks_tests
    SOURCES
        tests/test_market.cpp
        tests/test_order_book.cpp
        tests/test_render.cpp
    LIBS street::kicks)
