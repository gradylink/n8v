#include <n8v/n8v_c.h>

#include "core/backend.hpp"
#include "core/style.hpp"

#include "core/clay_convert.hpp"
#include "core/native_widget_meta.hpp"
#include "core/text_style_flags.hpp"
#include "core/ui_core_internal.hpp"

#include <clay.h>

#include <cstdlib>
#include <cstring>
#include <deque>
#include <string>
#include <string_view>
#include <unordered_map>

namespace {

using namespace n8v::detail::ui_internal;

std::deque<std::string> placeholderStorage;

std::unordered_map<n8v_string_buf *, std::string> entryShadows;

} // namespace

namespace n8v::detail::ui_internal {

void resetEntryFrameState() { placeholderStorage.clear(); }

} // namespace n8v::detail::ui_internal

extern "C" {

void n8v_entry(n8v_entry_options options) {
  Clay__OpenElement();

  const int ordinal = widgetOrdinal++;
  if (Clay_Hovered()) pendingCursor = n8v::CursorKind::Text;

  std::string *shadow = nullptr;
  if (options.value) {
    n8v_string_buf &buf = *options.value;
    if (!buf.data) n8v_string_buf_init(&buf);

    std::string &s = entryShadows[&buf];
    std::string_view currentBufView(buf.data ? buf.data : "", buf.length);
    if (currentBufView != s) s.assign(currentBufView);
    shadow = &s;
  }

  std::string_view placeholderView = toView(options.placeholder);

  bool hasValue = shadow && !shadow->empty();
  const bool focused = n8v::activeBackend().isEntryFocused(ordinal);
  const n8v::EntryPaint paint = n8v::activePaint().entry(focused, hasValue);
  const bool floatingLabelStyle = paint.labelColor.a > 0.0f;
  const bool labelFloated = floatingLabelStyle && (hasValue || focused);

  std::string maskedBuffer;
  std::string_view displayText;
  if (hasValue && options.password) {
    size_t count = 0;
    for (size_t i = 0; i < shadow->size();) {
      unsigned char c = (unsigned char)(*shadow)[i];
      i += (c & 0x80) == 0 ? 1 : (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : (c & 0xF8) == 0xF0 ? 4 : 1;
      ++count;
    }
    maskedBuffer.assign(count, '*');
    displayText = maskedBuffer;
  } else if (hasValue) {
    displayText = *shadow;
  } else if (!floatingLabelStyle) {
    displayText = placeholderView;
  } else {
    displayText = " ";
  }

  Clay_ElementDeclaration decl = {};
  decl.layout.padding = n8v::detail::toClay(paint.padding);
  decl.layout.sizing.width = CLAY_SIZING_GROW(0);
  decl.backgroundColor = n8v::detail::toClay(paint.background);
  decl.cornerRadius = {paint.cornerRadius.topLeft, paint.cornerRadius.topRight, paint.cornerRadius.bottomLeft, paint.cornerRadius.bottomRight};
  if (paint.borderWidth > 0.0f) {
    decl.border.color = n8v::detail::toClay(paint.borderColor);
    uint16_t bw = (uint16_t)paint.borderWidth;
    decl.border.width = {bw, bw, bw, bw, 0};
  }

  Clay_Dimensions nativeSize = n8v::activeBackend().measureNativeChrome(n8v::NativeWidgetKind::Entry, placeholderView, paint.fontSize);
  float fieldHeight = (float)paint.padding.top + (float)paint.fontSize * 1.2f + (float)paint.padding.bottom;
  if (nativeSize.height > 0) {
    decl.layout.sizing.height = CLAY_SIZING_FIXED(nativeSize.height);
    fieldHeight = nativeSize.height;
  } else if (floatingLabelStyle) {
    decl.layout.sizing.height = CLAY_SIZING_FIXED(fieldHeight);
  }

  placeholderStorage.emplace_back(placeholderView);

  widgetMetaStorage.push_back(n8v::detail::NativeWidgetMeta{});
  n8v::detail::NativeWidgetMeta &meta = widgetMetaStorage.back();
  meta.kind = n8v::NativeWidgetKind::Entry;
  meta.ordinal = ordinal;
  meta.entryValue = shadow;
  meta.placeholder = &placeholderStorage.back();
  meta.password = options.password;
  meta.entryHasCustomBorder = paint.borderWidth > 0.0f;
  meta.entryBuf = options.value;
  meta.onEntryChange = options.on_change;
  meta.onEntryChangeUserdata = options.on_change_userdata;
  decl.userData = &meta;

  Clay__ConfigureOpenElement(decl);

  {
    textStyleStorage.push_back(n8v::detail::TextStyleFlags{paint.font, false, false, false, true, ordinal});
    Clay_TextElementConfig textConfig = {};
    textConfig.textColor = n8v::detail::toClay(hasValue ? paint.textColor : paint.placeholderColor);
    textConfig.fontSize = paint.fontSize;
    textConfig.wrapMode = CLAY_TEXT_WRAP_NONE;
    textConfig.userData = &textStyleStorage.back();
    CLAY_TEXT(internString(displayText), textConfig);
  }

  if (floatingLabelStyle && !placeholderView.empty()) {
    Clay__OpenElement();
    Clay_ElementDeclaration labelDecl = {};
    labelDecl.floating.attachTo = CLAY_ATTACH_TO_PARENT;
    labelDecl.floating.attachPoints = {CLAY_ATTACH_POINT_LEFT_TOP, CLAY_ATTACH_POINT_LEFT_TOP};
    labelDecl.floating.pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH;
    labelDecl.floating.zIndex = 10;
    if (paint.transitionSeconds > 0.0f) {
      labelDecl.transition.handler = Clay_EaseOut;
      labelDecl.transition.duration = paint.transitionSeconds;
      labelDecl.transition.properties = (Clay_TransitionProperty)(CLAY_TRANSITION_PROPERTY_BOUNDING_BOX | CLAY_TRANSITION_PROPERTY_BACKGROUND_COLOR);
    }
    uint16_t labelFontSize = (uint16_t)easeValue(animKey(ordinal, 32), labelFloated ? paint.labelFontSize : paint.fontSize, paint.transitionSeconds);
    Clay_Color labelBackdrop = n8v::detail::toClay(paint.background);
    labelBackdrop.a = labelFloated ? labelBackdrop.a : 0.0f;
    labelDecl.backgroundColor = labelBackdrop;
    if (labelFloated) {
      labelDecl.layout.padding = {4, 4, 0, 0};
      labelDecl.floating.offset = {(float)paint.padding.left - 4.0f, -(float)labelFontSize * 0.6f};
    } else {
      labelDecl.floating.offset = {(float)paint.padding.left, (fieldHeight - (float)labelFontSize) / 2.0f};
    }
    Clay__ConfigureOpenElement(labelDecl);

    textStyleStorage.push_back(n8v::detail::TextStyleFlags{paint.font, false, false, false, true, ordinal});
    Clay_TextElementConfig labelTextConfig = {};
    labelTextConfig.textColor = n8v::detail::toClay(paint.labelColor);
    labelTextConfig.fontSize = labelFontSize;
    labelTextConfig.wrapMode = CLAY_TEXT_WRAP_NONE;
    labelTextConfig.userData = &textStyleStorage.back();
    CLAY_TEXT(internString(placeholderView), labelTextConfig);

    Clay__CloseElement();
  }

  Clay__CloseElement();
}

} // extern "C"
