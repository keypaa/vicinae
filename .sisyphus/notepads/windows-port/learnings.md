## [2026-06-05 14:02] Session: ses_16863307fffebiEnfooz4EgCge — Atlas

### Environment
- OS: Windows 11
- Compiler: MinGW-W64 x86_64-ucrt-posix-seh 13.2.0 (g++)
- CMake: 3.29.2
- Ninja: 1.12.0
- Python: 3.13.12
- Package managers: Scoop 0.5.3, winget, pip
- Qt 6.8.3 installed at C:\Qt\6.8.3\mingw_64

### CMake Bugs Fixed (T0.2)

1. `src/server/CMakeLists.txt` — `Qt6::DBus` removed from unconditional LIBS, added `find_package(Qt6 COMPONENTS DBus)` (optional) and `if(Qt6DBus_FOUND)` conditional block.
2. `src/server/CMakeLists.txt` — `else()` → `elseif(UNIX AND NOT APPLE)` to prevent linux-app-runtime from being compiled on Windows.
3. `src/server/CMakeLists.txt` — Wayland plugin import gated with `AND UNIX AND NOT APPLE`.
4. `src/server/src/pch.h` — DBus includes wrapped in `#ifdef Q_OS_LINUX`.
5. `vendor/zip/CMakeLists.txt` — Already correct (`if(UNIX)` excludes Windows), no change needed.

### CMake Bugs Found (T0.2)

1. `src/server/CMakeLists.txt:14` — `Qt6::DBus` in LIBS unconditionally
2. `src/server/CMakeLists.txt:608` — `else()` after `if (APPLE)` adds linux-app-runtime on Windows
3. `src/server/CMakeLists.txt:927-931` — Wayland plugin import gated only on `Qt6_FOUND`, not platform
4. `src/server/src/pch.h:16-17` — DBus includes unconditional
5. `vendor/zip/CMakeLists.txt:4` — `if(UNIX)` only defines _FILE_OFFSET_BITS and symlinks

### GuiPrivate Made Optional (T0.2 follow-up)

- `GuiPrivate` removed from REQUIRED `find_package(Qt6 ...)` — now found with `find_package(Qt6 COMPONENTS GuiPrivate QUIET)`.
- `Qt6::GuiPrivate` moved to `if(Qt6GuiPrivate_FOUND)` block in LIBS. Not available in Qt for MinGW on Windows.

### KF6 Guards Added (T0.2 follow-up)

1. Root `CMakeLists.txt:67` — `if ((NOT USE_SYSTEM_KF6 OR APPLE) AND NOT WIN32)` skips fetching/building KF6 on Windows (ECM not available).
2. `src/server/CMakeLists.txt:10` — `if (USE_SYSTEM_KF6 AND NOT APPLE AND NOT WIN32)` skips find_package.
3. `src/server/CMakeLists.txt:32-34` — `KF6::SyntaxHighlighting` moved from main LIBS to `if(NOT WIN32)` block.

### Build Strategy
- Use MinGW first (already installed, GCC-compatible flags)
- Existing GCC flags (-O3, -Wall, -Wextra, etc.) work with MinGW
- T0.3 (MSVC flags) is lower priority — MinGW works with existing flags
- sqlcipher `-w` flag works with MinGW (GCC-compatible)

## xdgpp Linux-only guarding (2026-06-05)

### Summary
Made the xdgpp library and all its usages Linux-only for the Windows port.

### Files changed (20)

#### CMake Changes
- **Root `CMakeLists.txt`**: Wrapped `add_subdirectory(${LIB_DIR}/xdgpp)` in `if (UNIX AND NOT APPLE)`
- **`src/server/CMakeLists.txt`**: Moved `vicinae::xdgpp` from main LIBS list to `if (UNIX AND NOT APPLE)` conditional block

#### Core xdgpp files guarded entirely
All xdg-app service files wrapped with `#ifdef Q_OS_LINUX`:
- `src/server/src/services/app-service/xdg/xdg-app.hpp`
- `src/server/src/services/app-service/xdg/xdg-app.cpp`
- `src/server/src/services/app-service/xdg/xdg-app-database.hpp`
- `src/server/src/services/app-service/xdg/xdg-app-database.cpp`
- `src/server/src/services/app-service/xdg/xdg-app-action.hpp`

