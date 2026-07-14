#pragma once
#include "services/paste/abstract-paste-service.hpp"
#include <windows.h>

class Win32PasteService : public AbstractPasteService {
public:
  bool supportsPaste() const override { return true; }
  bool pasteToApp(const AbstractWindowManager::AbstractWindow *window,
                  const AbstractApplication *app) override;
};
