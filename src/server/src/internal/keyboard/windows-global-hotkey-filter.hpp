#pragma once
#include <QAbstractNativeEventFilter>
#include <QObject>
#include <QPointer>
#include "keyboard.hpp"

class NavigationController;

#ifdef Q_OS_WIN
class WindowsGlobalHotkeyFilter : public QObject, public QAbstractNativeEventFilter {
  Q_OBJECT

public:
  explicit WindowsGlobalHotkeyFilter(NavigationController *nav, QObject *parent = nullptr);
  ~WindowsGlobalHotkeyFilter() override;

  void updateShortcut(const Keyboard::Shortcut &shortcut);

  bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;

private:
  bool registerHotKey(const Keyboard::Shortcut &shortcut);
  void unregisterHotKey();

  // Portable aliases for Win32 types (avoid <windows.h> in header)
  static unsigned short  qtKeyToWin32Vk(Qt::Key key);
  static unsigned long   qtModsToWin32Mods(Qt::KeyboardModifiers mods);

  QPointer<NavigationController> m_nav;
  Keyboard::Shortcut             m_currentShortcut;
  unsigned short                 m_hotkeyAtom = 0;
  unsigned int                   m_hotkeyId = 0;
  bool                           m_registered = false;
};
#else
// Stub for non-Windows platforms — no-op implementation
class WindowsGlobalHotkeyFilter : public QObject {
  Q_OBJECT

public:
  explicit WindowsGlobalHotkeyFilter(NavigationController *, QObject *parent = nullptr) : QObject(parent) {}
  void updateShortcut(const Keyboard::Shortcut &) {}
};
#endif
