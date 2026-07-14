#include "win32-app-runtime.hpp"
#include "services/app-service/app-service.hpp"
#include <QMetaObject>
#include <atomic>
#include <psapi.h>
#include <tlhelp32.h>

namespace {

std::atomic<Win32AppRuntime *> s_instance{nullptr};

struct FindWindowData {
  DWORD pid;
  HWND hwnd = nullptr;
};

BOOL CALLBACK enumFindWindow(HWND hwnd, LPARAM lParam) {
  auto *data = reinterpret_cast<FindWindowData *>(lParam);
  DWORD pid = 0;
  GetWindowThreadProcessId(hwnd, &pid);
  if (pid == data->pid && IsWindowVisible(hwnd)) {
    data->hwnd = hwnd;
    return FALSE;
  }
  return TRUE;
}

} // namespace

Win32AppRuntime::Win32AppRuntime(AppService &appService) : m_appService(appService) {
  m_pollTimer = new QTimer(this);
  m_pollTimer->setInterval(5000);
  connect(m_pollTimer, &QTimer::timeout, this, &Win32AppRuntime::refreshRunningCache);
  m_pollTimer->start();

  s_instance.store(this, std::memory_order_release);
  m_eventHook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr,
                                winEventProc, 0, 0, WINEVENT_OUTOFCONTEXT);

  refreshRunningCache();
}

Win32AppRuntime::~Win32AppRuntime() {
  s_instance.store(nullptr, std::memory_order_release);
  if (m_eventHook) {
    UnhookWinEvent(m_eventHook);
  }
}

void Win32AppRuntime::onForegroundChanged() { emit frontmostAppChanged(); }

void CALLBACK Win32AppRuntime::winEventProc(HWINEVENTHOOK /*hook*/, DWORD /*event*/, HWND /*hwnd*/,
                                            LONG /*idObject*/, LONG /*idChild*/,
                                            DWORD /*dwEventThread*/, DWORD /*dwmsEventTime*/) {
  if (auto *self = s_instance.load(std::memory_order_acquire)) {
    QMetaObject::invokeMethod(self, [self]() { self->onForegroundChanged(); },
                              Qt::QueuedConnection);
  }
}

void Win32AppRuntime::refreshRunningCache() {
  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snapshot == INVALID_HANDLE_VALUE) return;

  std::unordered_set<std::wstring> runningExes;
  {
    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(snapshot, &pe)) {
      do {
        runningExes.emplace(pe.szExeFile);
      } while (Process32NextW(snapshot, &pe));
    }
  }
  CloseHandle(snapshot);

  std::unordered_set<QString> ids;
  auto apps = m_appService.list();
  ids.reserve(apps.size());
  for (const auto &app : apps) {
    QString prog = app->program();
    if (prog.isEmpty()) continue;

    std::wstring progPath = prog.toStdWString();
    size_t sep = progPath.find_last_of(L"\\/");
    const wchar_t *progExe =
        (sep != std::wstring::npos) ? progPath.c_str() + sep + 1 : progPath.c_str();

    for (const auto &runningExe : runningExes) {
      if (_wcsicmp(progExe, runningExe.c_str()) == 0) {
        ids.emplace(app->id());
        break;
      }
    }
  }

  if (ids != m_runningIds) {
    m_runningIds = std::move(ids);
    emit runningAppsChanged();
  }
}

bool Win32AppRuntime::isRunning(const AbstractApplication &app) const {
  return m_runningIds.contains(app.id());
}

std::shared_ptr<AbstractApplication> Win32AppRuntime::frontmostApp() const {
  HWND hwnd = GetForegroundWindow();
  if (!hwnd) return nullptr;

  DWORD pid = 0;
  GetWindowThreadProcessId(hwnd, &pid);
  if (pid == 0) return nullptr;

  HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
  if (!process) return nullptr;

  wchar_t exePath[MAX_PATH];
  DWORD size = MAX_PATH;
  BOOL ok = QueryFullProcessImageNameW(process, 0, exePath, &size);
  CloseHandle(process);
  if (!ok) return nullptr;

  QString fullPath = QString::fromWCharArray(exePath);
  int sep = fullPath.lastIndexOf('\\');
  QString fileName = (sep >= 0) ? fullPath.mid(sep + 1) : fullPath;

  auto apps = m_appService.list();
  for (const auto &app : apps) {
    QString prog = app->program();
    if (prog.isEmpty()) continue;

    int progSep = prog.lastIndexOf('\\');
    QString progFile = (progSep >= 0) ? prog.mid(progSep + 1) : prog;

    if (fileName.compare(progFile, Qt::CaseInsensitive) == 0) return app;
  }

  return nullptr;
}

bool Win32AppRuntime::activate(const AbstractApplication &app) const {
  DWORD pid = findPidForApp(app);
  if (pid == 0) return m_appService.launch(app);

  FindWindowData data;
  data.pid = pid;
  EnumWindows(enumFindWindow, reinterpret_cast<LPARAM>(&data));

  if (data.hwnd) {
    SetForegroundWindow(data.hwnd);
    BringWindowToTop(data.hwnd);
    return true;
  }

  return m_appService.launch(app);
}

bool Win32AppRuntime::quit(const AbstractApplication &app) const {
  DWORD pid = findPidForApp(app);
  if (pid == 0) return false;

  FindWindowData data;
  data.pid = pid;
  EnumWindows(enumFindWindow, reinterpret_cast<LPARAM>(&data));

  if (data.hwnd) {
    PostMessage(data.hwnd, WM_CLOSE, 0, 0);
    return true;
  }

  return false;
}

bool Win32AppRuntime::forceQuit(const AbstractApplication &app) const {
  DWORD pid = findPidForApp(app);
  if (pid == 0) return false;

  HANDLE process = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
  if (!process) return false;

  BOOL ok = TerminateProcess(process, 1);
  CloseHandle(process);
  return ok == TRUE;
}

DWORD Win32AppRuntime::findPidForApp(const AbstractApplication &app) const {
  QString prog = app.program();
  if (prog.isEmpty()) return 0;

  std::wstring progPath = prog.toStdWString();
  size_t sep = progPath.find_last_of(L"\\/");
  const wchar_t *progExe =
      (sep != std::wstring::npos) ? progPath.c_str() + sep + 1 : progPath.c_str();

  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snapshot == INVALID_HANDLE_VALUE) return 0;

  DWORD pid = 0;
  PROCESSENTRY32W pe;
  pe.dwSize = sizeof(pe);
  if (Process32FirstW(snapshot, &pe)) {
    do {
      if (_wcsicmp(progExe, pe.szExeFile) == 0) {
        pid = pe.th32ProcessID;
        break;
      }
    } while (Process32NextW(snapshot, &pe));
  }
  CloseHandle(snapshot);
  return pid;
}
