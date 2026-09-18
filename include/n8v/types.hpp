#pragma once

#include <cstdint>

namespace n8v {

enum class Direction {
  Horizontal,
  Vertical,
};

enum class Align {
  Start,
  Center,
  End,
};

enum class ButtonStyle {
  Primary,
  Secondary,
  /** Transparent background */
  Ghost,
};

enum class StyleFamily {
  Plain,
  Material,
  Cupertino,
  Fluent,
  /** A user-supplied Paint installed with setCustomPaint(). */
  Custom,
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
  Text,
};

enum class NativeWidgetKind {
  Button,
  Link,
  Checkbox,
  Entry,
  Radio,
  Dropdown,
  Slider,
  DropdownChevron,
  Image,
  Icon,
  Switch,
  Sidebar,
};

enum class IconVariant {
  Outline,
  Filled,
};

enum class IconPosition {
  Leading,
  Trailing,
};

enum class SizingMode {
  /** Shrink to fit children, growing no larger than `max` (0 = unbounded). */
  Fit,
  /** Expand to fill available space in the parent, within [min, max] (max 0 = unbounded). */
  Grow,
  /** Exactly `value` pixels. */
  Fixed,
  /** `value` (0-1) as a fraction of the parent's size on this axis. */
  Percent,
};

struct Sizing {
  SizingMode mode = SizingMode::Fit;
  float value = 0.0f;
  float min = 0.0f;
  float max = 0.0f;

  static Sizing fit(float min = 0.0f, float max = 0.0f) { return {SizingMode::Fit, 0.0f, min, max}; }
  static Sizing grow(float min = 0.0f, float max = 0.0f) { return {SizingMode::Grow, 0.0f, min, max}; }
  static Sizing fixed(float pixels) { return {SizingMode::Fixed, pixels, 0.0f, 0.0f}; }
  static Sizing percent(float fraction) { return {SizingMode::Percent, fraction, 0.0f, 0.0f}; }
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

enum class RoundingMode {
  StyleDefault,
  None,
  Fixed,
};

struct Rounding {
  RoundingMode mode = RoundingMode::StyleDefault;
  CornerRadius radius{};

  static Rounding styleDefault() { return {}; }
  static Rounding none() { return {RoundingMode::None, {}}; }
  static Rounding fixed(CornerRadius r) { return {RoundingMode::Fixed, r}; }
  static Rounding fixed(float uniform) { return {RoundingMode::Fixed, {uniform, uniform, uniform, uniform}}; }
};

} // namespace n8v
