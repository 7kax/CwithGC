# CwithGC

CwithGC is a small garbage-collection runtime with a C11 API and C++20 implementations of
reference-counting, copying, and mark-and-sweep collectors.

## Build and Test

Building the project requires CMake 3.20 or newer and Ninja. The checked-in CMake presets provide
the supported local validation configurations:

```sh
cmake --preset default
cmake --build --preset default
ctest --preset default
```

Use the `strict` preset to treat compiler warnings as errors, except for the tracked root-frame
warning documented in `TODO.md`. The `sanitizers` preset additionally enables AddressSanitizer
and UndefinedBehaviorSanitizer:

```sh
cmake --preset strict
cmake --build --preset strict
ctest --preset strict

cmake --preset sanitizers
cmake --build --preset sanitizers
ctest --preset sanitizers
```

The debug inspection API is enabled by default because the test suite uses it. Disable it for a
library-only build:

```sh
cmake --preset library
cmake --build --preset library
```

## Formatting

The project uses the root `.clang-format` file for C, C++, and headers:

```sh
cmake --build --preset default --target format-check
cmake --build --preset default --target format
```

Run all repository-level formatting and English-language checks with:

```sh
cmake --build --preset default --target quality-check
```
