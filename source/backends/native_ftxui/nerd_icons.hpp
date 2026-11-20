#pragma once

#include <string_view>
#include <utility>

namespace n8v::detail {

inline const char *nerdIconGlyph(std::string_view name) {
  static const std::pair<std::string_view, const char *> glyphs[] = {
    {"add", ""},           {"arrow-down", ""},   {"arrow-left", ""},
    {"arrow-right", ""},   {"arrow-up", ""},     {"calendar", ""},
    {"check", ""},         {"chevron-down", ""}, {"chevron-left", ""},
    {"chevron-right", ""}, {"chevron-up", ""},   {"clock", ""},
    {"close", ""},         {"copy", ""},         {"delete", ""},
    {"download", ""},      {"edit", ""},         {"error", ""},
    {"external-link", ""}, {"file", ""},         {"folder", ""},
    {"heart", ""},         {"home", ""},         {"image", ""},
    {"info", ""},          {"link", ""},         {"lock", ""},
    {"mail", ""},          {"mention", ""},      {"menu", ""},
    {"mute", ""},          {"notification", ""}, {"notification-off", ""},
    {"pin", ""},           {"refresh", ""},      {"remove", ""},
    {"search", ""},        {"settings", ""},     {"share", ""},
    {"star", ""},          {"unlock", ""},       {"upload", ""},
    {"user", ""},          {"visibility", ""},   {"visibility-off", ""},
    {"volume", ""},        {"warning", ""},      {"wifi", ""},
  };
  for (const auto &[key, glyph] : glyphs)
    if (key == name) return glyph;
  return nullptr;
}

} // namespace n8v::detail
