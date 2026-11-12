#pragma once

#include <n8v/n8v_c.h>

namespace n8v::detail {

inline bool isThemeColor(const n8v_color &c) { return c.a < 0.0f; }

inline n8v_named_color themeColorName(const n8v_color &c) { return (n8v_named_color)(int)c.r; }

inline const char *themeColorSlug(n8v_named_color name) {
  switch (name) {
  case N8V_NAMED_COLOR_RED:
    return "red";
  case N8V_NAMED_COLOR_ORANGE:
    return "orange";
  case N8V_NAMED_COLOR_YELLOW:
    return "yellow";
  case N8V_NAMED_COLOR_GREEN:
    return "green";
  case N8V_NAMED_COLOR_BLUE:
    return "blue";
  case N8V_NAMED_COLOR_PURPLE:
    return "purple";
  case N8V_NAMED_COLOR_PINK:
    return "pink";
  default:
    return nullptr;
  }
}

inline n8v_color builtinThemeColor(n8v_named_color name, bool dark) {
  struct Pair {
    float light[3];
    float darkVariant[3];
  };
  static const Pair table[] = {
    {{0, 0, 0}, {0, 0, 0}},
    {{224, 27, 36}, {255, 123, 99}},   // red
    {{230, 97, 0}, {255, 163, 72}},    // orange
    {{245, 194, 17}, {248, 228, 92}},  // yellow
    {{38, 162, 105}, {143, 240, 164}}, // green
    {{53, 132, 228}, {153, 193, 241}}, // blue
    {{145, 65, 172}, {220, 138, 221}}, // purple
    {{214, 66, 143}, {255, 143, 199}}, // pink
  };
  const int index = (int)name;
  if (index <= 0 || index >= (int)(sizeof(table) / sizeof(table[0]))) return {0, 0, 0, 0};
  const float *rgb = dark ? table[index].darkVariant : table[index].light;
  return {rgb[0], rgb[1], rgb[2], 255.0f};
}

inline n8v_color resolveThemeColor(const n8v_color &c, bool dark = false) { return isThemeColor(c) ? builtinThemeColor(themeColorName(c), dark) : c; }

} // namespace n8v::detail
