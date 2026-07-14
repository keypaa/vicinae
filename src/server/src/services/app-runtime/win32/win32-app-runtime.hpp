#pragma once
#include "services/app-runtime/abstract-app-runtime.hpp"
#include <QTimer>
#include <optional>
#include <unordered_set>
#include <windows.h>

class AppService;

class Win32AppRuntime : public AbstractAppRuntime {
  Q_OBJECT

public:
  explicit Win32AppRuntime(AppService &appService);
  ~Win32AppRuntime() override;

  bool isRunning(const AbstractApplication &app) const override;
  std::shared_ptr<AbstractApplication> frontmostApp() const override;
  bool activate(const AbstractApplication &app) const override;
  bool quit(const AbstractApplication &app) const override;
  bool forceQuit(const AbstractApplication &app) const override;

private:
  void refreshRunningCache();
  void onForegroundChanged();
  DWORD findPidForApp(const AbstractApplication &app) const;

  static void CALLBACK winEventProc(HWINEVENTHOOK hook, DWORD event, HWND hwnd, LONG idObject,
                                    LONG idChild, DWORD dwEventThread, DWORD dwmsEventTime);

  AppService &m_appService;
  QTimer *m_pollTimer;
  HWINEVENTHOOK m_eventHook = nullptr;
  std::unordered_set<QString> m_runningIds;
};
