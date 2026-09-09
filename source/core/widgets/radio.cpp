#include <n8v/backend.hpp>
#include <n8v/style.hpp>
#include <n8v/ui.hpp>

#include "core/clay_convert.hpp"
#include "core/native_widget_meta.hpp"
#include "core/text_style_flags.hpp"
#include "core/ui_core_internal.hpp"

#include <clay.h>

#include <deque>
#include <functional>
#include <string_view>

namespace {

using namespace n8v::detail::ui_internal;

std::deque<std::function<void(int)>> radioChangeCallbacks;

void dispatchRadioSelect(Clay_ElementId /*elementId*/, Clay_PointerData pointerData, void *userData) {
  if (pointerData.state != CLAY_POINTER_DATA_PRESSED_THIS_FRAME) return;
  auto *meta = static_cast<n8v::detail::NativeWidgetMeta *>(userData);
  if (!meta || !meta->radioSelected || *meta->radioSelected == meta->radioValue) return;
  *meta->radioSelected = meta->radioValue;
  if (meta->onRadioChange && *meta->onRadioChange) (*meta->onRadioChange)(meta->radioValue);
}

} // namespace

namespace n8v::detail::ui_internal {

void resetRadioFrameState() { radioChangeCallbacks.clear(); }

} // namespace n8v::detail::ui_internal

namespace n8v::detail {

void RadioBuilder::operator()(std::string_view label) && {
  Clay__OpenElement();

  const int ordinal = widgetOrdinal++;
  const bool hovered = Clay_Hovered();
  // if (hovered) pendingCursor = CursorKind::Pointer;
  const bool pressed = hovered && activeBackend().pointerDown();
  const bool selectedValue = options.selected && *options.selected == options.value;
  const RadioPaint paint = activePaint().radio(selectedValue, hovered, pressed);
  const TextPaint labelPaint = activePaint().text({});

  Clay_Dimensions labelDims = activeBackend().measureText(label, labelPaint.font, labelPaint.fontSize, false, false);
  float indicatorSize = paint.indicatorSize > 0.0f ? paint.indicatorSize : labelDims.height;
  float indicatorGap = indicatorSize * 0.4f;

  Clay_ElementDeclaration decl = {};
  Padding pad = paint.padding;
  pad.left = (uint16_t)(indicatorSize + indicatorGap);
  decl.layout.padding = toClay(pad);
  decl.backgroundColor = {0, 0, 0, 1};

  Clay_Dimensions nativeSize = activeBackend().measureNativeChrome(NativeWidgetKind::Radio, label, labelPaint.fontSize);
  if (nativeSize.width > 0 && nativeSize.height > 0) {
    decl.layout.sizing.width = CLAY_SIZING_FIXED(nativeSize.width);
    decl.layout.sizing.height = CLAY_SIZING_FIXED(nativeSize.height);
  }

  widgetMetaStorage.push_back(NativeWidgetMeta{});
  NativeWidgetMeta &meta = widgetMetaStorage.back();
  meta.kind = NativeWidgetKind::Radio;
  meta.ordinal = ordinal;
  meta.radioSelected = options.selected;
  meta.radioValue = options.value;
  if (options.onChange) {
    radioChangeCallbacks.push_back(std::move(options.onChange));
    meta.onRadioChange = &radioChangeCallbacks.back();
  }
  meta.indicatorFillColor = easeColor(ordinal, 4, paint.background, paint.transitionSeconds);
  meta.indicatorBorderColor = easeColor(ordinal, 8, paint.borderColor, paint.transitionSeconds);
  meta.indicatorBorderWidth = easeValue(animKey(ordinal, 12), paint.borderWidth, paint.transitionSeconds);
  meta.indicatorGlyphColor = easeColor(ordinal, 16, paint.dotColor, paint.transitionSeconds);
  meta.indicatorGlyphScale = easeValue(animKey(ordinal, 20), selectedValue ? 1.0f : 0.0f, paint.transitionSeconds);
  meta.indicatorSize = indicatorSize;
  decl.userData = &meta;

  Clay__ConfigureOpenElement(decl);

  if (options.selected) {
    Clay_OnHover(dispatchRadioSelect, &meta);
  }

  textStyleStorage.push_back(n8v::detail::TextStyleFlags{labelPaint.font, false, false, false, true, ordinal});
  Clay_TextElementConfig textConfig = {};
  textConfig.textColor = toClay(labelPaint.color);
  textConfig.fontSize = labelPaint.fontSize;
  textConfig.wrapMode = CLAY_TEXT_WRAP_NONE;
  textConfig.userData = &textStyleStorage.back();
  CLAY_TEXT(internString(label), textConfig);

  Clay__CloseElement();
}

} // namespace n8v::detail
