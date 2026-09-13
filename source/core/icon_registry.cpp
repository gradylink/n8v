#include "core/icon_registry.hpp"

namespace n8v::detail {
namespace {

struct IconEntry {
  const char *name;
  const char *freedesktop;
};

constexpr IconEntry icons[] = {
  {"settings", "preferences-system-symbolic"},
  {"search", "edit-find-symbolic"},
  {"check", "object-select-symbolic"},
  {"close", "window-close-symbolic"},
  {"menu", "open-menu-symbolic"},
  {"add", "list-add-symbolic"},
  {"remove", "list-remove-symbolic"},
  {"delete", "user-trash-symbolic"},
  {"edit", "document-edit-symbolic"},
  {"copy", "edit-copy-symbolic"},
  {"download", "browser-download-symbolic"},
  {"upload", "export-symbolic"},
  {"refresh", "view-refresh-symbolic"},
  {"share", "emblem-shared-symbolic"},
  {"star", "starred-symbolic"},
  {"heart", "emblem-favorite-symbolic"},
  {"home", "go-home-symbolic"},
  {"info", "dialog-information-symbolic"},
  {"warning", "dialog-warning-symbolic"},
  {"error", "dialog-error-symbolic"},
  {"notification", "preferences-system-notifications-symbolic"},
  {"calendar", "x-office-calendar-symbolic"},
  {"clock", "clock-symbolic"},
  {"mail", "mail-unread-symbolic"},
  {"folder", "folder-symbolic"},
  {"file", "text-x-generic-symbolic"},
  {"image", "image-x-generic-symbolic"},
  {"link", "insert-link-symbolic"},
  {"lock", "system-lock-screen-symbolic"},
  {"unlock", "changes-allow-symbolic"},
  {"visibility", "view-reveal-symbolic"},
  {"visibility-off", "view-conceal-symbolic"},
  {"wifi", "network-wireless-symbolic"},
  {"volume", "audio-volume-high-symbolic"},
  {"mute", "audio-volume-muted-symbolic"},
  {"chevron-left", "pan-start-symbolic"},
  {"chevron-right", "pan-end-symbolic"},
  {"chevron-up", "pan-up-symbolic"},
  {"chevron-down", "pan-down-symbolic"},
  {"arrow-left", "go-previous-symbolic"},
  {"arrow-right", "go-next-symbolic"},
  {"arrow-up", "go-up-symbolic"},
  {"arrow-down", "go-down-symbolic"},
  {"external-link", "external-link-symbolic"},
  {"user", "avatar-default-symbolic"},
};

const IconEntry *findEntry(std::string_view name) {
  for (const IconEntry &entry : icons) {
    if (name == entry.name) return &entry;
  }
  return nullptr;
}

const char *familyDir(n8v::StyleFamily family) {
  switch (family) {
  case n8v::StyleFamily::Plain:
    return "plain";
  case n8v::StyleFamily::Material:
    return "material";
  case n8v::StyleFamily::Cupertino:
    return "cupertino";
  case n8v::StyleFamily::Fluent:
    return "fluent";
  }
  return "plain";
}

} // namespace

bool isKnownIconName(std::string_view name) { return findEntry(name) != nullptr; }

std::string iconAssetPath(n8v::StyleFamily family, std::string_view name) {
  if (!findEntry(name)) return {};
  return std::string(familyDir(family)) + "/" + std::string(name) + ".svg";
}

const char *resolveFreedesktopIconName(std::string_view name) {
  const IconEntry *entry = findEntry(name);
  return entry ? entry->freedesktop : nullptr;
}

} // namespace n8v::detail
