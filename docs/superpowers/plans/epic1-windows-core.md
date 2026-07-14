# EPIC 1 — Windows Core Infrastructure

## Global Constraints

- All new files go under `src/server/src/services/<service>/win32/`
- Follow existing patterns: `X11WindowManager`, `MacAppRuntime`, `X11ClipboardServer`
- C++23, MinGW-w64 13.2.0, Qt 6.8.3
- Register in factory functions (window-manager.cpp, clipboard-service.cpp, app-runtime.cpp)
- Add files to `src/server/CMakeLists.txt` under the `WIN32` block
- No raw `delete` on QObjects; use `deleteLater` where needed
- Use `Q_OS_WIN` for platform guards, never `_WIN32` in headers
- All source files pass through `clang-format` — use `make format` after implementation
- No comments unless absolutely necessary to explain something non-obvious
- Functions should be short and focused; one responsibility per file

## Task 1: WinWindowManager

Implement a Windows window manager backend in `src/server/src/services/window-manager/win32/`.

### Files to create:
- `src/server/src/services/window-manager/win32/win32-window-manager.hpp`
- `src/server/src/services/window-manager/win32/win32-window-manager.cpp`
- `src/server/src/services/window-manager/win32/win32-window.hpp`
- `src/server/src/services/window-manager/win32/win32-window.cpp`

### AbstractWindow subclass (Win32Window):
- Stores `HWND` handle
- `id()` returns hex string of HWND address
- `title()` via `GetWindowTextW`
- `wmClass()` via `GetClassNameW`
- `pid()` via `GetWindowThreadProcessId`
- `bounds()` via `GetWindowRect`
- `fullScreen()` — check if window rect matches primary monitor rect
- `canClose()` = true
- `canFullScreen()` = true

### Window manager (Win32WindowManager):
- `id()` returns `"win32"`
- `displayName()` returns `"Windows"`
- `isActivatable()` returns `true` unconditionally (always on Windows)
- `ping()` returns `true`
- `start()` — set up `SetWinEventHook(EVENT_SYSTEM_FOREGROUND, ...)` on a background thread via `QThread` to listen for focus changes; emit `focusChanged` when foreground window changes
- `listWindowsSync()` — use `EnumWindows` to enumerate top-level windows
  - Filter out windows that are not visible (`IsWindowVisible`), have no title, or are cloaked (`DWMWA_CLOAKED`)
- `getFocusedWindowSync()` — `GetForegroundWindow`, create `Win32Window`, return it
- `supportsFocusTracking()` returns `true`
- `focusWindowSync()` — `SetForegroundWindow`, `BringWindowToTop`
- `closeWindow()` — `SendMessage(hwnd, WM_CLOSE, 0, 0)` returns true
- `listScreensSync()` — inherited default is fine (uses QScreen)

### Win32 event listener:
- We don't need a separate file. Use `QThread` + `SetWinEventHook` inside the window manager itself.
- On focus change, emit `focusChanged`.
- Use `QWinEvent` integration if possible, otherwise a simple polling `QTimer` as fallback.

### Registration:
- In `window-manager.cpp` `createCandidates()`: add `#ifdef Q_OS_WIN` with `#include "win32/win32-window-manager.hpp"` and `candidates.emplace_back(std::make_unique<Win32WindowManager>())`

### CMakeLists.txt:
- Add under `WIN32` block:
  ```
  src/services/window-manager/win32/win32-window.hpp
  src/services/window-manager/win32/win32-window.cpp
  src/services/window-manager/win32/win32-window-manager.hpp
  src/services/window-manager/win32/win32-window-manager.cpp
  ```

## Task 2: WinClipboardServer

Implement a Windows clipboard server using Qt's clipboard monitoring (same pattern as `X11ClipboardServer`).

### Files to create:
- `src/server/src/services/clipboard/win32/win32-clipboard-server.hpp`
- `src/server/src/services/clipboard/win32/win32-clipboard-server.cpp`

### Implementation:
- Inherit from `AbstractQtClipboardServer` (same as `X11ClipboardServer`)
- `id()` returns `"win32-clipboard"`
- `isActivatable()` returns `QGuiApplication::platformName() == "windows"`
- `isAlive()` returns `true`

### Registration:
- In `clipboard-service.cpp` constructor: add `#ifdef Q_OS_WIN` with `#include "services/clipboard/win32/win32-clipboard-server.hpp"` and `factory.registerServer<Win32ClipboardServer>()`

### CMakeLists.txt:
- Add under `WIN32` block:
  ```
  src/services/clipboard/win32/win32-clipboard-server.hpp
  src/services/clipboard/win32/win32-clipboard-server.cpp
  ```
- Also add `src/services/clipboard/win32/` directory

## Task 3: WinAppRuntime

Implement a Windows app runtime backend in `src/server/src/services/app-runtime/win32/`.

### Files to create:
- `src/server/src/services/app-runtime/win32/win32-app-runtime.hpp`
- `src/server/src/services/app-runtime/win32/win32-app-runtime.cpp`

### Implementation:
- Inherit from `AbstractAppRuntime`
- `isRunning()`:
  - Use `CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)` to enumerate processes
  - Match executable name from `win-app.hpp`'s `executable()` property
  - Cache results, refresh every 5 seconds via `QTimer`
- `frontmostApp()`:
  - `GetForegroundWindow()` → `GetWindowThreadProcessId()` → get PID
  - Walk process list to find matching app
  - Returns `nullptr` if no match
- `activate()`:
  - Call `FindWindowW` with class or use PID to find window
  - `SetForegroundWindow(hwnd)`
  - Returns false if window can't be found
- `quit()`:
  - Find window for app → `SendMessage(hwnd, WM_CLOSE, 0, 0)` returns true
  - Fallback: `TerminateProcess` if `SendMessage` doesn't close in 2 seconds
- `forceQuit()`:
  - `OpenProcess(PROCESS_TERMINATE, FALSE, pid)` → `TerminateProcess`
- Polling via `QTimer` (every 5 seconds) to refresh `isRunning` cache
- `runningAppsChanged` emitted when cache changes
- `frontmostAppChanged` — emit when `EVENT_SYSTEM_FOREGROUND` fires, via `SetWinEventHook` (like the window manager does)

### Registration:
- In `app-runtime.cpp` `createProvider()`: change the `Q_OS_WIN` branch to `return std::make_unique<Win32AppRuntime>(appService)`

### CMakeLists.txt:
- Add under `WIN32` block:
  ```
  src/services/app-runtime/win32/win32-app-runtime.hpp
  src/services/app-runtime/win32/win32-app-runtime.cpp
  ```
