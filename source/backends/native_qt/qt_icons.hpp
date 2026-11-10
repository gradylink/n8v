#pragma once

#include <QStyle>

#include <string_view>
#include <unordered_map>

namespace n8v::detail::qt_icons {

inline bool resolveStandardPixmap(std::string_view name, QStyle::StandardPixmap &out) {
  static const std::unordered_map<std::string_view, QStyle::StandardPixmap> table = {
    {"check", QStyle::SP_DialogApplyButton},
    {"close", QStyle::SP_DialogCloseButton},
    {"delete", QStyle::SP_TrashIcon},
    {"refresh", QStyle::SP_BrowserReload},
    {"home", QStyle::SP_DirHomeIcon},
    {"folder", QStyle::SP_DirIcon},
    {"file", QStyle::SP_FileIcon},
    {"warning", QStyle::SP_MessageBoxWarning},
    {"error", QStyle::SP_MessageBoxCritical},
    {"info", QStyle::SP_MessageBoxInformation},
    {"volume", QStyle::SP_MediaVolume},
    {"mute", QStyle::SP_MediaVolumeMuted},
    {"menu", QStyle::SP_TitleBarMenuButton},
    {"chevron-left", QStyle::SP_ArrowLeft},
    {"chevron-right", QStyle::SP_ArrowRight},
    {"chevron-up", QStyle::SP_ArrowUp},
    {"chevron-down", QStyle::SP_ArrowDown},
    {"arrow-left", QStyle::SP_ArrowLeft},
    {"arrow-right", QStyle::SP_ArrowRight},
    {"arrow-up", QStyle::SP_ArrowUp},
    {"arrow-down", QStyle::SP_ArrowDown},
  };
  auto it = table.find(name);
  if (it == table.end()) return false;
  out = it->second;
  return true;
}

} // namespace n8v::detail::qt_icons
