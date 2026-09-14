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

n8v_checkbox_options pendingCheckboxOpts;

void dispatchCheckboxToggle(Clay_ElementId /*elementId*/, Clay_PointerData pointerData, void *userData) {
  if (pointerData.state != CLAY_POINTER_DATA_PRESSED_THIS_FRAME) return;
  auto *meta = static_cast<n8v::detail::NativeWidgetMeta *>(userData);
  if (!meta || !meta->checked) return;
  *meta->checked = !*meta->checked;
  if (meta->onChange) meta->onChange(*meta->checked, meta->onChangeUserdata);
}

} // namespace

namespace n8v::detail::ui_internal {

void resetCheckboxFrameState() {}

} // namespace n8v::detail::ui_internal

extern "C" {

void _n8v_set_checkbox_opts(n8v_checkbox_options opts) { pendingCheckboxOpts = opts; }

void _n8v_checkbox_commit(const char *label) {
  n8v_checkbox_options opts = pendingCheckboxOpts;
  std::string_view labelView = toView(label);

  Clay__OpenElement();

  const int ordinal = widgetOrdinal++;
  const bool hovered = Clay_Hovered();
  const bool pressed = hovered && n8v::activeBackend().pointerDown();
  const bool checkedValue = opts.checked && *opts.checked;
  const n8v::CheckboxPaint paint = n8v::activePaint().checkbox(checkedValue, hovered, pressed);
  const n8v::TextPaint labelPaint = n8v::activePaint().text({});

  Clay_Dimensions labelDims = n8v::activeBackend().measureText(labelView, labelPaint.font, labelPaint.fontSize, false, false);
  float indicatorSize = paint.indicatorSize > 0.0f ? paint.indicatorSize : labelDims.height;
  float indicatorGap = indicatorSize * 0.4f;

  Clay_Dimensions nativeSize = n8v::activeBackend().measureNativeChrome(n8v::NativeWidgetKind::Checkbox, labelView, labelPaint.fontSize);

  Clay_ElementDeclaration decl = {};
  n8v::Padding pad = paint.padding;
  pad.left = nativeSize.width > 0.0f ? (uint16_t)nativeSize.width : (uint16_t)(indicatorSize + indicatorGap);
  decl.layout.padding = n8v::detail::toClay(pad);
  decl.backgroundColor = {0, 0, 0, 1};

  float radius = easeValue(animKey(ordinal, 0), paint.cornerRadius.topLeft, paint.transitionSeconds);

  if (nativeSize.width > 0 && nativeSize.height > 0) {
    decl.layout.sizing.width = CLAY_SIZING_FIXED(nativeSize.width);
    decl.layout.sizing.height = CLAY_SIZING_FIXED(nativeSize.height);
  }

  widgetMetaStorage.push_back(n8v::detail::NativeWidgetMeta{});
  n8v::detail::NativeWidgetMeta &meta = widgetMetaStorage.back();
  meta.kind = n8v::NativeWidgetKind::Checkbox;
  meta.ordinal = ordinal;
  meta.checked = opts.checked;
  meta.onChange = opts.on_change;
  meta.onChangeUserdata = opts.on_change_userdata;
  meta.indicatorFillColor = easeColor(ordinal, 4, paint.background, paint.transitionSeconds);
  meta.indicatorBorderColor = easeColor(ordinal, 8, paint.borderColor, paint.transitionSeconds);
  meta.indicatorBorderWidth = easeValue(animKey(ordinal, 12), paint.borderWidth, paint.transitionSeconds);
  meta.indicatorGlyphColor = easeColor(ordinal, 16, paint.checkColor, paint.transitionSeconds);
  meta.indicatorCornerRadius = radius;
  meta.indicatorSize = indicatorSize;
  decl.userData = &meta;

  Clay__ConfigureOpenElement(decl);

  if (opts.checked) {
    Clay_OnHover(dispatchCheckboxToggle, &widgetMetaStorage.back());
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
