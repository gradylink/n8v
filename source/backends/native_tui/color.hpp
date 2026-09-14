#pragma once

#include <clay.h>
#include <ftxui/screen/color.hpp>

#include <algorithm>

namespace n8v::detail {

inline uint8_t clampByte(float v) { return (uint8_t)std::clamp(v, 0.0f, 255.0f); }

inline ftxui::Color toFtxuiColor(const Clay_Color &c) { return ftxui::Color::RGB(clampByte(c.r), clampByte(c.g), clampByte(c.b)); }

} // namespace n8v::detail
