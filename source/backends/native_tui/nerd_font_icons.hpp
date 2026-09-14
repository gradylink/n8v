#pragma once

#include <string_view>
#include <unordered_map>

namespace n8v::detail {

inline const std::unordered_map<std::string_view, char32_t> &nerdFontCodepoints() {
  static const std::unordered_map<std::string_view, char32_t> table = {
    {"settings", 0xf0493},      {"search", 0xf0349},         {"check", 0xf012c},        {"close", 0xf0156},      {"menu", 0xf035c},        {"add", 0xf0415},
    {"remove", 0xf0374},        {"delete", 0xf01b4},         {"edit", 0xf03eb},         {"copy", 0xf018f},       {"download", 0xf01da},    {"upload", 0xf0552},
    {"refresh", 0xf0450},       {"share", 0xf0497},          {"star", 0xf04ce},         {"heart", 0xf02d1},      {"home", 0xf02dc},        {"info", 0xf02fc},
    {"warning", 0xf0026},       {"error", 0xf0028},          {"notification", 0xf009a}, {"calendar", 0xf00ed},   {"clock", 0xf0150},       {"mail", 0xf01ee},
    {"folder", 0xf024b},        {"file", 0xf0214},           {"image", 0xf02e9},        {"link", 0xf0337},       {"lock", 0xf033e},        {"unlock", 0xf033f},
    {"visibility", 0xf0208},    {"visibility-off", 0xf0209}, {"wifi", 0xf05a9},         {"volume", 0xf057e},     {"mute", 0xf0581},        {"chevron-left", 0xf0141},
    {"chevron-right", 0xf0142}, {"chevron-up", 0xf0143},     {"chevron-down", 0xf0140}, {"arrow-left", 0xf004d}, {"arrow-right", 0xf0054}, {"arrow-up", 0xf005d},
    {"arrow-down", 0xf0045},    {"external-link", 0xf03cc},  {"user", 0xf0004},
  };
  return table;
}

inline std::string utf8Encode(char32_t cp) {
  std::string out;
  if (cp <= 0x7F) {
    out += (char)cp;
  } else if (cp <= 0x7FF) {
    out += (char)(0xC0 | (cp >> 6));
    out += (char)(0x80 | (cp & 0x3F));
  } else if (cp <= 0xFFFF) {
    out += (char)(0xE0 | (cp >> 12));
    out += (char)(0x80 | ((cp >> 6) & 0x3F));
    out += (char)(0x80 | (cp & 0x3F));
  } else {
    out += (char)(0xF0 | (cp >> 18));
    out += (char)(0x80 | ((cp >> 12) & 0x3F));
    out += (char)(0x80 | ((cp >> 6) & 0x3F));
    out += (char)(0x80 | (cp & 0x3F));
  }
  return out;
}

inline std::string nerdFontGlyph(std::string_view name) {
  auto &table = nerdFontCodepoints();
  auto it = table.find(name);
  if (it == table.end()) return {};
  return utf8Encode(it->second);
}

} // namespace n8v::detail
