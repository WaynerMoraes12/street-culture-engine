# doctest 2.4.12 (MIT) — vendored single header, see doctest/LICENSE.txt
add_library(street_doctest_main STATIC doctest/main.cpp)
add_library(street::doctest_main ALIAS street_doctest_main)
target_include_directories(street_doctest_main SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(street_doctest_main PUBLIC cxx_std_20)
