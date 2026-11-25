# Contributors

Thank you to everyone helping build, test, and refine **Rhenium** while it progresses through **Beta** toward `v1.0.0`!

---

## Creator & Core Maintainer

| Name | GitHub | Role |
| :--- | :--- | :--- |
| **irfandemir** | [@irfandemir00](https://github.com/irfandemir00) | Creator, Architecture & Core Maintainer |

---

## Community Contributors

> Rhenium is currently in **Beta (`v0.1.0-beta`)**. Community contributors whose pull requests, bug reports, documentation updates, or examples are merged will be recognized here.

<!-- Add new contributors below in alphabetical order:
| Name | GitHub | Contribution |
| :--- | :--- | :--- |
-->

---

## How to Contribute

Because Rhenium is still in Beta, your feedback and pull requests have a direct impact on the stability and direction of the `v1.0.0` API.

### 1. Fork & Clone

```powershell
git clone https://github.com/irfandemir00/Rhenium.git
cd Rhenium
```

### 2. Build & Run Tests

Make sure both **Debug** and **Release** builds pass cleanly with zero warnings before opening a pull request:

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

### 3. Contribution Guidelines

- **Keep the core lightweight:** Rhenium must never require Node.js, npm, or external JS toolchains at runtime or build time.
- **Compiler strictness:** Code must compile cleanly with `-Wall -Wextra -Wpedantic` (Clang/GCC) and `/W4` (MSVC) under **C++20**.
- **Security first:** Respect the `https://rhenium.localhost` origin boundary and `rhenium::security::FsScope` rules for any new native bridge commands.
- **Add tests:** Cover new core, security, config, asset, or IPC behaviors in [`tests/test_core.cpp`](tests/test_core.cpp).
- **Focused PRs:** Keep pull requests scoped to a single bug fix, feature, or documentation improvement with a clear description of how it was tested.

### Ways to Help During Beta

- 🐛 **Report bugs:** Open an issue on [GitHub Issues](https://github.com/irfandemir00/Rhenium/issues) with your Windows version, compiler, and reproduction steps.
- 🖥️ **Test edge cases:** Try frameless windows across multi-monitor and mixed-DPI setups.
- 📚 **Improve docs & examples:** Share sample apps, guides, or API clarifications.
- 🧩 **Build roadmap features:** Check the Beta Roadmap in [`README.md`](README.md) if you want to help with upcoming capabilities.
