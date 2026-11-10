#include "file_dialog.hpp"

#include <dbus/dbus.h>
#include <dbus_lazy_vars.h>

#include <unistd.h>

#include <cctype>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

namespace n8v::detail {
namespace {

constexpr const char *kService = "org.freedesktop.portal.Desktop";
constexpr const char *kObjectPath = "/org/freedesktop/portal/desktop";
constexpr const char *kChooserInterface = "org.freedesktop.portal.FileChooser";
constexpr const char *kRequestInterface = "org.freedesktop.portal.Request";

struct Active {
  FileDialogDone done;
  std::string requestPath;
  std::string matchRule;
  std::string extraMatchRule;
};

DBusConnection *g_conn = nullptr;
bool g_connectFailed = false;
Active *g_active = nullptr;
unsigned g_tokenCounter = 0;

std::string matchRuleFor(const std::string &path) { return "type='signal',interface='" + std::string(kRequestInterface) + "',member='Response',path='" + path + "'"; }

void finish(FileDialogResult::Status status, std::vector<std::string> paths = {}) {
  if (!g_active) return;
  Active *a = g_active;
  g_active = nullptr;
  for (const std::string *rule : {&a->matchRule, &a->extraMatchRule}) {
    if (!rule->empty() && g_conn) dbus_bus_remove_match(g_conn, rule->c_str(), nullptr);
  }
  FileDialogDone done = std::move(a->done);
  delete a;
  done({status, std::move(paths)});
}

void appendString(DBusMessageIter *it, const char *value) { dbus_message_iter_append_basic(it, DBUS_TYPE_STRING, &value); }

struct Entry {
  DBusMessageIter entry;
  DBusMessageIter variant;
  DBusMessageIter *dict;

