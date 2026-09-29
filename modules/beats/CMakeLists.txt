street_add_module(beats
    SOURCES
        src/audio.cpp
        src/composer.cpp
        src/dsp.cpp
        src/grooves.cpp
        src/grooves_80s.cpp
        src/kit.cpp
        src/pattern.cpp
        src/render.cpp
        src/styles.cpp
        src/synth.cpp
        src/theory.cpp
        src/visual.cpp
        src/voices.cpp
    DEPS street::core)

street_add_tests(street_beats_tests
    SOURCES
        tests/test_audio.cpp
        tests/test_beats.cpp
        tests/test_dsp.cpp
        tests/test_pattern.cpp
        tests/test_theory.cpp
        tests/test_visual.cpp
    LIBS street::beats)
