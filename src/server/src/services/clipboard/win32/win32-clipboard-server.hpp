#pragma once
#include "../qt/qt-clipboard-server.hpp"
#include <QGuiApplication>
#include <qbuffer.h>
#include <qclipboard.h>
#include <QUrl>
#include <QMimeData>

class Win32ClipboardServer : public AbstractQtClipboardServer {
public:
  QString id() const override { return "win32-clipboard"; }
  bool isActivatable() const override { return QGuiApplication::platformName() == "windows"; }
};
