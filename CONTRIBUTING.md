# Contributing to Rhenium

Thank you for your interest in contributing to **Rhenium**! Rhenium is currently in **Beta (`v0.1.0-beta`)**, so bug reports, tests, docs, and pull requests have a huge impact on shaping `v1.0.0`.

## Getting Started

1. **Fork & clone** the repository:
   ```powershell
   git clone https://github.com/irfandemir00/Rhenium.git
   cd Rhenium
   ```
2. **Build & run tests** in both Debug and Release configurations:
   ```powershell
   # Debug build + unit tests
   cmake -B build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
   cmake --build build-debug
   ctest --test-dir build-debug --output-on-failure

   # Release build (embedded assets) + unit tests
   cmake -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DRHENIUM_EMBED_ASSETS=ON
   cmake --build build-release
   ctest --test-dir build-release --output-on-failure
   ```

## Guidelines

- **Zero Node.js requirement:** Keep the framework and templates free of Node.js, npm, or external JS bundler dependencies.
- **Compiler warnings:** Ensure changes compile with zero warnings under `-Wall -Wextra -Wpedantic` (Clang/GCC) and `/W4` (MSVC) in **C++20**.
- **Security model:** Preserve the `https://rhenium.localhost` origin boundary and `rhenium::security::FsScope` filesystem restrictions for any built-in bridge commands.
- **Unit tests:** Add or update tests in [`tests/test_core.cpp`](tests/test_core.cpp) for core, config, asset, security, or IPC changes.
- **Focused PRs:** Keep pull requests focused on a single bug fix, feature, or documentation improvement, and include reproduction or verification steps.

## Beta Focus Areas

- Bug reports across Windows 10/11 and multi-DPI monitor setups
- New `window.rhenium` native capabilities and `v1.0.0` roadmap items in [`README.md`](README.md)
- Additional example apps and documentation improvements

For the project maintainer and contributor roster, see [`Contributors.md`](Contributors.md).
