#include "win32-notification-client.hpp"

namespace {

QSystemTrayIcon::MessageIcon mapUrgency(AbstractDesktopNotificationClient::Urgency urgency) {
  using Urgency = AbstractDesktopNotificationClient::Urgency;
  switch (urgency) {
  case Urgency::High:
    return QSystemTrayIcon::Warning;
  case Urgency::Low:
    return QSystemTrayIcon::Critical;
  default:
    return QSystemTrayIcon::Information;
  }
}

} // namespace

Win32NotificationClient::Win32NotificationClient() {
  m_trayIcon = new QSystemTrayIcon(this);
  m_trayIcon->setIcon(QIcon(":/icons/vicinae.svg"));
  m_trayIcon->show();
}

bool Win32NotificationClient::send(const Notification &n) {
  m_trayIcon->showMessage(n.title, n.body, mapUrgency(n.urgency));
  return true;
}
