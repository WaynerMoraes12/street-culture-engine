# `cmake` — build helpers

[`StreetHelpers.cmake`](StreetHelpers.cmake) keeps every module's `CMakeLists.txt` down to a few
lines:

| Function | What it does |
|---|---|
| `street_add_module(<name> SOURCES … DEPS …)` | creates the static library `street_<name>` with alias `street::<name>`, exposes `include/` publicly, requires C++20 and applies the warning set |
| `street_add_tests(<name> SOURCES … LIBS …)` | builds a doctest executable, links `street::doctest_main`, and registers it with CTest (run from the repo root) |
| `street_apply_warnings(<target>)` | MSVC: `/W4 /permissive- /Zc:__cplusplus`; GCC/Clang: `-Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor -Woverloaded-virtual -Wimplicit-fallthrough`; adds `/WX` or `-Werror` when `STREET_WARNINGS_AS_ERRORS=ON` |
| `street_enable_sanitizers()` | adds AddressSanitizer + UndefinedBehaviorSanitizer when `STREET_SANITIZE=ON` (ASan on MSVC) |

A whole module declaration looks like this:

```cmake
street_add_module(kicks SOURCES src/order_book.cpp src/market.cpp DEPS street::core)
street_add_tests(street_kicks_tests SOURCES tests/test_order_book.cpp LIBS street::kicks)
```

## Options

| Option | Default | Effect |
|---|---|---|
| `STREET_BUILD_TESTS` | `ON` | build the doctest suites and CTest entries |
| `STREET_WARNINGS_AS_ERRORS` | `OFF` (CI: `ON`) | fail the build on any warning |
| `STREET_SANITIZE` | `OFF` | ASan + UBSan instrumentation |

## Presets

[`CMakePresets.json`](../CMakePresets.json) defines `release`, `debug` and `sanitize`
(Ninja) plus `msvc` (Visual Studio 2022), each with matching build and test presets:

```bash
cmake --preset sanitize && cmake --build --preset sanitize && ctest --preset sanitize
```
