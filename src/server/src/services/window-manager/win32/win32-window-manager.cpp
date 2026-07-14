#include "win32-window-manager.hpp"
#include "win32-window.hpp"
#include <QThread>
#include <dwmapi.h>
#include <string>
#include <vector>

namespace {

bool isWindowEligible(HWND hwnd) {
  if (!IsWindowVisible(hwnd)) return false;

  wchar_t buffer[1024];
  int len = GetWindowTextW(hwnd, buffer, sizeof(buffer) / sizeof(wchar_t));
  if (len == 0) return false;

  BOOL cloaked = FALSE;
  if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked)))) {
    if (cloaked) return false;
  }

  return true;
}

BOOL CALLBACK enumWindowsProc(HWND hwnd, LPARAM lParam) {
  auto *windows = reinterpret_cast<AbstractWindowManager::WindowList *>(lParam);
  if (isWindowEligible(hwnd)) {
    windows->emplace_back(std::make_shared<Win32Window>(hwnd));
  }
  return TRUE;
}

} // namespace

Win32WindowManager::Win32WindowManager() = default;

Win32WindowManager::~Win32WindowManager() {
  if (m_stopEvent) {
    SetEvent(m_stopEvent);
  }
  if (m_hookThread) {
    m_hookThread->wait();
    m_hookThread->deleteLater();
    m_hookThread = nullptr;
  }
  HWINEVENTHOOK hook = m_eventHook.exchange(nullptr);
  if (hook) {
    UnhookWinEvent(hook);
  }
  if (m_helperWindow) {
    DestroyWindow(m_helperWindow);
  }
  if (m_stopEvent) {
    CloseHandle(m_stopEvent);
  }
}

void Win32WindowManager::start() {
  WNDCLASSEXW wc{};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = helperWndProc;
  wc.lpszClassName = HELPER_CLASS_NAME;
  wc.hInstance = GetModuleHandleW(nullptr);
  RegisterClassExW(&wc);

  m_helperWindow = CreateWindowExW(0, HELPER_CLASS_NAME, L"VicinaeWMHelper", 0, 0, 0, 0, 0, nullptr,
                                   nullptr, wc.hInstance, nullptr);
  if (!m_helperWindow) return;

  SetWindowLongPtrW(m_helperWindow, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

  m_stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  if (!m_stopEvent) return;

  m_hookThread = QThread::create([this]() {
    HWINEVENTHOOK hook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr,
                                         winEventProc, 0, 0, WINEVENT_OUTOFCONTEXT);

    if (hook && m_helperWindow) {
      PostMessage(m_helperWindow, WM_VC_SETUP_HOOK, reinterpret_cast<WPARAM>(hook), 0);
    }

    HANDLE handles[] = {m_stopEvent};
    bool running = true;
    while (running) {
      DWORD result = MsgWaitForMultipleObjects(1, handles, FALSE, INFINITE, QS_ALLINPUT);
      if (result == WAIT_OBJECT_0) {
        running = false;
      } else {
        MSG msg{};
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
          TranslateMessage(&msg);
          DispatchMessage(&msg);
        }
      }
    }

    if (hook) UnhookWinEvent(hook);
  });
  m_hookThread->start();
}

void Win32WindowManager::onFocusChanged() {
  emit focusChanged();
}

LRESULT CALLBACK Win32WindowManager::helperWndProc(HWND hwnd, UINT msg, WPARAM wParam,
                                                   LPARAM lParam) {
  if (msg == WM_VC_SETUP_HOOK) {
    auto *self = reinterpret_cast<Win32WindowManager *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (self) {
      self->m_eventHook.store(reinterpret_cast<HWINEVENTHOOK>(wParam));
    }
    return 0;
  }

  if (msg == WM_VC_FOCUS_CHANGED) {
    auto *self = reinterpret_cast<Win32WindowManager *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (self) self->onFocusChanged();
    return 0;
  }

  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void CALLBACK Win32WindowManager::winEventProc(HWINEVENTHOOK /*hook*/, DWORD /*event*/,
                                               HWND hwnd, LONG /*idObject*/, LONG /*idChild*/,
                                               DWORD /*dwEventThread*/, DWORD /*dwmsEventTime*/) {
  if (!hwnd) return;

  HWND helper = FindWindowW(HELPER_CLASS_NAME, nullptr);
  if (helper) {
    PostMessage(helper, WM_VC_FOCUS_CHANGED, reinterpret_cast<WPARAM>(hwnd), 0);
  }
}

AbstractWindowManager::WindowList Win32WindowManager::listWindowsSync() const {
  WindowList windows;
  EnumWindows(enumWindowsProc, reinterpret_cast<LPARAM>(&windows));
  return windows;
}

std::shared_ptr<AbstractWindowManager::AbstractWindow> Win32WindowManager::getFocusedWindowSync() const {
  HWND hwnd = GetForegroundWindow();
  if (!hwnd) return nullptr;
  return std::make_shared<Win32Window>(hwnd);
}

void Win32WindowManager::focusWindowSync(const AbstractWindow &window) const {
  const auto &win32Window = static_cast<const Win32Window &>(window);
  HWND hwnd = win32Window.handle();
  SetForegroundWindow(hwnd);
  BringWindowToTop(hwnd);
}

bool Win32WindowManager::closeWindow(const AbstractWindow &window) const {
  const auto &win32Window = static_cast<const Win32Window &>(window);
  HWND hwnd = win32Window.handle();
  SendMessage(hwnd, WM_CLOSE, 0, 0);
  return true;
}
