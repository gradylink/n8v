#pragma once

#include <n8v/options.hpp>
#include <n8v/types.hpp>

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
  Color background;
  Color textColor;
  CornerRadius cornerRadius;
  Padding padding;
  FontFamily font = FontFamily::DejaVuSans;
  uint16_t fontSize = 16;
  float transitionSeconds = 0.0f;
};

struct Paint {
  virtual ~Paint() = default;
  virtual ButtonPaint button(ButtonStyle style, bool hovered, bool pressed) const = 0;
  virtual TextPaint text(const TextOptions &options) const = 0;
  virtual CheckboxPaint checkbox(bool checked, bool hovered, bool pressed) const = 0;
};

const Paint &activePaint();

/** Defaults to the N8V_STYLE env var (plain/material/cupertino/fluent), else Plain. */
void setStyleFamily(StyleFamily family);
StyleFamily activeStyleFamily();

} // namespace n8v
