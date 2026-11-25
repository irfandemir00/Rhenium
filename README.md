# Rhenium

**Rhenium** is a lightweight, desktop application framework for **Windows (Win32 + WebView2)** where a thin **C++20** native shell launches the window and OS bridge, while all application functions, state, and UI logic are written in **HTML, CSS, and JavaScript** — with zero Node.js or npm required.

## Features

- **Thin C++20 Native Host**: Start your application in a single line (`rhenium::App::from_config_file("rhenium.json").run()`).
- **JS-First Application Logic**: Write all application functions, state, and DOM handling in standard ES2020 JavaScript (`ui/js/`).
- **Built-In `window.rhenium` Native Bridge**: Promise-based OS APIs for window management, native file/folder/message dialogs, clipboard, async filesystem operations, and external URL launching.
- **Frameless & Modern Win32 UI**: DWM dark titlebar, frameless window resizing, and `[data-rh-drag-region]` drag/double-click maximize support.
- **Dev & Release Asset Flow**:
  - **Debug**: Reads directly from `ui/` on disk with `ReadDirectoryChangesW` live-reload (CSS hot-swap without full reload on `.css` edits, automatic reload on `.html`/`.js` edits).
  - **Release**: Embeds all `ui/` assets into the executable via `rhenium_pack` and serves them from memory over `https://rhenium.localhost/`.
- **Secure Defaults**: IPC is restricted strictly to `https://rhenium.localhost`; external links open in the default browser; Release builds disable DevTools, default context menus, and browser accelerator keys; customizable CSP via `rhenium.json`.

## Modular Directory Structure

```text
include/rhenium/
  rhenium.hpp                # Umbrella public header
  core/                      # Config, Security, AssetResolver, IpcRouter
  bridge/                    # Embedded window.rhenium JS SDK
  platform/                  # Win32 native APIs and Window host
  runtime/                   # App coordinator
src/
  core/                      # Core library (rhenium_core)
  bridge/                    # runtime_js.cpp + builtin_commands.cpp (rh:*)
  platform/win32/            # WebView2 loader, window, file watcher, dialogs/clipboard
  runtime/                   # app.cpp
tools/
  rhenium_pack/              # Static UI asset packer for Release builds
  rhenium_cli/               # CLI tool (main.cpp, process.cpp, templates.cpp)
examples/showcase/
  src/main.cpp               # Minimal C++ launcher
  ui/
    index.html
    styles/{tokens,layout,components}.css
    js/{state,ui,native,main}.js
```

## `window.rhenium` JavaScript API

- **`window.rhenium.window`**: `minimize()`, `toggleMaximize()`, `isMaximized()`, `close()`, `setTitle(title)`, `startDrag()`, `openDevTools()`, `reload()`
- **`window.rhenium.dialog`**: `openFile(options)`, `saveFile(options)`, `openFolder(options)`, `messageBox(options)`
- **`window.rhenium.clipboard`**: `readText()`, `writeText(text)`
- **`window.rhenium.fs`**: `readText(path)`, `writeText(path, content)`, `exists(path)`, `stat(path)`, `listDir(path)`
- **`window.rhenium.shell`**: `openExternal(url)`
- **`window.rhenium.app`**: `getInfo()`
- **`window.rhenium.invoke(cmd, args)`** & **`window.rhenium.on(eventName, cb)`**

## Requirements

- Windows 10/11 with Microsoft Edge WebView2 Runtime
- CMake 3.22+, Ninja, and a C++20 compiler (LLVM-MinGW `clang++`/`g++` or MSVC)

## Building & Testing

```powershell
# Debug build + CTest unit tests
cmake -B build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug
ctest --test-dir build-debug --output-on-failure

# Release build (embeds UI assets into the executable)
cmake -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DRHENIUM_EMBED_ASSETS=ON
cmake --build build-release
```

## Showcase & CLI

```powershell
# Run the Showcase application
.\build-debug\examples\showcase\rhenium_showcase.exe

# Scaffold, run in dev mode, and build a Release bundle (dist/<app>.exe + WebView2Loader.dll)
.\build-debug\tools\rhenium_cli\rhenium.exe init my_app
.\build-debug\tools\rhenium_cli\rhenium.exe dev my_app
.\build-debug\tools\rhenium_cli\rhenium.exe build my_app
```

## Minimal Application Example

`src/main.cpp`:
```cpp
#include <rhenium/rhenium.hpp>

int main() {
    return rhenium::App::from_config_file("rhenium.json").run();
}
```

`ui/app.js`:
```js
const rh = window.rhenium;

document.getElementById("open-btn").addEventListener("click", async () => {
  const filePath = await rh.dialog.openFile({ title: "Select a File" });
  if (filePath) {
    document.getElementById("output").textContent = await rh.fs.readText(filePath);
  }
});
```
