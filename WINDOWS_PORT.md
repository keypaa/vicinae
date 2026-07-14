# Vicinae Windows Port

## Current State

`vicinae.exe` (CLI), `vicinae-server.exe`, and `vicinae-browser-link.exe` — **ALL COMPILE AND LINK** on MinGW-w64 x86_64-ucrt-posix-seh 13.2.0, Qt 6.8.3.

---

## Environment

| Component | Path |
|---|---|
| Compiler | `C:/Strawberry/c/bin/c++.exe` (GCC 13.2.0) |
| Qt | `C:/Qt/6.8.3/mingw_64` |
| ICU | `C:/tools/icu-mingw/mingw64` |

## CMake Configure

```
cmake -S . -B build-qt-check \
  -G Ninja \
  -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/mingw_64" \
  -DCMAKE_BUILD_TYPE=Release \
  -DWAYLAND_LAYER_SHELL=OFF \
  -DWLR_DATA_CONTROL=OFF \
  -DUSE_SYSTEM_KF6=OFF \
  -DUSE_SYSTEM_QT_KEYCHAIN=OFF \
  -DUSE_SYSTEM_GLAZE=OFF \
  -DUSE_SYSTEM_CMARK_GFM=OFF \
  -DLIBQALCULATE_BACKEND=OFF
```

Build: `cmake --build build-qt-check --target vicinae-server` (or `--target vicinae` for the CLI)

---

## What Has Been Done

### 1. `std::filesystem::path::c_str()` → wchar_t portability

On Windows/MinGW, `fs::path::c_str()` returns `const wchar_t*` instead of `const char*`. This breaks every call site that passes a path to a `std::string`- or `const char*`-based API.

**Pattern applied (~30 files):**

| Context | Fix pattern |
|---|---|
| glaze APIs (`glz::read_file_json`, `glz::write_file_json`, `glz::read_file_jsonc`) | `path.c_str()` → `path.string()` |
| Qt APIs (`QFileSystemWatcher::addPath`, `QFile::QFile`, `QLocalServer::listen`, `QMimeDatabase::mimeTypeForFile`, `Clipboard::Text`) | `path.c_str()` → `QString::fromStdString(path.string())` |
| `qWarning()`, `qDebug()` stream output | `path.c_str()` → `path.string().c_str()` |
| `zip_open()` (C API) | `path.c_str()` → `path.string().c_str()` |
| `QTemporaryFile::filesystemFileName()` (Qt 6 returns `QString`) | `filesystemFileName().c_str()` → `fileName().toUtf8().constData()` |
| `soulver_initialize()` | `path.c_str()` → `path.string().c_str()` |
| `std::ofstream` constructor | Already accepts `fs::path` — no change needed |
| Assigning `fs::path` to `std::string` | Use `.string()` explicitly |

**Key insight**: `fs::path` has NO implicit conversion to `std::string` on Windows (native `string_type` is `std::wstring`). Every implicit conversion site explodes.

### 2. `std::ranges::to<Container>()` — GCC 13.2 limitation

`std::ranges::to` is not available in GCC 13.2 (introduced in GCC 14). Replaced with manual loops across ~15 files.

**Common patterns:**

```cpp
// Before (GCC 14+):
auto v = range | std::views::transform(...) | std::ranges::to<std::vector>();

// After (GCC 13):
std::vector<T> v;
for (auto &&elem : range | std::views::transform(...)) {
  v.emplace_back(std::move(elem));
}
```

For `std::ranges::to<QString>()` (joining `QString` parts):
```cpp
// Before:
result = parts | std::views::transform(...) | std::views::join | std::ranges::to<QString>();

// After:
QString result;
for (auto &&part : parts) { result += part.text; }
```

### 3. `std::println` — GCC 13.2 limitation

`std::println` is unavailable in GCC 13.2. Replaced all ~53 calls with `std::format` + `std::cerr <<`.

