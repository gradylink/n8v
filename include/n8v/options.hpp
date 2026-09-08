#pragma once

#include <n8v/types.hpp>

#include <functional>
#include <string>
#include <string_view>

namespace n8v {

struct FlexOptions {
  Direction direction = Direction::Horizontal;
  uint16_t gap = 0;
  Padding padding = {};
  Align hAlign = Align::Start;
  Align vAlign = Align::Start;
};

struct TextOptions {
  bool bold = false;
  bool italic = false;
  /** If non-empty, the text is painted as a link (color + underline) to this URL. */
  std::string_view url = {};
  /** Ignored by style families that pick their own color (e.g. links). */
  Color color = {0, 0, 0, 255};
};

struct ButtonOptions {
  ButtonStyle style = ButtonStyle::Primary;
  /** Fires on press, not release. */
  std::function<void()> onClick = nullptr;
};

struct CheckboxOptions {
  bool *checked = nullptr;
  std::function<void(bool)> onChange = nullptr;
};

struct EntryOptions {
  std::string *value = nullptr;
  std::string_view placeholder = {};
  bool password = false;
  std::function<void(std::string_view)> onChange = nullptr;
};

} // namespace n8v
