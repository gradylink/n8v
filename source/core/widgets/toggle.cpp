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

n8v_toggle_options pendingToggleOpts;

void dispatchToggleFlip(Clay_ElementId /*elementId*/, Clay_PointerData pointerData, void *userData) {
  if (pointerData.state != CLAY_POINTER_DATA_PRESSED_THIS_FRAME) return;
  auto *meta = static_cast<n8v::detail::NativeWidgetMeta *>(userData);
  if (!meta || !meta->checked) return;
  *meta->checked = !*meta->checked;
  if (meta->onChange) meta->onChange(*meta->checked, meta->onChangeUserdata);
}

} // namespace

namespace n8v::detail::ui_internal {

void resetToggleFrameState() {}

} // namespace n8v::detail::ui_internal

extern "C" {

void _n8v_set_toggle_opts(n8v_toggle_options opts) { pendingToggleOpts = opts; }

void _n8v_toggle_commit(const char *label) {
  n8v_toggle_options opts = pendingToggleOpts;
  std::string_view labelView = toView(label);

  Clay__OpenElement();

  const int ordinal = widgetOrdinal++;
  const bool hovered = Clay_Hovered();
  const bool pressed = hovered && n8v::activeBackend().pointerDown();
  const bool onValue = opts.checked && *opts.checked;
  const n8v::TogglePaint paint = n8v::activePaint().toggle(onValue, hovered, pressed);
  const n8v::TextPaint labelPaint = n8v::activePaint().text({});

  float knobPosition = easeValue(animKey(ordinal, 0), onValue ? 1.0f : 0.0f, paint.transitionSeconds);
  float knobSize = easeValue(animKey(ordinal, 1), onValue ? paint.knobSizeOn : paint.knobSizeOff, paint.transitionSeconds);
  n8v::Color trackColor = easeColor(ordinal, 4, onValue ? paint.trackOnColor : paint.trackOffColor, paint.transitionSeconds);
  n8v::Color trackBorderColor = easeColor(ordinal, 8, paint.trackBorderColor, paint.transitionSeconds);
  float trackBorderWidth = easeValue(animKey(ordinal, 12), onValue ? 0.0f : paint.trackBorderWidth, paint.transitionSeconds);
  n8v::Color knobColor = easeColor(ordinal, 16, onValue ? paint.knobOnColor : paint.knobOffColor, paint.transitionSeconds);
  float glyphScale = easeValue(animKey(ordinal, 20), paint.showGlyphWhenOn && onValue ? 1.0f : 0.0f, paint.transitionSeconds);

  Clay_Dimensions nativeSize = n8v::activeBackend().measureNativeChrome(n8v::NativeWidgetKind::Switch, labelView, labelPaint.fontSize);

  Clay_ElementDeclaration decl = {};
  n8v::Padding pad = paint.padding;
  if (nativeSize.width > 0.0f) {
    pad.left = (uint16_t)nativeSize.width;
  } else if (n8v::activeBackend().aliasesSwitchAsCheckbox()) {
    Clay_Dimensions labelDims = n8v::activeBackend().measureText(labelView, labelPaint.font, labelPaint.fontSize, false, false);
    float indicatorSize = labelDims.height;
    pad.left = (uint16_t)(indicatorSize + indicatorSize * 0.4f);
  } else {
    pad.left = (uint16_t)(paint.trackWidth + paint.trackHeight * 0.5f);
  }
  decl.layout.padding = n8v::detail::toClay(pad);
  decl.backgroundColor = {0, 0, 0, 1};

  if (nativeSize.width > 0 && nativeSize.height > 0) {
    decl.layout.sizing.width = CLAY_SIZING_FIXED(nativeSize.width);
    decl.layout.sizing.height = CLAY_SIZING_FIXED(nativeSize.height);
  }

  widgetMetaStorage.push_back(n8v::detail::NativeWidgetMeta{});
  n8v::detail::NativeWidgetMeta &meta = widgetMetaStorage.back();
  meta.kind = n8v::NativeWidgetKind::Switch;
  meta.ordinal = ordinal;
  meta.checked = opts.checked;
  meta.onChange = opts.on_change;
  meta.onChangeUserdata = opts.on_change_userdata;
  meta.switchTrackColor = trackColor;
  meta.switchTrackBorderColor = trackBorderColor;
  meta.switchTrackBorderWidth = trackBorderWidth;
  meta.switchKnobColor = knobColor;
  meta.switchKnobGlyphColor = paint.knobGlyphColor;
  meta.switchGlyphScale = glyphScale;
  meta.switchKnobPosition = knobPosition;
  meta.switchKnobSize = knobSize;
  meta.switchTrackWidth = paint.trackWidth;
  meta.switchTrackHeight = paint.trackHeight;
  decl.userData = &meta;

  Clay__ConfigureOpenElement(decl);

  if (opts.checked) {
    Clay_OnHover(dispatchToggleFlip, &meta);
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