```cpp
// Before:
std::println(std::cerr, "Failed to open {}: {}", path, error);

// After:
std::cerr << std::format("Failed to open {}: {}", path, error) << "\n";
```

### 4. Windows SDK name conflicts

`enum class TokenType` in `src/lib/figura/src/lexer.hpp` conflicts with Windows SDK typedef `TOKEN_TYPE`. Changed to plain `enum TokenType`.

### 5. `QString::fromStdPath()` — not available in Qt 6.8 MinGW

Replaced all uses with `QString::fromStdString(path.string())`.

### 6. Platform-specific code guarded

| File | Guard |
|---|---|
| `src/server/src/services/power-manager/systemd/systemd-power-manager.cpp` | Entire file behind `#ifdef Q_OS_LINUX` |
| `src/server/src/services/app-runtime/app-runtime.cpp` | Added `WindowsAppRuntime` stub behind `#ifdef Q_OS_WIN` |
| `src/server/src/services/telemetry/telemetry-service.cpp` | `Environment::chassisType()` guarded `#ifdef Q_OS_LINUX` |
| `src/server/src/internal/pid-file/pid-file.cpp` | `::kill()` call guarded `#ifndef Q_OS_WIN` |
| `src/server/CMakeLists.txt` | xdgpp library guarded `if UNIX AND NOT APPLE` |
| `src/server/src/utils/environment.hpp` | `chassisType()` (Linux), sections restructured |
| `src/server/src/vicinae.cpp` | `#include <xdgpp/...>` guarded, Windows uses `QStandardPaths` |
| `src/server/src/workspace/workspace-extensions/workspace-file-search-ext.cpp` | Future: already behind `#ifdef Q_OS_LINUX` |
| Various `#include <xdgpp/...>` | Guarded `#ifdef Q_OS_LINUX` in 9+ source files |

### 7. `#include <print>` removed

Removed all `#include <print>` references (unavailable in GCC 13.2).

### 8. Linker fix: cmark-gfm library order

Swapped `${CMARK_LIBRARY} ${CMARK_EXT_LIBRARY}` → `${CMARK_EXT_LIBRARY} ${CMARK_LIBRARY}` in `src/server/CMakeLists.txt:18`. With MinGW's ld, static libraries are processed left-to-right; the extensions lib references `cmark_arena_push`/`cmark_arena_pop` from the core lib, so the core must come after.

### 9. Missing includes added

- `#include <set>` in `app-service.cpp`
- `#include <optional>` in `visit-tracker.hpp`
- `QString` → `const char*` conversion fixes in various files

### 10. Fix remaining compilation errors (previous sessions)

Files fixed in the latest session (each fixed 1 error in a new file until all compiled):

- `src/server/src/config/config.cpp` — 4 `path.c_str()` + 1 std::ranges::to
- `src/server/src/services/news/news-service.cpp` — `m_stateFile.c_str()`
- `src/server/src/qml/dmenu-model.cpp` — `.native()` → `.string()`
- `src/server/src/qml/dmenu-view-host.cpp` — std::ranges::to
- `src/server/src/services/telemetry/telemetry-service.cpp` — 3 `m_statePath.c_str()` + 1 std::ranges::to + 1 `chassisType()` guard
- `src/server/src/services/snippet/snippet-db.cpp` — 3 `m_path.c_str()`
- `src/server/src/services/window-manager/window-manager.cpp` — missing `AbstractWindowManager::` prefix
- `src/server/src/internal/zip/unzip.cpp` — `path.c_str()` + `filesystemFileName().c_str()`
- `src/server/src/internal/pid-file/pid-file.cpp` — `::kill()` guard
- `src/server/src/qml/root-search-sources.cpp` — `compressPath()` return type fix
- `src/server/src/qml/theme-list-model.cpp` — 2 `theme->path()->c_str()`
- `src/server/src/services/extension-boilerplate-generator/extension-boilerplate-generator.cpp` — implicit fs::path→string
- `src/server/src/qml/installed-extensions-model.cpp` — `m.path.c_str()`
- `src/server/src/services/snippet/snippet-copy.hpp` — `QString::arg(std::string)`
- `src/server/src/qml/manage-snippets-view-host.cpp` — std::ranges::to
- `src/server/src/services/app-runtime/app-runtime.cpp` — Windows stub
- `src/server/src/services/power-manager/systemd/systemd-power-manager.cpp` — full Linux guard
- Various `std::ranges::to<...>()` in `calculator-history-model.hpp`, `view-utils.cpp`, `xdg-app-database.cpp`, `snippet-copy.hpp`

