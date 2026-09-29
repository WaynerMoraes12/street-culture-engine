cmake_minimum_required(VERSION 3.21)

project(street_culture_engine
    VERSION 1.0.0
    DESCRIPTION "Hoops, kicks & hip-hop: a street-culture engine in modern C++20"
    HOMEPAGE_URL "https://github.com/WaynerMoraes12/street-culture-engine"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(STREET_BUILD_TESTS "Build the unit test suites" ON)
option(STREET_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(STREET_SANITIZE "Build with AddressSanitizer + UndefinedBehaviorSanitizer" OFF)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(StreetHelpers)

# Source files contain UTF-8 box-drawing characters and accents.
add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
street_enable_sanitizers()

if(STREET_BUILD_TESTS)
    enable_testing()
    add_subdirectory(third_party)
endif()

add_subdirectory(modules/core)
add_subdirectory(modules/beats)
add_subdirectory(modules/bars)
add_subdirectory(modules/hoops)
add_subdirectory(modules/kicks)
add_subdirectory(modules/culture)
add_subdirectory(apps/street)