  Entry(DBusMessageIter *dictIter, const char *key, const char *sig) : dict(dictIter) {
    dbus_message_iter_open_container(dict, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
    appendString(&entry, key);
    dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, sig, &variant);
  }
  ~Entry() {
    dbus_message_iter_close_container(&entry, &variant);
    dbus_message_iter_close_container(dict, &entry);
  }
};

void addStringEntry(DBusMessageIter *dict, const char *key, const std::string &value) {
  Entry e(dict, key, "s");
  appendString(&e.variant, value.c_str());
}

void addBoolEntry(DBusMessageIter *dict, const char *key, bool value) {
  Entry e(dict, key, "b");
  dbus_bool_t v = value;
  dbus_message_iter_append_basic(&e.variant, DBUS_TYPE_BOOLEAN, &v);
}

void addBytesEntry(DBusMessageIter *dict, const char *key, const std::string &value) {
  Entry e(dict, key, "ay");
  DBusMessageIter arr;
  dbus_message_iter_open_container(&e.variant, DBUS_TYPE_ARRAY, "y", &arr);
  const char *data = value.c_str();
  dbus_message_iter_append_fixed_array(&arr, DBUS_TYPE_BYTE, &data, static_cast<int>(value.size()) + 1);
  dbus_message_iter_close_container(&e.variant, &arr);
}

void addFilters(DBusMessageIter *dict, const std::vector<FileDialogFilter> &filters) {
  Entry e(dict, "filters", "a(sa(us))");
  DBusMessageIter list;
  dbus_message_iter_open_container(&e.variant, DBUS_TYPE_ARRAY, "(sa(us))", &list);
  for (const FileDialogFilter &filter : filters) {
    DBusMessageIter st, pats;
    dbus_message_iter_open_container(&list, DBUS_TYPE_STRUCT, nullptr, &st);
    appendString(&st, filter.name.c_str());
    dbus_message_iter_open_container(&st, DBUS_TYPE_ARRAY, "(us)", &pats);
    auto addGlob = [&](const std::string &glob) {
      DBusMessageIter ps;
      dbus_message_iter_open_container(&pats, DBUS_TYPE_STRUCT, nullptr, &ps);
      dbus_uint32_t kind = 0;
      dbus_message_iter_append_basic(&ps, DBUS_TYPE_UINT32, &kind);
      appendString(&ps, glob.c_str());
      dbus_message_iter_close_container(&pats, &ps);
    };
    for (const std::string &glob : filter.patterns) {
      std::string lower = glob, upper = glob;
      for (char &c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
      for (char &c : upper) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
      addGlob(lower);
      if (upper != lower) addGlob(upper);
      if (glob != lower && glob != upper) addGlob(glob);
    }
    dbus_message_iter_close_container(&st, &pats);
    dbus_message_iter_close_container(&list, &st);
  }
  dbus_message_iter_close_container(&e.variant, &list);
}

std::vector<std::string> parseUris(DBusMessageIter dictArray) {
  std::vector<std::string> paths;
  DBusMessageIter dict;
  dbus_message_iter_recurse(&dictArray, &dict);
  while (dbus_message_iter_get_arg_type(&dict) == DBUS_TYPE_DICT_ENTRY) {
    DBusMessageIter entry;
    dbus_message_iter_recurse(&dict, &entry);
    const char *key = nullptr;
    dbus_message_iter_get_basic(&entry, &key);
    dbus_message_iter_next(&entry);
    if (key && std::strcmp(key, "uris") == 0 && dbus_message_iter_get_arg_type(&entry) == DBUS_TYPE_VARIANT) {
      DBusMessageIter variant, uris;
      dbus_message_iter_recurse(&entry, &variant);
      if (dbus_message_iter_get_arg_type(&variant) == DBUS_TYPE_ARRAY) {
        dbus_message_iter_recurse(&variant, &uris);
        while (dbus_message_iter_get_arg_type(&uris) == DBUS_TYPE_STRING) {
          const char *uri = nullptr;
          dbus_message_iter_get_basic(&uris, &uri);
          if (uri) {
            std::string path = fileUriToPath(uri);
            if (!path.empty()) paths.push_back(std::move(path));
          }
          dbus_message_iter_next(&uris);
        }
      }
    }
    dbus_message_iter_next(&dict);
  }
  return paths;
}

DBusHandlerResult onSignal(DBusConnection *, DBusMessage *msg, void *) {
  if (!g_active || !dbus_message_is_signal(msg, kRequestInterface, "Response")) return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  const char *path = dbus_message_get_path(msg);
  if (!path || g_active->requestPath != path) return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

  DBusMessageIter it;
  dbus_uint32_t response = 2;
  std::vector<std::string> paths;
  if (dbus_message_iter_init(msg, &it) && dbus_message_iter_get_arg_type(&it) == DBUS_TYPE_UINT32) {
    dbus_message_iter_get_basic(&it, &response);
    dbus_message_iter_next(&it);
    if (response == 0 && dbus_message_iter_get_arg_type(&it) == DBUS_TYPE_ARRAY) paths = parseUris(it);
  }
  if (response == 0 && !paths.empty()) {
    finish(FileDialogResult::Status::Done, std::move(paths));
  } else {
    finish(FileDialogResult::Status::Cancelled);
  }
  return DBUS_HANDLER_RESULT_HANDLED;
}

void onReply(DBusPendingCall *pending, void *) {
  DBusMessage *reply = dbus_pending_call_steal_reply(pending);
  dbus_pending_call_unref(pending);
  if (!reply) return;

  if (dbus_message_get_type(reply) == DBUS_MESSAGE_TYPE_ERROR) {
    finish(FileDialogResult::Status::Unavailable);
  } else if (g_active) {
    const char *handle = nullptr;
    if (dbus_message_get_args(reply, nullptr, DBUS_TYPE_OBJECT_PATH, &handle, DBUS_TYPE_INVALID) && handle && g_active->requestPath != handle) {
      g_active->requestPath = handle;
      g_active->extraMatchRule = matchRuleFor(handle);
      dbus_bus_add_match(g_conn, g_active->extraMatchRule.c_str(), nullptr);
    }
  }
  dbus_message_unref(reply);
}

bool connect() {
  if (g_conn) return true;
  if (g_connectFailed || !lzy_dbus_lazy_is_available()) return false;

  dbus_threads_init_default();
  DBusError err;
  dbus_error_init(&err);
  DBusConnection *conn = dbus_bus_get_private(DBUS_BUS_SESSION, &err);
  if (!conn) {
    dbus_error_free(&err);
    g_connectFailed = true;
    return false;
  }
  dbus_connection_set_exit_on_disconnect(conn, FALSE);
  if (!dbus_connection_add_filter(conn, onSignal, nullptr, nullptr)) {
    dbus_connection_close(conn);
    dbus_connection_unref(conn);
    g_connectFailed = true;
    return false;
  }
  g_conn = conn;
  return true;
}

} // namespace

bool startPlatformFileDialog(const FileDialogRequest &request, FileDialogDone done) {
  if (g_active || !connect()) return false;

  const char *unique = dbus_bus_get_unique_name(g_conn);
  if (!unique) return false;
  std::string sender = unique[0] == ':' ? unique + 1 : unique;
  for (char &c : sender) {
    if (c == '.') c = '_';
  }
  const std::string token = "n8v_" + std::to_string(static_cast<long>(getpid())) + "_" + std::to_string(++g_tokenCounter);

  auto active = std::make_unique<Active>();
  active->done = std::move(done);
  active->requestPath = std::string(kObjectPath) + "/request/" + sender + "/" + token;
  active->matchRule = matchRuleFor(active->requestPath);

  const bool save = request.kind == N8V_FILE_DIALOG_SAVE_FILE;
  DBusMessage *msg = dbus_message_new_method_call(kService, kObjectPath, kChooserInterface, save ? "SaveFile" : "OpenFile");
  if (!msg) return false;

  DBusMessageIter it, dict;
  dbus_message_iter_init_append(msg, &it);
  appendString(&it, "");
  const std::string title = !request.title.empty() ? request.title : save ? "Save File" : request.kind == N8V_FILE_DIALOG_OPEN_FOLDER ? "Select Folder" : "Open File";
  appendString(&it, title.c_str());
  dbus_message_iter_open_container(&it, DBUS_TYPE_ARRAY, "{sv}", &dict);
  addStringEntry(&dict, "handle_token", token);
  addBoolEntry(&dict, "modal", true);
  if (save) {
    if (!request.defaultName.empty()) addStringEntry(&dict, "current_name", request.defaultName);
  } else {
    addBoolEntry(&dict, "multiple", request.multiple);
    addBoolEntry(&dict, "directory", request.kind == N8V_FILE_DIALOG_OPEN_FOLDER);
  }
  if (!request.initialDir.empty()) addBytesEntry(&dict, "current_folder", request.initialDir);
  if (!request.filters.empty() && request.kind != N8V_FILE_DIALOG_OPEN_FOLDER) addFilters(&dict, request.filters);
  dbus_message_iter_close_container(&it, &dict);

  DBusError err;
  dbus_error_init(&err);
  dbus_bus_add_match(g_conn, active->matchRule.c_str(), &err);
  if (dbus_error_is_set(&err)) {
    dbus_error_free(&err);
    dbus_message_unref(msg);
    return false;
  }

  DBusPendingCall *pending = nullptr;
  if (!dbus_connection_send_with_reply(g_conn, msg, &pending, 15000) || !pending) {
    dbus_bus_remove_match(g_conn, active->matchRule.c_str(), nullptr);
    dbus_message_unref(msg);
    return false;
  }
  dbus_message_unref(msg);

  g_active = active.release();
  if (!dbus_pending_call_set_notify(pending, onReply, nullptr, nullptr)) {
    dbus_pending_call_unref(pending);
    finish(FileDialogResult::Status::Unavailable);
    return true;
  }
  dbus_connection_flush(g_conn);
  return true;
}

void pollPlatformFileDialog() {
  if (!g_conn || !g_active) return;
  if (!dbus_connection_read_write(g_conn, 0)) {
    finish(FileDialogResult::Status::Cancelled);
    return;
  }
  while (dbus_connection_dispatch(g_conn) == DBUS_DISPATCH_DATA_REMAINS) {
  }
}

} // namespace n8v::detail
