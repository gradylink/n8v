#pragma once

#include <n8v/types.hpp>

#include <clay.h>

namespace n8v::detail {

inline Clay_Color toClay(const Color &color) { return {color.r, color.g, color.b, color.a}; }

inline Clay_Padding toClay(const Padding &padding) { return {padding.left, padding.right, padding.top, padding.bottom}; }

inline Clay_CornerRadius toClay(const CornerRadius &radius) { return {radius.topLeft, radius.topRight, radius.bottomLeft, radius.bottomRight}; }

} // namespace n8v::detail
