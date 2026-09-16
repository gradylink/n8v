#pragma once

#include <n8v/options.hpp>
#include <n8v/types.hpp>

#include <cstdint>

namespace n8v {

struct ButtonPaint {
  Color background;
  Color textColor;
  CornerRadius cornerRadius;
  Padding padding;
  FontFamily font = FontFamily::DejaVuSans;
  uint16_t fontSize = 16;
  float transitionSeconds = 0.0f;
};

struct TextPaint {
  Color color;
  FontFamily font = FontFamily::DejaVuSans;
  uint16_t fontSize = 16;
};

struct CheckboxPaint {
  Color background = {0, 0, 0, 0};
  Color borderColor = {0, 0, 0, 0};
  float borderWidth = 0.0f;
  Color checkColor = {0, 0, 0, 0};
  CornerRadius cornerRadius;
  float indicatorSize = 0.0f;
  Padding padding;
  FontFamily font = FontFamily::DejaVuSans;
  uint16_t fontSize = 16;
  float transitionSeconds = 0.0f;
};

struct RadioPaint {
  Color background = {0, 0, 0, 0};
  Color borderColor = {0, 0, 0, 0};
  float borderWidth = 0.0f;
  Color dotColor = {0, 0, 0, 0};
  float indicatorSize = 0.0f;
  Padding padding;
  FontFamily font = FontFamily::DejaVuSans;
  uint16_t fontSize = 16;
  float transitionSeconds = 0.0f;
};

struct TogglePaint {
  Color trackOnColor = {0, 0, 0, 0};
  Color trackOffColor = {0, 0, 0, 0};
  Color trackBorderColor = {0, 0, 0, 0};
  float trackBorderWidth = 0.0f;
  Color knobOnColor = {0, 0, 0, 0};
  Color knobOffColor = {0, 0, 0, 0};
  Color knobGlyphColor = {0, 0, 0, 0};
  bool showGlyphWhenOn = false;
  float trackWidth = 0.0f;
  float trackHeight = 0.0f;
  float knobSizeOff = 0.0f;
  float knobSizeOn = 0.0f;
  Padding padding;
  FontFamily font = FontFamily::DejaVuSans;
  uint16_t fontSize = 16;
  float transitionSeconds = 0.0f;
};

struct EntryPaint {
  Color background;
  Color textColor;
  Color placeholderColor;
  Color borderColor = {0, 0, 0, 0};
  float borderWidth = 0.0f;
  bool outlined = false;
  Color labelColor = {0, 0, 0, 0};
  uint16_t labelFontSize = 12;
  float transitionSeconds = 0.0f;
  CornerRadius cornerRadius;
  Padding padding;
  FontFamily font = FontFamily::DejaVuSans;
  uint16_t fontSize = 16;
};

struct DropdownPaint {
  Color background;
  Color textColor;
  Color placeholderColor;
  Color popupBackground;
  Color itemHoverBackground;
  Color itemSelectedBackground = {0, 0, 0, 0};
  CornerRadius cornerRadius;
  Padding padding;
  FontFamily font = FontFamily::DejaVuSans;
  uint16_t fontSize = 16;
  float transitionSeconds = 0.0f;
  Color labelColor = {0, 0, 0, 0};
  uint16_t labelFontSize = 12;
  Color indicatorColor = {0, 0, 0, 0};
  float indicatorWidth = 0.0f;
};

struct ImagePaint {
  CornerRadius cornerRadius;
};

struct IconPaint {
  Color tint;
  float defaultSize = 20.0f;
};

struct SliderPaint {
  Color trackColor;
  Color fillColor;
  Color thumbColor;
  float trackHeight = 20.0f;
  float thumbWidth = 16.0f;
  float thumbHeight = 16.0f;
  Color thumbBorderColor = {0, 0, 0, 0};
  float thumbBorderWidth = 0.0f;
  float trackGap = 0.0f;
};

struct SidebarPaint {
  Color background = {0, 0, 0, 0};
  Color borderColor = {0, 0, 0, 0};
  float borderWidth = 0.0f;
  CornerRadius cornerRadius;
  Padding padding;
  float rowGap = 4.0f;
};

struct Paint {
  virtual ~Paint() = default;
  virtual ButtonPaint button(ButtonStyle style, bool hovered, bool pressed) const = 0;
  virtual TextPaint text(const TextOptions &options) const = 0;
  virtual CheckboxPaint checkbox(bool checked, bool hovered, bool pressed) const = 0;
  virtual RadioPaint radio(bool selected, bool hovered, bool pressed) const = 0;
  virtual TogglePaint toggle(bool on, bool hovered, bool pressed) const = 0;
  virtual EntryPaint entry(bool focused, bool hasValue) const = 0;
  virtual DropdownPaint dropdown(bool open, bool hasSelection, bool hovered, bool pressed) const = 0;
  virtual SliderPaint slider(bool hovered, bool pressed) const = 0;
  virtual ImagePaint image() const = 0;
  virtual IconPaint icon() const = 0;
  virtual SidebarPaint sidebar() const = 0;
};

} // namespace n8v
