#pragma once
#include "services/window-manager/abstract-window-manager.hpp"
#include <QString>
#include <optional>
#include <windows.h>

class Win32Window : public AbstractWindowManager::AbstractWindow {
public:
  explicit Win32Window(HWND hwnd);

  QString id() const override;
  QString title() const override;
  QString wmClass() const override;
  std::optional<int> pid() const override;
  std::optional<WindowBounds> bounds() const override;
  bool fullScreen() const override;
  bool canClose() const override { return true; }
  bool canFullScreen() const override { return true; }

  HWND handle() const { return m_hwnd; }

private:
  HWND m_hwnd;
};
