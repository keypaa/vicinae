#include "win32-paste-service.hpp"
#include "services/window-manager/win32/win32-window.hpp"

bool Win32PasteService::pasteToApp(const AbstractWindowManager::AbstractWindow *window,
                                   const AbstractApplication *) {
  if (window) {
    auto *win32Window = dynamic_cast<const Win32Window *>(window);
    if (win32Window) {
      HWND hwnd = win32Window->handle();
      SetForegroundWindow(hwnd);
    }
  }

  INPUT inputs[4]{};

  inputs[0].type = INPUT_KEYBOARD;
  inputs[0].ki.wVk = VK_CONTROL;

  inputs[1].type = INPUT_KEYBOARD;
  inputs[1].ki.wVk = 'V';

  inputs[2].type = INPUT_KEYBOARD;
  inputs[2].ki.wVk = 'V';
  inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;

  inputs[3].type = INPUT_KEYBOARD;
  inputs[3].ki.wVk = VK_CONTROL;
  inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;

  SendInput(4, inputs, sizeof(INPUT));
  return true;
}
