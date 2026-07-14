#include <windows.h>
#include <winbase.h>

#include "win32-power-manager.hpp"

bool Win32PowerManager::powerOff() {
  return ExitWindowsEx(EWX_POWEROFF | EWX_FORCEIFHUNG, SHTDN_REASON_MAJOR_OTHER);
}

bool Win32PowerManager::reboot() {
  return ExitWindowsEx(EWX_REBOOT | EWX_FORCEIFHUNG, SHTDN_REASON_MAJOR_OTHER);
}

bool Win32PowerManager::softReboot() { return false; }

bool Win32PowerManager::sleep() const { return SetSuspendState(FALSE, FALSE, FALSE); }

bool Win32PowerManager::suspend() { return SetSuspendState(FALSE, FALSE, FALSE); }

bool Win32PowerManager::hibernate() { return SetSuspendState(TRUE, FALSE, FALSE); }

bool Win32PowerManager::lock() { return LockWorkStation(); }

bool Win32PowerManager::logout() {
  return ExitWindowsEx(EWX_LOGOFF | EWX_FORCEIFHUNG, SHTDN_REASON_MAJOR_OTHER);
}

bool Win32PowerManager::canPowerOff() const { return true; }
bool Win32PowerManager::canReboot() const { return true; }
bool Win32PowerManager::canSoftReboot() const { return false; }
bool Win32PowerManager::canSuspend() const { return true; }
bool Win32PowerManager::canSleep() const { return true; }
bool Win32PowerManager::canHibernate() const { return true; }
bool Win32PowerManager::canLock() const { return true; }
bool Win32PowerManager::canLogOut() const { return true; }

QString Win32PowerManager::id() const { return "win32"; }
