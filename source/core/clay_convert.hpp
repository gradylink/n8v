#pragma once

#include <n8v/types.hpp>

#include <clay.h>

namespace n8v::detail {

inline Clay_Color toClay(const Color &color) { return {color.r, color.g, color.b, color.a}; }

inline Clay_Padding toClay(const Padding &padding) { return {padding.left, padding.right, padding.top, padding.bottom}; }

inline Clay_CornerRadius toClay(const CornerRadius &radius) { return {radius.topLeft, radius.topRight, radius.bottomLeft, radius.bottomRight}; }

inline Clay_LayoutAlignmentX toClayX(Align align) {
  switch (align) {
  case Align::Start:
    return CLAY_ALIGN_X_LEFT;
  case Align::Center:
    return CLAY_ALIGN_X_CENTER;
  case Align::End:
    return CLAY_ALIGN_X_RIGHT;
  }
  return CLAY_ALIGN_X_LEFT;
}

inline Clay_LayoutAlignmentY toClayY(Align align) {
  switch (align) {
  case Align::Start:
    return CLAY_ALIGN_Y_TOP;
  case Align::Center:
    return CLAY_ALIGN_Y_CENTER;
  case Align::End:
    return CLAY_ALIGN_Y_BOTTOM;
  }
  return CLAY_ALIGN_Y_TOP;
}

inline Clay_SizingAxis toClay(const Sizing &sizing) {
  switch (sizing.mode) {
  case SizingMode::Fit:
    return CLAY_SIZING_FIT(sizing.min, sizing.max);
  case SizingMode::Grow:
    return CLAY_SIZING_GROW(sizing.min, sizing.max);
  case SizingMode::Fixed:
    return CLAY_SIZING_FIXED(sizing.value);
  case SizingMode::Percent:
    return CLAY_SIZING_PERCENT(sizing.value);
  }
  return CLAY_SIZING_FIT(0, 0);
}

} // namespace n8v::detail
