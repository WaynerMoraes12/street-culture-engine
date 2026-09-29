street_add_module(bars
    SOURCES
        src/analysis.cpp
        src/english.cpp
        src/english_rules.cpp
        src/ghostwriter.cpp
        src/language.cpp
        src/lexicon.cpp
        src/numbers.cpp
        src/phonetics.cpp
        src/portuguese.cpp
        src/render.cpp
        src/rhyme.cpp
        src/samples.cpp
        src/tokenizer.cpp
    DEPS street::core)

street_add_tests(street_bars_tests
    SOURCES
        tests/test_analysis.cpp
        tests/test_ghostwriter.cpp
        tests/test_language.cpp
        tests/test_lexicon.cpp
        tests/test_render.cpp
        tests/test_rhyme.cpp
        tests/test_syllables.cpp
        tests/test_tokenizer.cpp
    LIBS street::bars)