### 11. CLI ported from POSIX sockets to QLocalSocket

**Files:**
- `src/cli/src/ipc-client.hpp` — Full rewrite of `IpcClient` to use `QLocalSocket` instead of `AF_UNIX` sockets
- `src/cli/CMakeLists.txt` — Added `find_package(Qt6 COMPONENTS Network)` and `Qt6::Network` link
- `src/cli/src/server.cpp` — `kill()`/`execv()` → `TerminateProcess()`/`CreateProcessA()` on Windows, `fs::path::c_str()` fixes
- `src/cli/src/theme.cpp` — `fs::path::c_str()` → `.string()` in `std::format`

`vicinae.exe` now compiles and links on MinGW-w64.

### 12. Browser link ported from POSIX sockets to QLocalSocket

**Files:**
- `src/browser-extension/src/browser.cpp` — Full rewrite from `AF_UNIX`/`poll()` to `QLocalSocket` + timeout-based event loop
- `src/browser-extension/CMakeLists.txt` — Added `find_package(Qt6 COMPONENTS Network)` and `Qt6::Network` link

**Key changes:**
- Removed `<netinet/in.h>`, `<sys/poll.h>`, `<sys/socket.h>`, `<sys/un.h>`, `<unistd.h>`
- Added `<QLocalSocket>`, platform headers (`<windows.h>`, `<io.h>`, `<fcntl.h>`)
- `socket(AF_UNIX)`/`connect()`/`close()` → `QLocalSocket::connectToServer()`/`waitForConnected()`
- `SocketTransport` wraps `QLocalSocket*` instead of `int fd`
- `poll()` replaced with `waitForReadyRead(50)` timeout loop + `PeekNamedPipe`/`poll()` stdin check
- New `VicinaeFrame` class reads from `QLocalSocket::readAll()`
- `Frame::readPart` uses `_read()` on Windows, `::read()` on POSIX
- `isatty(STDIN_FILENO)` → `_isatty(_fileno(stdin))` on Windows
- Stdin/stdout set to binary mode on Windows via `_setmode(..., _O_BINARY)`
- `socketPath()` returns `\\.\pipe\vicinae` on Windows

`vicinae-browser-link.exe` now compiles and links on MinGW-w64.

---

## What Remains

### A. ~~`vicinae` CLI — Unix domain socket to Windows named pipe~~ **DONE**

`vicinae.exe` compiles and links. The CLI was ported from POSIX `AF_UNIX` sockets to `QLocalSocket`:

**Changes:**
- `src/cli/src/ipc-client.hpp` — Replaced `socket(AF_UNIX)`/`::connect()`/`::send()`/`::recv()`/`::close()` with `QLocalSocket` (`connectToServer`, `write`, `waitForReadyRead`, `read`)
- `src/cli/CMakeLists.txt` — Added `Qt6::Network` dependency
- `src/cli/src/server.cpp` — Fixed `kill()`/`execv()` → `TerminateProcess()`/`CreateProcessA()` on Windows, `fs::path::c_str()` → `.string()`
- `src/cli/src/theme.cpp` — Fixed `fs::path::c_str()` → `.string()` in `std::format` call

The protocol stays the same (length-prefixed JSON-RPC messages). Socket path is `\\.\pipe\vicinae` on Windows, `$XDG_RUNTIME_DIR/vicinae/vicinae.sock` on Linux.

