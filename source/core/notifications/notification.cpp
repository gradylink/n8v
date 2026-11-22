#include <n8v/n8v_c.h>

#include "core/notifications/notification.hpp"

#include <string>

namespace {

n8v::detail::NotificationUrgency urgencyOf(n8v_notification_urgency u) {
  switch (u) {
  case N8V_NOTIFICATION_URGENCY_LOW: return n8v::detail::NotificationUrgency::Low;
  case N8V_NOTIFICATION_URGENCY_CRITICAL: return n8v::detail::NotificationUrgency::Critical;
  default: return n8v::detail::NotificationUrgency::Normal;
  }
}

std::string str(const char *s) { return s ? s : ""; }

} // namespace

namespace n8v::detail {

void pumpNotifications() { pollPlatformNotifications(); }

} // namespace n8v::detail

extern "C" {

bool n8v_notifications_available(void) { return n8v::detail::platformNotificationsAvailable(); }

void n8v_set_notification_app(const char *name, const char *desktop_entry) { n8v::detail::setPlatformNotificationApp(str(name), str(desktop_entry)); }

bool n8v_notify(n8v_notification_options options) {
  n8v::detail::NotificationRequest request;
  request.title = str(options.title);
  request.body = str(options.body);
  request.icon = str(options.icon);
  request.id = str(options.id);
  request.urgency = urgencyOf(options.urgency);
  request.silent = options.silent;
  n8v::detail::NotificationDone done;
  if (options.on_done) {
    const n8v_notification_done_fn onDone = options.on_done;
    void *userdata = options.on_done_userdata;
    done = [onDone, userdata](bool clicked) { onDone(clicked, userdata); };
  }
  return n8v::detail::platformNotify(request, std::move(done));
}

void n8v_close_notification(const char *id) { n8v::detail::platformCloseNotification(str(id)); }

}
