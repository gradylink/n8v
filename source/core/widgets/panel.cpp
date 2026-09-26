#include <n8v/n8v_c.h>

#include "core/backend.hpp"
#include "core/style.hpp"

#include "core/clay_convert.hpp"
#include "core/native_widget_meta.hpp"
#include "core/ui_core_internal.hpp"

#include <clay.h>

#include <cmath>

namespace {

using namespace n8v::detail::ui_internal;

uint16_t snapToGrid(uint16_t value, float unit) {
  if (unit <= 0.0f || value == 0) return value;
  return (uint16_t)(std::lround((float)value / unit) * unit);
}

uint16_t snapGapToGrid(uint16_t value, float unit) {
  uint16_t rounded = snapToGrid(value, unit);
  if (rounded == 0 && value > 0 && unit > 0.0f) rounded = (uint16_t)unit;
  return rounded;
}

} // namespace

extern "C" {

void n8v_open_panel(n8v_panel_options opts) {
  Clay__OpenElement();

  const int ordinal = widgetOrdinal++;
  const n8v::PanelRole role = toPanelRole(opts.role);
  const bool hovered = Clay_Hovered();
  const bool pressed = hovered && n8v::activeBackend().pointerDown();
  const n8v::PanelPaint paint = n8v::activePaint().panel(role, hovered, pressed);

  n8v::Padding pad = paint.padding;
  uint16_t gap = opts.gap;
  Clay_Dimensions cell = n8v::activeBackend().cellSize();
  if (cell.width > 0.0f && cell.height > 0.0f) {
    bool scrollingThisAxis = opts.direction == N8V_DIRECTION_HORIZONTAL ? opts.clip_horizontal : opts.clip_vertical;
    float gapUnit = opts.direction == N8V_DIRECTION_HORIZONTAL ? cell.width : cell.height;
    gap = scrollingThisAxis ? snapToGrid(gap, gapUnit) : snapGapToGrid(gap, gapUnit);
    pad.left = snapToGrid(pad.left, cell.width);
    pad.right = snapToGrid(pad.right, cell.width);
    pad.top = snapToGrid(pad.top, cell.height);
    pad.bottom = snapToGrid(pad.bottom, cell.height);
  }

  Clay_ElementDeclaration decl = {};
  decl.layout.layoutDirection = opts.direction == N8V_DIRECTION_HORIZONTAL ? CLAY_LEFT_TO_RIGHT : CLAY_TOP_TO_BOTTOM;
  decl.layout.childGap = gap;
  decl.layout.padding = n8v::detail::toClay(pad);
  decl.layout.childAlignment = {n8v::detail::toClayX(toAlign(opts.h_align)), n8v::detail::toClayY(toAlign(opts.v_align))};
  decl.layout.sizing.width = n8v::detail::toClay(toSizing(opts.width));
  decl.layout.sizing.height = n8v::detail::toClay(toSizing(opts.height));
  decl.clip.horizontal = opts.clip_horizontal;
  decl.clip.vertical = opts.clip_vertical;
  if ((opts.clip_horizontal || opts.clip_vertical) && n8v::activeBackend().ownsScrollMath()) {
    decl.clip.childOffset = Clay_GetScrollOffset();
  }
  // Clay skips emitting a rectangle render command entirely when alpha is exactly 0
  // (see clay.h's `backgroundColor.a > 0` gate). For native-chrome backends that
  // command is what drives creating the panel's underlying host widget, so a fully
  // transparent background would make it never get created at all. Clamp to a
  // barely-nonzero alpha so the host widget always exists.
  n8v::Color background = paint.background;
  if (background.a <= 0.0f) background.a = 1.0f;
  decl.backgroundColor = n8v::detail::toClay(background);
  decl.cornerRadius = n8v::detail::toClay(paint.cornerRadius);
  decl.border.color = n8v::detail::toClay(paint.borderColor);
  decl.border.width = {(uint16_t)paint.borderWidth, (uint16_t)paint.borderWidth, (uint16_t)paint.borderWidth, (uint16_t)paint.borderWidth, 0};

  widgetMetaStorage.push_back(n8v::detail::NativeWidgetMeta{});
  n8v::detail::NativeWidgetMeta &meta = widgetMetaStorage.back();
  meta.kind = n8v::NativeWidgetKind::Panel;
  meta.ordinal = ordinal;
  meta.panelRole = role;
  meta.panelBackground = paint.background;
  meta.panelBorderColor = paint.borderColor;
  meta.panelBorderWidth = paint.borderWidth;
  meta.panelCornerRadius = paint.cornerRadius;
  decl.userData = &meta;

  Clay__ConfigureOpenElement(decl);
}

void n8v_close_panel(void) { Clay__CloseElement(); }

} // extern "C"
