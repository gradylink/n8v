#include "notification.hpp"

#include <dbus/dbus.h>
#include <dbus_lazy_vars.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
#include <string>

namespace n8v::detail {
namespace {

constexpr const char *kService = "org.freedesktop.Notifications";
constexpr const char *kPath = "/org/freedesktop/Notifications";
constexpr const char *kInterface = "org.freedesktop.Notifications";

struct Shown {
  std::string id;
  NotificationDone done;
};

DBusConnection *g_conn = nullptr;
bool g_connectFailed = false;
std::map<dbus_uint32_t, Shown> g_shown;
std::map<std::string, dbus_uint32_t> g_byId;
std::string g_appName = "n8v app";
std::string g_desktopEntry;

void finish(dbus_uint32_t serviceId, bool clicked) {
  const auto found = g_shown.find(serviceId);
  if (found == g_shown.end()) return;
  Shown shown = std::move(found->second);
  g_shown.erase(found);
  if (!shown.id.empty()) {
    const auto byId = g_byId.find(shown.id);
    if (byId != g_byId.end() && byId->second == serviceId) g_byId.erase(byId);
  }
  if (shown.done) shown.done(clicked);
}

DBusHandlerResult onSignal(DBusConnection *, DBusMessage *msg, void *) {
  if (dbus_message_is_signal(msg, kInterface, "ActionInvoked")) {
    dbus_uint32_t id = 0;
    const char *key = nullptr;
    if (dbus_message_get_args(msg, nullptr, DBUS_TYPE_UINT32, &id, DBUS_TYPE_STRING, &key, DBUS_TYPE_INVALID) && key && std::strcmp(key, "default") == 0) finish(id, true);
  } else if (dbus_message_is_signal(msg, kInterface, "NotificationClosed")) {
    dbus_uint32_t id = 0, reason = 0;
    if (dbus_message_get_args(msg, nullptr, DBUS_TYPE_UINT32, &id, DBUS_TYPE_UINT32, &reason, DBUS_TYPE_INVALID)) finish(id, false);
  }
  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

bool connect() {
  if (g_conn) return true;
  if (g_connectFailed) return false;
  if (!lzy_dbus_lazy_is_available()) {
    std::fprintf(stderr, "[n8v] notifications: libdbus-1 isn't available\n");
    g_connectFailed = true;
    return false;
  }

  dbus_threads_init_default();
  DBusError err;
  dbus_error_init(&err);
  DBusConnection *conn = dbus_bus_get_private(DBUS_BUS_SESSION, &err);
  if (!conn) {
    std::fprintf(stderr, "[n8v] notifications: no session bus (%s)\n", dbus_error_is_set(&err) ? err.message : "no reason given");
    dbus_error_free(&err);
    g_connectFailed = true;
    return false;
  }
  dbus_connection_set_exit_on_disconnect(conn, FALSE);
  dbus_bus_add_match(conn, "type='signal',interface='org.freedesktop.Notifications'", &err);
  if (dbus_error_is_set(&err) || !dbus_connection_add_filter(conn, onSignal, nullptr, nullptr)) {
    dbus_error_free(&err);
    dbus_connection_close(conn);
    dbus_connection_unref(conn);
    g_connectFailed = true;
    return false;
  }
  g_conn = conn;
  return true;
}

void appendString(DBusMessageIter *it, const std::string &value) {
  const char *c = value.c_str();
  dbus_message_iter_append_basic(it, DBUS_TYPE_STRING, &c);
}

void addHint(DBusMessageIter *dict, const char *key, int type, const char *signature, const void *value) {
  DBusMessageIter entry, variant;
  dbus_message_iter_open_container(dict, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
  dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);
  dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, signature, &variant);
  dbus_message_iter_append_basic(&variant, type, value);
  dbus_message_iter_close_container(&entry, &variant);
  dbus_message_iter_close_container(dict, &entry);
}

} // namespace

bool platformNotificationsAvailable() {
  if (!connect()) return false;
  static bool answer = false;
  static std::chrono::steady_clock::time_point asked{};
  const auto now = std::chrono::steady_clock::now();
  if (asked != std::chrono::steady_clock::time_point{} && now - asked < std::chrono::seconds(5)) return answer;
  asked = now;
  const bool owned = dbus_bus_name_has_owner(g_conn, kService, nullptr);
  static bool told = false;
  if (!owned && !told) {
    told = true;
    std::fprintf(stderr, "[n8v] notifications: no notification service on the session bus\n");
  }
  answer = owned;
  return answer;
}

void setPlatformNotificationApp(const std::string &name, const std::string &desktopEntry) {
  if (!name.empty()) g_appName = name;
  g_desktopEntry = desktopEntry;
}

bool platformNotify(const NotificationRequest &request, NotificationDone done) {
  if (!platformNotificationsAvailable()) return false;

  dbus_uint32_t replaces = 0;
  if (!request.id.empty()) {
    const auto previous = g_byId.find(request.id);
    if (previous != g_byId.end()) replaces = previous->second;
  }

  DBusMessage *msg = dbus_message_new_method_call(kService, kPath, kInterface, "Notify");
  if (!msg) return false;
  DBusMessageIter it, actions, hints;
  dbus_message_iter_init_append(msg, &it);
  appendString(&it, g_appName);
  dbus_message_iter_append_basic(&it, DBUS_TYPE_UINT32, &replaces);
  appendString(&it, std::string());
  appendString(&it, request.title);
  appendString(&it, request.body);
  dbus_message_iter_open_container(&it, DBUS_TYPE_ARRAY, "s", &actions);
  appendString(&actions, "default");
  appendString(&actions, "Open");
  dbus_message_iter_close_container(&it, &actions);

  dbus_message_iter_open_container(&it, DBUS_TYPE_ARRAY, "{sv}", &hints);
  const unsigned char urgency = request.urgency == NotificationUrgency::Low ? 0 : request.urgency == NotificationUrgency::Critical ? 2 : 1;
  addHint(&hints, "urgency", DBUS_TYPE_BYTE, "y", &urgency);
  const dbus_bool_t silent = request.silent ? TRUE : FALSE;
  if (request.silent) addHint(&hints, "suppress-sound", DBUS_TYPE_BOOLEAN, "b", &silent);
  const char *category = "im.received";
  addHint(&hints, "category", DBUS_TYPE_STRING, "s", &category);
  if (!g_desktopEntry.empty()) {
    const char *entry = g_desktopEntry.c_str();
    addHint(&hints, "desktop-entry", DBUS_TYPE_STRING, "s", &entry);
  }
  std::error_code ignored;
  if (!request.icon.empty() && std::filesystem::exists(request.icon, ignored)) {
    const char *icon = request.icon.c_str();
    addHint(&hints, "image-path", DBUS_TYPE_STRING, "s", &icon);
  }
  dbus_message_iter_close_container(&it, &hints);
  const dbus_int32_t timeout = -1;
  dbus_message_iter_append_basic(&it, DBUS_TYPE_INT32, &timeout);

  DBusError err;
  dbus_error_init(&err);
  DBusMessage *reply = dbus_connection_send_with_reply_and_block(g_conn, msg, 1000, &err);
  dbus_message_unref(msg);
  if (!reply) {
    std::fprintf(stderr, "[n8v] notifications: the service didn't take it (%s)\n", dbus_error_is_set(&err) ? err.message : "no answer");
    dbus_error_free(&err);
    return false;
  }
  dbus_uint32_t serviceId = 0;
  const bool ok = dbus_message_get_args(reply, nullptr, DBUS_TYPE_UINT32, &serviceId, DBUS_TYPE_INVALID);
  dbus_message_unref(reply);
  if (!ok) {
    std::fprintf(stderr, "[n8v] notifications: the service answered with something unexpected\n");
    return false;
  }

  if (replaces != 0) {
    const auto old = g_shown.find(replaces);
    if (old != g_shown.end() && replaces == serviceId) {
      NotificationDone replacedDone = std::move(old->second.done);
      g_shown.erase(old);
      if (replacedDone) replacedDone(false);
    }
  }
  g_shown[serviceId] = Shown{request.id, std::move(done)};
  if (!request.id.empty()) g_byId[request.id] = serviceId;
  return true;
}

void platformCloseNotification(const std::string &id) {
  if (!g_conn || id.empty()) return;
  const auto found = g_byId.find(id);
  if (found == g_byId.end()) return;
  const dbus_uint32_t serviceId = found->second;
  DBusMessage *msg = dbus_message_new_method_call(kService, kPath, kInterface, "CloseNotification");
  if (msg) {
    dbus_message_append_args(msg, DBUS_TYPE_UINT32, &serviceId, DBUS_TYPE_INVALID);
    dbus_connection_send(g_conn, msg, nullptr);
    dbus_connection_flush(g_conn);
    dbus_message_unref(msg);
  }
  finish(serviceId, false);
}

void pollPlatformNotifications() {
  if (!g_conn) return;
  if (!dbus_connection_read_write(g_conn, 0)) return;
  while (dbus_connection_dispatch(g_conn) == DBUS_DISPATCH_DATA_REMAINS) {
  }
}

} // namespace n8v::detail