### B. ~~`vicinae-browser-link` — Unix domain socket + poll to Windows~~ **DONE**

`vicinae-browser-link.exe` compiles and links. The browser link was ported from POSIX `AF_UNIX`/`poll()` to `QLocalSocket`:

**Changes:**
- `src/browser-extension/src/browser.cpp` — Replaced `socket(AF_UNIX)`/`connect()`/`poll()`/`close()` with `QLocalSocket` (`connectToServer`, `waitForConnected`, `waitForReadyRead`, `readAll`)
- `src/browser-extension/CMakeLists.txt` — Added `Qt6::Network` dependency
- Native messaging (stdin/stdout) kept as-is — works cross-platform
- Stdin availability checked via `PeekNamedPipe` (Windows) or `poll()` with 0 timeout (POSIX)

### C. ~~Verify server runs on Windows~~ **DONE**

`vicinae-server.exe` was tested and runs successfully on Windows.

**Crash fixed — CalculatorExtension null backend:**
- `src/server/src/extensions/calculator/calculator-extension.hpp` — Added null check in `initialized()` before calling `calc->backend()->refreshExchangeRates()` (was SIGSEGV because no calculator backends are available on Windows)
- `src/server/src/services/calculator-service/dummy/dummy-calculator-backend.hpp` — **NEW**: Header-only dummy calculator backend following the project's existing dummy pattern (always activatable, returns error for compute calls)
- `src/server/src/services/calculator-service/calculator-service.cpp` — Added dummy backend as fallback when no real backends are available

**Fix — `vicinae://settings` deeplink:**
- `src/server/src/ipc-command-handler.cpp` — Bare `vicinae://settings` now opens the settings window (previously fell through to "invalid deeplink")

**Startup verification results:**

| Component | Status |
|---|---|
| Server startup (no SIGSEGV/SIGABRT) | ✅ |
| Clipboard server (dummy fallback) | ✅ |
| Extension manager (Node.js) | ✅ |
| Calculator (dummy backend) | ✅ |
| IPC command server (named pipe `vicinae`) | ✅ |
| Server stays alive indefinitely | ✅ |
| `vicinae version` | ✅ |
| `vicinae ping` | ✅ |
| `vicinae toggle` / `open` / `close` | ✅ |
| `vicinae state open` | ✅ |
| `vicinae deeplink vicinae://settings` | ✅ |
| `vicinae deeplink vicinae://settings/open` | ✅ |
| `vicinae deeplink vicinae://settings/close` | ✅ |

