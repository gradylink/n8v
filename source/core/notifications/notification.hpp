#pragma once

#include <functional>
#include <string>

namespace n8v::detail {

enum class NotificationUrgency { Low, Normal, Critical };

struct NotificationRequest {
  std::string title;
  std::string body;
  std::string icon;
  std::string id;
  NotificationUrgency urgency = NotificationUrgency::Normal;
  bool silent = false;
};

using NotificationDone = std::function<void(bool clicked)>;

void pumpNotifications();

bool platformNotificationsAvailable();
void setPlatformNotificationApp(const std::string &name, const std::string &desktopEntry);
bool platformNotify(const NotificationRequest &request, NotificationDone done);
void platformCloseNotification(const std::string &id);
void pollPlatformNotifications();

} // namespace n8v::detail
