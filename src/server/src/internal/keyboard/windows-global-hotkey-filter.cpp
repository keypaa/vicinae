#include <QtCore/qt_windows.h>

// Must include Qt headers before our header so Q_OS_WIN is defined
// when the class definition is parsed.
#include "windows-global-hotkey-filter.hpp"
#include "navigation-controller.hpp"
#include <QDebug>

WindowsGlobalHotkeyFilter::WindowsGlobalHotkeyFilter(NavigationController *nav, QObject *parent)
    : QObject(parent), m_nav(nav) {
  // Generate a unique hotkey ID via GlobalAddAtom to avoid conflicts
  m_hotkeyAtom = GlobalAddAtom(L"VicinaeToggleHotkey");
  m_hotkeyId = static_cast<UINT>(m_hotkeyAtom);
}

WindowsGlobalHotkeyFilter::~WindowsGlobalHotkeyFilter() {
  unregisterHotKey();
  if (m_hotkeyAtom) {
    GlobalDeleteAtom(m_hotkeyAtom);
    m_hotkeyAtom = 0;
  }
}

bool WindowsGlobalHotkeyFilter::nativeEventFilter(const QByteArray &eventType, void *message,
                                                  qintptr *result) {
  Q_UNUSED(eventType);
  Q_UNUSED(result);

  auto *msg = static_cast<MSG *>(message);
  if (msg->message == WM_HOTKEY && msg->wParam == m_hotkeyId) {
    if (m_nav) {
      m_nav->toggleWindow();
    }
    return true;
  }
  return false;
}

void WindowsGlobalHotkeyFilter::updateShortcut(const Keyboard::Shortcut &shortcut) {
  unregisterHotKey();

  if (!shortcut.isValid()) {
    m_currentShortcut = Keyboard::Shortcut();
    return;
  }

  if (registerHotKey(shortcut)) {
    m_currentShortcut = shortcut;
  } else {
    qWarning() << "Failed to register global hotkey" << shortcut.toDisplayString()
               << "— the key combination may be in use by another application";
    m_currentShortcut = Keyboard::Shortcut();
  }
}

bool WindowsGlobalHotkeyFilter::registerHotKey(const Keyboard::Shortcut &shortcut) {
  WORD vk = qtKeyToWin32Vk(shortcut.key());
  DWORD mods = qtModsToWin32Mods(shortcut.mods());

  if (!RegisterHotKey(nullptr, m_hotkeyId, mods, vk)) {
    return false;
  }

  m_registered = true;
  return true;
}

void WindowsGlobalHotkeyFilter::unregisterHotKey() {
  if (m_registered) {
    UnregisterHotKey(nullptr, m_hotkeyId);
    m_registered = false;
  }
}

WORD WindowsGlobalHotkeyFilter::qtKeyToWin32Vk(Qt::Key key) {
  if (key >= Qt::Key_A && key <= Qt::Key_Z) {
    return static_cast<WORD>(key);
  }
  if (key >= Qt::Key_0 && key <= Qt::Key_9) {
    return static_cast<WORD>(key);
  }
  if (key == Qt::Key_Space) {
    return VK_SPACE;
  }
  if (key >= Qt::Key_F1 && key <= Qt::Key_F12) {
    return static_cast<WORD>(VK_F1 + (static_cast<int>(key) - static_cast<int>(Qt::Key_F1)));
  }
  if (key == Qt::Key_Return || key == Qt::Key_Enter) {
    return VK_RETURN;
  }
  if (key == Qt::Key_Escape) {
    return VK_ESCAPE;
  }
  if (key == Qt::Key_Tab) {
    return VK_TAB;
  }
  if (key == Qt::Key_Backspace) {
    return VK_BACK;
  }
  if (key == Qt::Key_Delete) {
    return VK_DELETE;
  }
  if (key == Qt::Key_Home) {
    return VK_HOME;
  }
  if (key == Qt::Key_End) {
    return VK_END;
  }
  if (key == Qt::Key_PageUp) {
    return VK_PRIOR;
  }
  if (key == Qt::Key_PageDown) {
    return VK_NEXT;
  }
  if (key >= Qt::Key_Left && key <= Qt::Key_Down) {
    return static_cast<WORD>(VK_LEFT + (static_cast<int>(key) - static_cast<int>(Qt::Key_Left)));
  }

  // Fallback: try to map via MapVirtualKey, otherwise use VK_F24 as sentinel
  WORD vk = static_cast<WORD>(MapVirtualKeyW(static_cast<UINT>(key), MAPVK_VSC_TO_VK));
  return vk != 0 ? vk : VK_F24;
}

DWORD WindowsGlobalHotkeyFilter::qtModsToWin32Mods(Qt::KeyboardModifiers mods) {
  DWORD result = MOD_NOREPEAT; // 0x4000 — prevents repeated toggles on key-hold

  if (mods & Qt::ControlModifier) result |= MOD_CONTROL;
  if (mods & Qt::AltModifier) result |= MOD_ALT;
  if (mods & Qt::ShiftModifier) result |= MOD_SHIFT;
  if (mods & Qt::MetaModifier) result |= MOD_WIN;

  return result;
}