**Known non-critical warnings (harmless):**
1. "GnomeClipboardServer: GNOME environment detected" — false positive on Windows; falls back to dummy clipboard
2. "Failed to load visits file" / "Failed to parse news state" — expected on first run (files don't exist yet)
3. Dummy backends active: clipboard, calculator, window manager, app database, audio, notifications, power management

### D. Build system improvements

- The CLI (`src/cli/CMakeLists.txt`) now links `Qt6::Network` — **DONE**
- The browser link (`src/browser-extension/CMakeLists.txt`) now links `Qt6::Network` — **DONE**
- On Windows, these targets need `Qt6::Network` for `QLocalSocket`

---

## Detailed Implementation Guide

### CLI: `src/cli/src/ipc-client.hpp` — **DONE**

The POSIX-socket-based `IpcClient` was rewritten to use `QLocalSocket`. `SocketTransport` was kept (now wraps `QLocalSocket*` instead of `int fd`) since `ipc::AbstractTransport` is the interface expected by the generated `ipc::RpcTransport`. Key changes:

- POSIX headers replaced with `<QLocalSocket>`
- `connect()` uses `QLocalSocket::connectToServer()` + `waitForConnected(5000)`
- `send()` uses `QLocalSocket::write()` + `flush()`
- `recv()` uses `QLocalSocket::waitForReadyRead(30000)` + `read()`
- `QLocalSocket` held in `std::unique_ptr` (QObject is non-movable)
- `socketPath()` returns `\\.\pipe\vicinae` on Windows, existing Unix logic otherwise
- `server.cpp` and `theme.cpp` needed ancillary fixes (Windows process creation, `fs::path::c_str()` usage)

### Browser link: `src/browser-extension/src/browser.cpp`

Similar approach:
- Use `QLocalSocket` to connect to the vicinae server
- Remove all POSIX headers
- Replace `poll()` with `QLocalSocket::waitForReadyRead()` in a loop
- The native messaging stdin/stdout part stays the same (works cross-platform)
- `isatty()` → `_isatty(_fileno(stdin))` on Windows

**Poll() replacement strategy:**
Instead of `poll()` on two fds (stdin + server socket), use two threads or an event loop:
- Thread 1: Read stdin (native messaging input)
- Thread 2: Read from `QLocalSocket` (server responses/events)

Or use `WaitForMultipleObjects` on Windows.

Simplest approach: keep the synchronous loop but alternate between `stdin` and the `QLocalSocket`. Use `QLocalSocket::waitForReadyRead(timeout)` with a short timeout, and regularly check `stdin` for available data.

```cpp
// Simplified approach:
while (!m_shouldExit) {
  // Check server socket
  if (vicinaeSocket.state() == QLocalSocket::ConnectedState) {
    vicinaeSocket.waitForReadyRead(100);
    while (vicinaeSocket.bytesAvailable() > 0) {
      // read frame
    }
  }
  
  // Check stdin (native messaging input)
  DWORD avail;
  if (PeekNamedPipe(GetStdHandle(STD_INPUT_HANDLE), nullptr, 0, nullptr, &avail, nullptr) && avail > 0) {
    // read and process
  }
}
```

### Generating Figura codegen on Windows

The `figura` tool (codegen) already compiles on Windows. It's used to generate `ipc-server.hpp`, `ipc-client.hpp`, `browser-ipc-client.hpp`, etc. from `.fig` protocol files. The generated files live in `build-qt-check/generated/`. This step should run automatically during build.

If it doesn't, run manually:
```
cmake --build build-qt-check --target figura
cmake --build build-qt-check --target vicinae-server_autogen_timestamp_deps
```

---

## Tests

The project does NOT have a conventional test suite. There is no `tests/` directory at the top level. Individual libraries have test options:

| Library | Tests |
|---|---|
| `src/lib/vicinae-ipc/` | Has `BUILD_TESTS` option (requires Catch2) — not enabled |
| `src/lib/figura/` | Not checked for tests |
| `vendor/sqlcipher/` | External, not our concern |

No test steps exist for the server itself. Verification is done by building and running.

**Verification checklist:**
1. Build succeeds: `cmake --build build-qt-check --target vicinae-server` and `cmake --build build-qt-check --target vicinae`
2. Binaries exist: `Test-Path build-qt-check/bin/vicinae-server.exe` and `Test-Path build-qt-check/bin/vicinae.exe`
3. Can launch (will fail at runtime without proper config, but shouldn't crash):
   ```
   cd build-qt-check/bin
   ./vicinae.exe --help
   ./vicinae-server.exe --help
   ```

---

## Git History / Changed Files

~90 files modified. View with: `git diff --stat HEAD`

Major categories:
- `src/server/src/**/*.cpp` and `*.hpp` — main server fixes
- `src/lib/figura/src/` — lexer enum fix
- `src/lib/script-command/src/` — path::c_str fix
- `src/lib/linux-utils/src/` — path::c_str fix
- `src/cli/src/` — PORTED (QLocalSocket, Qt6::Network)
- `src/browser-extension/src/` — PORTED (QLocalSocket, Qt6::Network)
- `CMakeLists.txt` (root) — xdgpp guard
- `src/server/CMakeLists.txt` — cmark-gfm link order

All changes are uncommitted. Last commit: `3128e73a feat(settings): add GUI toggle for compact mode (#1469)`
