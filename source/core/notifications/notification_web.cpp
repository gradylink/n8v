#include "notification.hpp"

#include <emscripten/val.h>

#include <map>
#include <string>

namespace n8v::detail {
namespace {

using emscripten::val;

struct Shown {
  NotificationDone done;
};

std::map<std::string, Shown> g_shown;
std::string g_appName;
unsigned g_counter = 0;

bool supported() { return !val::global("Notification").isUndefined(); }

} // namespace

bool platformNotificationsAvailable() { return supported(); }

void setPlatformNotificationApp(const std::string &name, const std::string &) { g_appName = name; }

bool platformNotify(const NotificationRequest &request, NotificationDone done) {
  if (!supported()) return false;
  const val Notification = val::global("Notification");
  const std::string permission = Notification["permission"].as<std::string>();
  if (permission == "default") {
    Notification.call<val>("requestPermission");
    return false;
  }
  if (permission != "granted") return false;

  const std::string tag = request.id.empty() ? "n8v-" + std::to_string(++g_counter) : request.id;
  val options = val::object();
  options.set("body", request.body);
  options.set("tag", tag);
  options.set("silent", request.silent);
  options.set("requireInteraction", request.urgency == NotificationUrgency::Critical);
  val shown = Notification.new_(request.title, options);
  val events = val::global("window");
  if (events["__n8vNotes"].isUndefined()) events.set("__n8vNotes", val::array());
  val queue = events["__n8vNotes"];
  shown.set(
    "onclick",
    val::global("Function")
      .new_(std::string("queue"), std::string("tag"), std::string("return function() { queue.push([tag, true]); };"))
      .call<val>("call", val::null(), queue, tag)
  );
  shown.set(
    "onclose",
    val::global("Function")
      .new_(std::string("queue"), std::string("tag"), std::string("return function() { queue.push([tag, false]); };"))
      .call<val>("call", val::null(), queue, tag)
  );

  if (const auto old = g_shown.find(tag); old != g_shown.end()) {
    NotificationDone replaced = std::move(old->second.done);
    g_shown.erase(old);
    if (replaced) replaced(false);
  }
  g_shown[tag] = Shown{std::move(done)};
  return true;
}

void platformCloseNotification(const std::string &id) {
  const auto found = g_shown.find(id);
  if (found == g_shown.end()) return;
  NotificationDone done = std::move(found->second.done);
  g_shown.erase(found);
  if (done) done(false);
}

void pollPlatformNotifications() {
  if (g_shown.empty()) return;
  val queue = val::global("window")["__n8vNotes"];
  if (queue.isUndefined()) return;
  while (queue["length"].as<int>() > 0) {
    val entry = queue.call<val>("shift");
    const std::string tag = entry[0].as<std::string>();
    const bool clicked = entry[1].as<bool>();
    const auto found = g_shown.find(tag);
    if (found == g_shown.end()) continue;
    NotificationDone done = std::move(found->second.done);
    g_shown.erase(found);
    if (done) done(clicked);
  }
}

} // namespace n8v::detail
