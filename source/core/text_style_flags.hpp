#pragma once

#include <n8v/types.hpp>

namespace n8v::detail {

struct TextStyleFlags {
  FontFamily font = FontFamily::DejaVuSans;
  bool bold = false;
  bool italic = false;
  bool underline = false;
  bool ownedByWidget = false;
  int ordinal = 0;
  bool strikethrough = false;
};

constexpr uint16_t textFontId(FontFamily font, bool bold, bool italic) { return (uint16_t)(((uint16_t)font << 2) | (bold ? 1 : 0) | (italic ? 2 : 0)); }

} // namespace n8v::detail
