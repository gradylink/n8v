#pragma once

#include <n8v/types.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace n8v {

struct FlexOptions {
  Direction direction = Direction::Horizontal;
  uint16_t gap = 0;
  Padding padding = {};
  Align hAlign = Align::Start;
  Align vAlign = Align::Start;
  Sizing width = Sizing::fit();
  Sizing height = Sizing::fit();
  bool clipHorizontal = false;
  bool clipVertical = false;
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

struct RadioOptions {
  /** Shared by every radio button in the same group - selecting one sets *selected to its value. */
  int *selected = nullptr;
  int value = 0;
  std::function<void(int)> onChange = nullptr;
};

struct EntryOptions {
  std::string *value = nullptr;
  std::string_view placeholder = {};
  bool password = false;
  std::function<void(std::string_view)> onChange = nullptr;
};

struct DropdownOptions {
  std::vector<std::string_view> items = {};
  /** Index into `items`. Out of range (including untouched -1) shows `placeholder`. */
  int *selected = nullptr;
  std::string_view placeholder = {};
  std::function<void(int)> onChange = nullptr;
};

struct SliderOptions {
  float *value = nullptr;
  float min = 0.0f;
  float max = 1.0f;
  std::function<void(float)> onChange = nullptr;
};

enum class ImageSource {
  Path,
  Bundle,
  Encoded,
  Rgba,
};

struct ImageOptions {
  ImageSource source = ImageSource::Path;
  std::string path;
  const uint8_t *encodedData = nullptr;
  size_t encodedSize = 0;
  const uint8_t *pixels = nullptr;
  int pixelWidth = 0;
  int pixelHeight = 0;
  Sizing width = Sizing::fit();
  Sizing height = Sizing::fit();
  Rounding rounding = Rounding::styleDefault();
};

} // namespace n8v
