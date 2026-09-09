#include <n8v/backend.hpp>
#include <n8v/style.hpp>
#include <n8v/ui.hpp>

#include "core/clay_convert.hpp"
#include "core/native_widget_meta.hpp"
#include "core/open_url.hpp"
#include "core/text_style_flags.hpp"
#include "core/ui_core_internal.hpp"

#include <clay.h>

#include <deque>
#include <functional>
#include <string>
#include <string_view>

namespace {

using namespace n8v::detail::ui_internal;

std::deque<std::function<void()>> clickCallbacks;
std::deque<std::string> urlStorage;

void dispatchClick(Clay_ElementId /*elementId*/, Clay_PointerData pointerData, void *userData) {
  if (pointerData.state != CLAY_POINTER_DATA_PRESSED_THIS_FRAME) return;
  auto *callback = static_cast<std::function<void()> *>(userData);
  if (callback && *callback) (*callback)();
}

void dispatchLinkClick(Clay_ElementId /*elementId*/, Clay_PointerData pointerData, void *userData) {
  if (pointerData.state != CLAY_POINTER_DATA_PRESSED_THIS_FRAME) return;
  auto *url = static_cast<std::string *>(userData);
  if (url) n8v::detail::openUrl(*url);
}

} // namespace

namespace n8v::detail::ui_internal {

void resetLeafFrameState() {
  clickCallbacks.clear();
  urlStorage.clear();
}

} // namespace n8v::detail::ui_internal

namespace n8v::detail {

void LeafBuilder::operator()(std::string_view label) && {
  if (isButton) {
    Clay__OpenElement();

    const int ordinal = widgetOrdinal++;
    const bool hovered = Clay_Hovered();
    // if (hovered) pendingCursor = CursorKind::Pointer;
    const bool pressed = hovered && activeBackend().pointerDown();
    const ButtonPaint paint = activePaint().button(buttonOptions.style, hovered, pressed);

    Clay_ElementDeclaration decl = {};
    decl.layout.padding = toClay(paint.padding);
    decl.backgroundColor = toClay(paint.background);

    float radius = easeValue(animKey(ordinal, 0), paint.cornerRadius.topLeft, paint.transitionSeconds);
    decl.cornerRadius = {radius, radius, radius, radius};
    if (paint.transitionSeconds > 0.0f) {
      decl.transition.handler = Clay_EaseOut;
      decl.transition.duration = paint.transitionSeconds;
      decl.transition.properties = CLAY_TRANSITION_PROPERTY_BACKGROUND_COLOR;
    }

    Clay_Dimensions nativeSize = activeBackend().measureNativeChrome(NativeWidgetKind::Button, label, paint.fontSize);
    if (nativeSize.width > 0 && nativeSize.height > 0) {
      decl.layout.sizing.width = CLAY_SIZING_FIXED(nativeSize.width);
      decl.layout.sizing.height = CLAY_SIZING_FIXED(nativeSize.height);
    }

    const bool hasOnClick = static_cast<bool>(buttonOptions.onClick);
    if (hasOnClick) {
      clickCallbacks.push_back(std::move(buttonOptions.onClick));
      widgetMetaStorage.push_back(NativeWidgetMeta{NativeWidgetKind::Button, ordinal, &clickCallbacks.back(), nullptr});
    } else {
      widgetMetaStorage.push_back(NativeWidgetMeta{NativeWidgetKind::Button, ordinal, nullptr, nullptr});
    }
    decl.userData = &widgetMetaStorage.back();

    Clay__ConfigureOpenElement(decl);

    if (hasOnClick) {
      Clay_OnHover(dispatchClick, widgetMetaStorage.back().onClick);
    }

    textStyleStorage.push_back(n8v::detail::TextStyleFlags{paint.font, false, false, false, true, ordinal});
    Clay_TextElementConfig textConfig = {};
    textConfig.textColor = toClay(paint.textColor);
    textConfig.fontSize = paint.fontSize;
    textConfig.wrapMode = CLAY_TEXT_WRAP_NONE;
    textConfig.userData = &textStyleStorage.back();
    CLAY_TEXT(internString(label), textConfig);

    Clay__CloseElement();
  } else if (!textOptions.url.empty()) {
    Clay__OpenElement();

    const int ordinal = widgetOrdinal++;
    if (Clay_Hovered()) pendingCursor = CursorKind::Pointer;

    urlStorage.emplace_back(textOptions.url);
    widgetMetaStorage.push_back(NativeWidgetMeta{NativeWidgetKind::Link, ordinal, nullptr, &urlStorage.back()});

    Clay_ElementDeclaration decl = {};
    decl.backgroundColor = {255, 255, 255, 255};
    decl.userData = &widgetMetaStorage.back();

    const TextPaint textPaint = activePaint().text(textOptions);

    Clay_Dimensions nativeSize = activeBackend().measureNativeChrome(NativeWidgetKind::Link, label, textPaint.fontSize);
    if (nativeSize.width > 0 && nativeSize.height > 0) {
      decl.layout.sizing.width = CLAY_SIZING_FIXED(nativeSize.width);
      decl.layout.sizing.height = CLAY_SIZING_FIXED(nativeSize.height);
    }

    Clay__ConfigureOpenElement(decl);

    Clay_OnHover(dispatchLinkClick, widgetMetaStorage.back().url);

    textStyleStorage.push_back(n8v::detail::TextStyleFlags{textPaint.font, textOptions.bold, textOptions.italic, true, true, ordinal});
    Clay_TextElementConfig textConfig = {};
    textConfig.textColor = toClay(textPaint.color);
    textConfig.fontSize = textPaint.fontSize;
    textConfig.wrapMode = CLAY_TEXT_WRAP_NONE;
    textConfig.userData = &textStyleStorage.back();
    CLAY_TEXT(internString(label), textConfig);

    Clay__CloseElement();
  } else {
    const int ordinal = widgetOrdinal++;
    const TextPaint textPaint = activePaint().text(textOptions);
    textStyleStorage.push_back(n8v::detail::TextStyleFlags{textPaint.font, textOptions.bold, textOptions.italic, false, false, ordinal});
    Clay_TextElementConfig textConfig = {};
    textConfig.textColor = toClay(textPaint.color);
    textConfig.fontSize = textPaint.fontSize;
    textConfig.userData = &textStyleStorage.back();
    CLAY_TEXT(internString(label), textConfig);
  }
}

} // namespace n8v::detail
