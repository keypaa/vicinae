#pragma once
#include "../abstract-desktop-notification-client.hpp"
#include <QSystemTrayIcon>

class Win32NotificationClient : public AbstractDesktopNotificationClient {
public:
  Win32NotificationClient();
  bool send(const Notification &notification) override;

private:
  QSystemTrayIcon *m_trayIcon;
};
