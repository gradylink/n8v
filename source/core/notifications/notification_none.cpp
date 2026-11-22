#include "notification.hpp"

namespace n8v::detail {

bool platformNotificationsAvailable() { return false; }
void setPlatformNotificationApp(const std::string &, const std::string &) {}
bool platformNotify(const NotificationRequest &, NotificationDone) { return false; }
void platformCloseNotification(const std::string &) {}
void pollPlatformNotifications() {}

} // namespace n8v::detail
