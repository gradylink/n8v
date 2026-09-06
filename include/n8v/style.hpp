#pragma once

#include <n8v/options.hpp>
#include <n8v/types.hpp>

namespace n8v {

struct ButtonPaint {
  Color background;
  Color hoverBackground;
  Color textColor;
  CornerRadius cornerRadius;
  Padding padding;
};

struct Paint {
  virtual ~Paint() = default;
  virtual ButtonPaint button(ButtonStyle style, bool hovered) const = 0;
  virtual Color text(const TextOptions &options) const = 0;
};

/** Only Plain is currently registered - setStyleFamily() has no other family to switch to yet. */
const Paint &activePaint();

void setStyleFamily(StyleFamily family);
StyleFamily activeStyleFamily();

} // namespace n8v
