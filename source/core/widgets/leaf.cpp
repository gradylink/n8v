#include <n8v/n8v_c.h>

#include "core/backend.hpp"
#include "core/style.hpp"

#include "core/clay_convert.hpp"
#include "core/icon_loader.hpp"
#include "core/native_widget_meta.hpp"
#include "core/open_url.hpp"
#include "core/text_style_flags.hpp"
#include "core/ui_core_internal.hpp"

#include <clay.h>

#include <deque>
#include <string>
#include <string_view>

namespace {

using namespace n8v::detail::ui_internal;

n8v_button_options pendingButtonOpts;
n8v_text_options pendingTextOpts;

std::deque<std::string> urlStorage;

void dispatchClick(Clay_ElementId /*elementId*/, Clay_PointerData pointerData, void *userData) {
  if (pointerData.state != CLAY_POINTER_DATA_PRESSED_THIS_FRAME) return;
  auto *meta = static_cast<n8v::detail::NativeWidgetMeta *>(userData);
  if (meta && meta->onClick) meta->onClick(meta->onClickUserdata);
}

void dispatchLinkClick(Clay_ElementId /*elementId*/, Clay_PointerData pointerData, void *userData) {
  if (pointerData.state != CLAY_POINTER_DATA_PRESSED_THIS_FRAME) return;
  auto *url = static_cast<std::string *>(userData);
  if (url) n8v::detail::openUrl(*url);
}

n8v::Color toTextColor(n8v_color c) {
  if (c.a == 0.0f) return {0, 0, 0, 255};
  return toColor(c);
}

} // namespace

namespace n8v::detail::ui_internal {

void resetLeafFrameState() { urlStorage.clear(); }

} // namespace n8v::detail::ui_internal

