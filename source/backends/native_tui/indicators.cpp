#include "color.hpp"
#include "tui_backend_impl.hpp"

#include "core/native_widget_meta.hpp"

namespace n8v::detail {

namespace {
constexpr const char *radioLeftHalf = "\xee\x82\xb6";
constexpr const char *radioRightHalf = "\xee\x82\xb4";
} // namespace

void TuiBackend::renderCheckboxOrRadioIndicator(NativeWidgetMeta &meta, const Clay_BoundingBox &labelBox, bool isRadio) {
  CellRect labelCell = cellRect(labelBox);
  int y = labelCell.y;
  int glyphRight = labelCell.x - 2;
  int glyphLeft = labelCell.x - 3;

  Clay_Color fillColor{meta.indicatorFillColor.r, meta.indicatorFillColor.g, meta.indicatorFillColor.b, meta.indicatorFillColor.a};
  ftxui::Color fg = toFtxuiColor(fillColor);

  if (isRadio) {
    grid_.setGlyph(glyphLeft, y, radioLeftHalf, fg, true);
    grid_.setGlyph(glyphRight, y, radioRightHalf, fg, true);
  } else {
    grid_.setGlyph(glyphRight, y, "✓", fg, true);
  }
}

void TuiBackend::renderSwitchIndicator(NativeWidgetMeta &meta, const Clay_BoundingBox &labelBox) {
  CellRect labelCell = cellRect(labelBox);
  int y = labelCell.y;
  int trackRight = labelCell.x - 2;
  int trackLeft = labelCell.x - 4;

  Clay_Color trackColor{meta.switchTrackColor.r, meta.switchTrackColor.g, meta.switchTrackColor.b, meta.switchTrackColor.a};
  grid_.fillBackground({trackLeft, y, trackRight - trackLeft + 1, 1}, toFtxuiColor(trackColor));

  int knobCellX = meta.switchKnobPosition >= 0.5f ? trackRight : trackLeft;
  Clay_Color knobColor{meta.switchKnobColor.r, meta.switchKnobColor.g, meta.switchKnobColor.b, meta.switchKnobColor.a};
  const char *knobGlyph = meta.switchGlyphScale > 0.5f ? "✓" : "●";
  Clay_Color glyphColor =
    meta.switchGlyphScale > 0.5f ? Clay_Color{meta.switchKnobGlyphColor.r, meta.switchKnobGlyphColor.g, meta.switchKnobGlyphColor.b, meta.switchKnobGlyphColor.a} : knobColor;
  grid_.setGlyph(knobCellX, y, knobGlyph, toFtxuiColor(glyphColor), true);
}

void TuiBackend::drawDropdownChevron(const Clay_BoundingBox &box, const Clay_Color &color, bool pointsUp) {
  CellRect rect = cellRect(box);
  if (rect.w <= 0 || rect.h <= 0) return;
  int cx = rect.x + rect.w / 2;
  int cy = rect.y + rect.h / 2;
  grid_.setGlyph(cx, cy, pointsUp ? "▲" : "▼", toFtxuiColor(color));
}

} // namespace n8v::detail
