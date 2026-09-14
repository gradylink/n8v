#include <n8v/n8v_c.h>

#include "core/backend.hpp"
#include "core/style.hpp"

#include "core/clay_convert.hpp"
#include "core/native_widget_meta.hpp"
#include "core/text_style_flags.hpp"
#include "core/ui_core_internal.hpp"

#include <clay.h>

namespace {

using namespace n8v::detail::ui_internal;

n8v_radio_options pendingRadioOpts;

void dispatchRadioSelect(Clay_ElementId /*elementId*/, Clay_PointerData pointerData, void *userData) {
  if (pointerData.state != CLAY_POINTER_DATA_PRESSED_THIS_FRAME) return;
  auto *meta = static_cast<n8v::detail::NativeWidgetMeta *>(userData);
  if (!meta || !meta->radioSelected || *meta->radioSelected == meta->radioValue) return;
  *meta->radioSelected = meta->radioValue;
  if (meta->onRadioChange) meta->onRadioChange(meta->radioValue, meta->onRadioChangeUserdata);
}

} // namespace

namespace n8v::detail::ui_internal {

void resetRadioFrameState() {}

} // namespace n8v::detail::ui_internal

extern "C" {

void _n8v_set_radio_opts(n8v_radio_options opts) { pendingRadioOpts = opts; }

void _n8v_radio_commit(const char *label) {
  n8v_radio_options opts = pendingRadioOpts;
  std::string_view labelView = toView(label);

  Clay__OpenElement();

  const int ordinal = widgetOrdinal++;
  const bool hovered = Clay_Hovered();
  const bool pressed = hovered && n8v::activeBackend().pointerDown();
  const bool selectedValue = opts.selected && *opts.selected == opts.value;
  const n8v::RadioPaint paint = n8v::activePaint().radio(selectedValue, hovered, pressed);
  const n8v::TextPaint labelPaint = n8v::activePaint().text({});

  Clay_Dimensions labelDims = n8v::activeBackend().measureText(labelView, labelPaint.font, labelPaint.fontSize, false, false);
  float indicatorSize = paint.indicatorSize > 0.0f ? paint.indicatorSize : labelDims.height;
  float indicatorGap = indicatorSize * 0.4f;

  Clay_Dimensions nativeSize = n8v::activeBackend().measureNativeChrome(n8v::NativeWidgetKind::Radio, labelView, labelPaint.fontSize);

  Clay_ElementDeclaration decl = {};
  n8v::Padding pad = paint.padding;
  pad.left = nativeSize.width > 0.0f ? (uint16_t)nativeSize.width : (uint16_t)(indicatorSize + indicatorGap);
  decl.layout.padding = n8v::detail::toClay(pad);
  decl.backgroundColor = {0, 0, 0, 1};

  if (nativeSize.width > 0 && nativeSize.height > 0) {
    decl.layout.sizing.width = CLAY_SIZING_FIXED(nativeSize.width);
    decl.layout.sizing.height = CLAY_SIZING_FIXED(nativeSize.height);
  }

  widgetMetaStorage.push_back(n8v::detail::NativeWidgetMeta{});
  n8v::detail::NativeWidgetMeta &meta = widgetMetaStorage.back();
  meta.kind = n8v::NativeWidgetKind::Radio;
  meta.ordinal = ordinal;
  meta.radioSelected = opts.selected;
  meta.radioValue = opts.value;
  meta.onRadioChange = opts.on_change;
  meta.onRadioChangeUserdata = opts.on_change_userdata;
  meta.indicatorFillColor = easeColor(ordinal, 4, paint.background, paint.transitionSeconds);
  meta.indicatorBorderColor = easeColor(ordinal, 8, paint.borderColor, paint.transitionSeconds);
  meta.indicatorBorderWidth = easeValue(animKey(ordinal, 12), paint.borderWidth, paint.transitionSeconds);
  meta.indicatorGlyphColor = easeColor(ordinal, 16, paint.dotColor, paint.transitionSeconds);
  meta.indicatorGlyphScale = easeValue(animKey(ordinal, 20), selectedValue ? 1.0f : 0.0f, paint.transitionSeconds);
  meta.indicatorSize = indicatorSize;
  decl.userData = &meta;

  Clay__ConfigureOpenElement(decl);

  if (opts.selected) {
    Clay_OnHover(dispatchRadioSelect, &meta);
  }

  textStyleStorage.push_back(n8v::detail::TextStyleFlags{labelPaint.font, false, false, false, true, ordinal});
  Clay_TextElementConfig textConfig = {};
  textConfig.textColor = n8v::detail::toClay(labelPaint.color);
  textConfig.fontSize = labelPaint.fontSize;
  textConfig.wrapMode = CLAY_TEXT_WRAP_NONE;
  textConfig.userData = &textStyleStorage.back();
  CLAY_TEXT(internString(labelView), textConfig);

  Clay__CloseElement();
}

} // extern "C"
