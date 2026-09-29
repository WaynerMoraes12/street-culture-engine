# `third_party`

| Library | Version | License | Used for |
|---|---|---|---|
| [doctest](https://github.com/doctest/doctest) | 2.4.12 | MIT ([LICENSE.txt](doctest/LICENSE.txt)) | unit tests only |

doctest is vendored as its single header, so tests build offline, with no package
manager, on every CMake version the project supports. `main.cpp` compiles doctest's
`main()` once into the `street::doctest_main` library, and every test executable links
against it.

The `street` binary itself has **zero** third-party dependencies.
