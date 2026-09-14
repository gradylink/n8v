#include "color.hpp"
#include "tui_backend_impl.hpp"

#include "core/native_widget_meta.hpp"

#include <algorithm>
#include <cmath>

namespace n8v::detail {

namespace {
float fractionFromPointer(const CellRect &rect, float pointerX) {
  if (rect.w <= 0) return 0.0f;
  float localX = pointerX - (float)rect.x * cellPxW;
  float fraction = localX / ((float)rect.w * cellPxW);
  return std::clamp(fraction, 0.0f, 1.0f);
}

void applySliderValue(NativeWidgetMeta &meta, float fraction) {
  if (!meta.sliderValue) return;
  float newValue = meta.sliderMin + fraction * (meta.sliderMax - meta.sliderMin);
  if (newValue == *meta.sliderValue) return;
  *meta.sliderValue = newValue;
  if (meta.onSliderChange) meta.onSliderChange(newValue, meta.onSliderChangeUserdata);
}
} // namespace

void TuiBackend::renderSlider(NativeWidgetMeta &meta, const Clay_RenderCommand &command) {
  CellRect rect = cellRect(command.boundingBox);
  if (!grid_.clip(rect) || rect.w <= 0 || rect.h <= 0) return;

  bool hit = Clay_PointerOver(Clay_ElementId{command.id});
  if (justClicked_ && hit) {
    draggingSliderOrdinal_ = meta.ordinal;
    applySliderValue(meta, fractionFromPointer(rect, pointerX_));
  } else if (pointerDown_ && draggingSliderOrdinal_ == meta.ordinal) {
    applySliderValue(meta, fractionFromPointer(rect, pointerX_));
  } else if (!pointerDown_ && draggingSliderOrdinal_ == meta.ordinal) {
    draggingSliderOrdinal_ = -1;
  }

  if (pendingSliderKeyStep_ != 0 && meta.ordinal == focusedOrdinal_) {
    float range = meta.sliderMax - meta.sliderMin;
    float step = range > 0.0f ? range / 20.0f : 0.0f; // 20 keyboard steps end-to-end
    if (meta.sliderValue) applySliderValue(meta, std::clamp(((*meta.sliderValue + (float)pendingSliderKeyStep_ * step) - meta.sliderMin) / std::max(range, 0.0001f), 0.0f, 1.0f));
    pendingSliderKeyStep_ = 0;
  }

  float range = meta.sliderMax - meta.sliderMin;
  float value = meta.sliderValue ? *meta.sliderValue : meta.sliderMin;
  float fraction = range > 0.0f ? std::clamp((value - meta.sliderMin) / range, 0.0f, 1.0f) : 0.0f;
  int thumbCell = std::clamp((int)std::lround(fraction * (float)(rect.w - 1)), 0, std::max(rect.w - 1, 0));

  ftxui::Color fillColor = ftxui::Color::RGB(40, 90, 200);
  ftxui::Color trackColor = ftxui::Color::RGB(210, 210, 210);
  for (int x = 0; x < rect.w; ++x) {
    grid_.fillBackground({rect.x + x, rect.y, 1, rect.h}, x <= thumbCell ? fillColor : trackColor);
  }
  for (int y = 0; y < rect.h; ++y) {
    grid_.setGlyph(rect.x + thumbCell, rect.y + y, "●", ftxui::Color::RGB(255, 255, 255), true);
  }
}

} // namespace n8v::detail
