#include "win32-window.hpp"
#include <QString>
#include <optional>

Win32Window::Win32Window(HWND hwnd) : m_hwnd(hwnd) {}

QString Win32Window::id() const {
  return QString::number(reinterpret_cast<quintptr>(m_hwnd), 16);
}

QString Win32Window::title() const {
  wchar_t buffer[1024];
  int len = GetWindowTextW(m_hwnd, buffer, sizeof(buffer) / sizeof(wchar_t));
  if (len > 0) {
    return QString::fromWCharArray(buffer, len);
  }
  return {};
}

QString Win32Window::wmClass() const {
  wchar_t buffer[256];
  int len = GetClassNameW(m_hwnd, buffer, sizeof(buffer) / sizeof(wchar_t));
  if (len > 0) {
    return QString::fromWCharArray(buffer, len);
  }
  return {};
}

std::optional<int> Win32Window::pid() const {
  DWORD processId = 0;
  GetWindowThreadProcessId(m_hwnd, &processId);
  if (processId != 0) {
    return static_cast<int>(processId);
  }
  return std::nullopt;
}

std::optional<AbstractWindowManager::WindowBounds> Win32Window::bounds() const {
  RECT rect{};
  if (!GetWindowRect(m_hwnd, &rect)) {
    return std::nullopt;
  }
  return WindowBounds{
      .x = static_cast<int32_t>(rect.left),
      .y = static_cast<int32_t>(rect.top),
      .width = static_cast<int32_t>(rect.right - rect.left),
      .height = static_cast<int32_t>(rect.bottom - rect.top),
  };
}

bool Win32Window::fullScreen() const {
  RECT windowRect{};
  if (!GetWindowRect(m_hwnd, &windowRect)) {
    return false;
  }

  HMONITOR monitor = MonitorFromWindow(m_hwnd, MONITOR_DEFAULTTONEAREST);
  if (!monitor) {
    return false;
  }

  MONITORINFOEXW mi{};
  mi.cbSize = sizeof(mi);
  if (!GetMonitorInfoW(monitor, &mi)) {
    return false;
  }

  return windowRect.left == mi.rcMonitor.left &&
         windowRect.top == mi.rcMonitor.top &&
         windowRect.right == mi.rcMonitor.right &&
         windowRect.bottom == mi.rcMonitor.bottom;
}
