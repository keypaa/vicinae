#pragma once
#include "services/window-manager/abstract-window-manager.hpp"
#include <QThread>
#include <atomic>
#include <windows.h>

class Win32WindowManager : public AbstractWindowManager {
  Q_OBJECT

public:
  Win32WindowManager();
  ~Win32WindowManager() override;

  QString id() const override { return "win32"; }
  QString displayName() const override { return "Windows"; }

  WindowList listWindowsSync() const override;
  std::shared_ptr<AbstractWindow> getFocusedWindowSync() const override;
  bool supportsFocusTracking() const override { return true; }
  void focusWindowSync(const AbstractWindow &window) const override;
  bool closeWindow(const AbstractWindow &window) const override;

  bool ping() const override { return true; }
  bool isActivatable() const override { return true; }
  void start() override;

private:
  void onFocusChanged();

  static LRESULT CALLBACK helperWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
  static void CALLBACK winEventProc(HWINEVENTHOOK hook, DWORD event, HWND hwnd, LONG idObject,
                                    LONG idChild, DWORD dwEventThread, DWORD dwmsEventTime);

  static constexpr wchar_t HELPER_CLASS_NAME[] = L"VicinaeWin32WMHelper";
  static constexpr UINT WM_VC_SETUP_HOOK = WM_APP;
  static constexpr UINT WM_VC_FOCUS_CHANGED = WM_APP + 1;

  HWND m_helperWindow = nullptr;
  QThread *m_hookThread = nullptr;
  HANDLE m_stopEvent = nullptr;
  std::atomic<HWINEVENTHOOK> m_eventHook{nullptr};
};