extern "C" {

void _n8v_set_button_opts(n8v_button_options opts) { pendingButtonOpts = opts; }

void _n8v_button_commit(const char *label) {
  n8v_button_options opts = pendingButtonOpts;
  std::string_view labelView = toView(label);
  std::string_view iconView = toView(opts.icon);
  bool hasIcon = !iconView.empty();

  Clay__OpenElement();

  const int ordinal = widgetOrdinal++;
  const bool hovered = Clay_Hovered();
  const bool pressed = hovered && n8v::activeBackend().pointerDown();
  const n8v::ButtonPaint paint = n8v::activePaint().button(toButtonStyle(opts.style), hovered, pressed);

  const n8v::detail::DecodedImage *iconImage = hasIcon ? n8v::detail::getOrDecodeIcon(iconView, paint.fontSize, paint.textColor) : nullptr;
  bool iconTrailing = opts.icon_position == N8V_ICON_POSITION_TRAILING;

  Clay_ElementDeclaration decl = {};
  decl.layout.padding = n8v::detail::toClay(paint.padding);
  decl.backgroundColor = n8v::detail::toClay(paint.background);
  if (iconImage) {
    decl.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
    decl.layout.childGap = paint.fontSize / 2;
    decl.layout.childAlignment.y = CLAY_ALIGN_Y_CENTER;
  }

  float radius = easeValue(animKey(ordinal, 0), paint.cornerRadius.topLeft, paint.transitionSeconds);
  decl.cornerRadius = {radius, radius, radius, radius};
  if (paint.transitionSeconds > 0.0f) {
    decl.transition.handler = Clay_EaseOut;
    decl.transition.duration = paint.transitionSeconds;
    decl.transition.properties = CLAY_TRANSITION_PROPERTY_BACKGROUND_COLOR;
  }

  Clay_Dimensions nativeSize = n8v::activeBackend().measureNativeChrome(n8v::NativeWidgetKind::Button, labelView, paint.fontSize, iconImage != nullptr);
  if (nativeSize.width > 0) decl.layout.sizing.width = CLAY_SIZING_FIXED(nativeSize.width);
  if (nativeSize.height > 0) {
    decl.layout.sizing.height = CLAY_SIZING_FIXED(nativeSize.height);
  }

  widgetMetaStorage.push_back(n8v::detail::NativeWidgetMeta{});
  n8v::detail::NativeWidgetMeta &meta = widgetMetaStorage.back();
  meta.kind = n8v::NativeWidgetKind::Button;
  meta.ordinal = ordinal;
  meta.onClick = opts.on_click;
  meta.onClickUserdata = opts.on_click_userdata;
  meta.image = iconImage;
  meta.iconName = hasIcon ? internCString(iconView) : nullptr;
  meta.iconVariant = toIconVariant(opts.icon_variant);
  meta.iconTrailing = iconTrailing;
  decl.userData = &meta;

  Clay__ConfigureOpenElement(decl);

  if (opts.on_click) {
    Clay_OnHover(dispatchClick, &meta);
  }

  bool drawIconAsClayChild = iconImage && !n8v::activeBackend().rendersNativeChrome();
  auto openIconChild = [&] {
    Clay__OpenElement();
    Clay_ElementDeclaration iconDecl = {};
    iconDecl.image.imageData = const_cast<n8v::detail::DecodedImage *>(iconImage);
    iconDecl.layout.sizing.width = CLAY_SIZING_FIXED((float)paint.fontSize);
    iconDecl.layout.sizing.height = CLAY_SIZING_FIXED((float)paint.fontSize);
    widgetMetaStorage.push_back(n8v::detail::NativeWidgetMeta{});
    n8v::detail::NativeWidgetMeta &iconMeta = widgetMetaStorage.back();
    iconMeta.kind = n8v::NativeWidgetKind::Icon;
    iconMeta.ordinal = widgetOrdinal++;
    iconMeta.image = iconImage;
    iconMeta.iconName = hasIcon ? internCString(iconView) : nullptr;
    iconMeta.iconTint = paint.textColor;
    iconDecl.userData = &iconMeta;
    Clay__ConfigureOpenElement(iconDecl);
    Clay__CloseElement();
  };

  if (drawIconAsClayChild && !iconTrailing) openIconChild();

  textStyleStorage.push_back(n8v::detail::TextStyleFlags{paint.font, false, false, false, true, ordinal});
  Clay_TextElementConfig textConfig = {};
  textConfig.textColor = n8v::detail::toClay(paint.textColor);
  textConfig.fontSize = paint.fontSize;
  textConfig.wrapMode = CLAY_TEXT_WRAP_NONE;
  textConfig.userData = &textStyleStorage.back();
  CLAY_TEXT(internString(labelView), textConfig);

  if (drawIconAsClayChild && iconTrailing) openIconChild();

  Clay__CloseElement();
}

void _n8v_set_text_opts(n8v_text_options opts) { pendingTextOpts = opts; }

void _n8v_text_commit(const char *label) {
  n8v_text_options opts = pendingTextOpts;
  std::string_view labelView = toView(label);
  std::string_view urlView = toView(opts.url);

  if (!urlView.empty()) {
    Clay__OpenElement();

    const int ordinal = widgetOrdinal++;
    if (Clay_Hovered()) pendingCursor = n8v::CursorKind::Pointer;

    urlStorage.emplace_back(urlView);
    widgetMetaStorage.push_back(n8v::detail::NativeWidgetMeta{});
    n8v::detail::NativeWidgetMeta &meta = widgetMetaStorage.back();
    meta.kind = n8v::NativeWidgetKind::Link;
    meta.ordinal = ordinal;
    meta.url = &urlStorage.back();

    Clay_ElementDeclaration decl = {};
    decl.backgroundColor = {255, 255, 255, 255};
    decl.userData = &meta;

    n8v::TextOptions textOptions{};
    textOptions.bold = opts.bold;
    textOptions.italic = opts.italic;
    textOptions.strikethrough = opts.strikethrough;
    textOptions.url = urlView;
    textOptions.color = toTextColor(opts.color);
    const n8v::TextPaint textPaint = n8v::activePaint().text(textOptions);

    Clay_Dimensions nativeSize = n8v::activeBackend().measureNativeChrome(n8v::NativeWidgetKind::Link, labelView, textPaint.fontSize);
    if (nativeSize.width > 0) decl.layout.sizing.width = CLAY_SIZING_FIXED(nativeSize.width);
    if (nativeSize.height > 0) decl.layout.sizing.height = CLAY_SIZING_FIXED(nativeSize.height);

    Clay__ConfigureOpenElement(decl);

    Clay_OnHover(dispatchLinkClick, meta.url);

    textStyleStorage.push_back(n8v::detail::TextStyleFlags{textPaint.font, opts.bold, opts.italic, true, true, ordinal, opts.strikethrough});
    Clay_TextElementConfig textConfig = {};
    textConfig.textColor = n8v::detail::toClay(textPaint.color);
    textConfig.fontSize = textPaint.fontSize;
    textConfig.wrapMode = CLAY_TEXT_WRAP_NONE;
    textConfig.userData = &textStyleStorage.back();
    CLAY_TEXT(internString(labelView), textConfig);

    Clay__CloseElement();
  } else {
    const int ordinal = widgetOrdinal++;
    n8v::TextOptions textOptions{};
    textOptions.bold = opts.bold;
    textOptions.italic = opts.italic;
    textOptions.strikethrough = opts.strikethrough;
    textOptions.color = toTextColor(opts.color);
    const n8v::TextPaint textPaint = n8v::activePaint().text(textOptions);
    textStyleStorage.push_back(n8v::detail::TextStyleFlags{textPaint.font, opts.bold, opts.italic, false, false, ordinal, opts.strikethrough});
    Clay_TextElementConfig textConfig = {};
    textConfig.textColor = n8v::detail::toClay(textPaint.color);
    textConfig.fontSize = textPaint.fontSize;
    textConfig.userData = &textStyleStorage.back();
    CLAY_TEXT(internString(labelView), textConfig);
  }
}

} // extern "C"
