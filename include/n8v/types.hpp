#pragma once

#include <cstdint>

namespace n8v {

enum class Direction {
  Horizontal,
  Vertical,
};

enum class ButtonStyle {
  Primary,
  Secondary,
};

enum class StyleFamily {
  Plain,
  Material,
  Cupertino,
  Fluent,
};

/** Bundled font families. Selawik has no italic face and falls back to its regular. */
enum class FontFamily {
  DejaVuSans,
  Roboto,
  Inter,
  Selawik,
};

enum class CursorKind {
  Default,
  Pointer,
};

enum class NativeWidgetKind {
  Button,
  Link,
};

struct Color {
  float r = 0, g = 0, b = 0, a = 255;
};

struct Padding {
  uint16_t left = 0, right = 0, top = 0, bottom = 0;
};

struct CornerRadius {
  float topLeft = 0, topRight = 0, bottomLeft = 0, bottomRight = 0;
};

} // namespace n8v