#### Source files with conditional xdgpp includes
- `vicinae.cpp`: Changed `#ifndef Q_OS_MACOS` → `#ifdef Q_OS_LINUX`
- `environment.hpp`: Split into cross-platform base + Linux-only section guarded by `#ifdef Q_OS_LINUX`
- `telemetry-service.cpp`: Guarded `xdgpp::currentDesktop()` and `xdgpp::dataHome()` calls
- `soulver-core.cpp`: Guarded xdgpp include (file is already Linux-only in CMake)
- `system-run-view-host.cpp`: Guarded `xdgpp::ExecParser` include and usage
- `system-extension.hpp`: Guarded `xdgpp::ExecParser` include and usage

#### App service platform branching
- `app-service.cpp`: Changed from macOS/else to macOS/Linux/Windows with `DummyAppDatabase` fallback on Windows

#### Call-site guards for Linux-only Environment functions
- `server.cpp`: Guarded `detectAppLauncher()` and `fallbackIconSearchPaths()` calls
- `navigation-controller.cpp`: Guarded `isHudDisabled()` call
- `launcher-window.cpp`: Guarded `isHudDisabled()`, `supportsArbitraryWindowPlacement()`, `isLayerShellSupported()`
- `oauth-overlay-host.cpp`: Guarded `isLayerShellSupported()` call
- `bug-report-url.hpp`: Guarded `getEnvironmentDescription()` with fallback to "unknown"

## [2026-06-05 14:15] Fixed std::filesystem::path::c_str() calls

### Changes Made
- `src/server/src/qml/browse-apps-model.cpp:48,49,57` — Changed `app->path().c_str()` to `QString::fromStdPath(app->path())`
  - `findDefaultOpener()` takes `const QString &` (Target = QString)
  - `OpenAppAction` third arg is `const std::vector<QString> &`
  - `Clipboard::Text` first member is `QString`
- `src/server/src/root-search/scripts/script-root-provider.hpp:95` — Changed `m_file->path().c_str()` to `QString::fromStdPath(m_file->path())`
  - Same `Clipboard::Text` pattern
- `src/server/src/theme/theme-parser.cpp:278` — Already had `icon()->string()` (no change needed)
- `src/server/src/theme/theme-compiler.cpp` — Does not exist

### Rationale
- On Windows/MinGW, `std::filesystem::path::c_str()` returns `const wchar_t*`, not `const char*`
- `std::quoted<char>()` fails with `const wchar_t*`
- `QString::fromStdPath()` is the Qt6-recommended portable way to convert `std::filesystem::path` to `QString`
- `path::string()` returns UTF-8 `std::string` for use with `std::quoted` and streams

### Build Result (2026-06-05)
- Build was progressing (compiled figura binary, ran code generation)
- Then hit errors in theme-parser.cpp, browse-apps-model.cpp, script-root-provider.hpp
- All three fixed with `.string()` or `QString::fromStdPath()` for `path::c_str()` calls
- `theme-compiler.cpp` does NOT exist in source tree (error from truncated build log was likely misattributed)
- Need to run build again to discover next set of errors

### Key Design Decisions

#### environment.hpp split approach
Instead of guarding the entire file (which would break cross-platform callers), split into:
1. **Cross-platform** (outside guard): `appImageDir()`, `nodeBinaryOverride()`, `isAppImage()`, `vicinaeApiBaseUrl()`
2. **Linux-only** (inside `#ifdef Q_OS_LINUX`): All desktop environment detection, Wayland, XDG, chassis type, icon paths, etc.

#### vicinae.cpp Windows alternatives
- `dataDir()`: Uses `QStandardPaths::GenericDataLocation` on both Windows and macOS
- `configDir()`: Falls back to `dataDir()` on both Windows and macOS
- `stateDir()`: Falls back to `dataDir()` on both Windows and macOS
- `systemDataDirs()`: Mirrors macOS behavior (bundle resource dir)
