#include <n8v/backend.hpp>
#include <n8v/style.hpp>
#include <n8v/ui.hpp>

#include "core/clay_convert.hpp"
#include "core/native_widget_meta.hpp"
#include "core/open_url.hpp"
#include "core/text_style_flags.hpp"

#include <clay.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <deque>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace {

static bool initialized = false;
static std::vector<char> clayMemory;

static std::deque<std::string> textStorage;
static std::deque<std::string> urlStorage;
static std::deque<std::string> placeholderStorage;
static std::deque<std::function<void()>> clickCallbacks;
static std::deque<std::function<void(bool)>> changeCallbacks;
static std::deque<std::function<void(std::string_view)>> entryChangeCallbacks;
static std::deque<std::function<void(int)>> radioChangeCallbacks;
static std::deque<n8v::detail::TextStyleFlags> textStyleStorage;
static std::deque<n8v::detail::NativeWidgetMeta> widgetMetaStorage;

static float currentDelta = 0.0f;
static int widgetOrdinal = 0;
static n8v::CursorKind pendingCursor = n8v::CursorKind::Default;

struct RadiusAnimation {
  float start = 0.0f;
  float current = 0.0f;
  float target = 0.0f;
  float elapsed = 0.0f;
};
static std::unordered_map<int, RadiusAnimation> radiusAnimations;

float easeRadius(int key, float target, float duration) {
  RadiusAnimation &anim = radiusAnimations[key];
  if (anim.target != target) {
    anim.start = anim.current;
    anim.target = target;
    anim.elapsed = 0.0f;
  }
  if (duration <= 0.0f) {
    anim.current = target;
    return target;
  }
  anim.elapsed += currentDelta;
  float ratio = anim.elapsed / duration;
  if (ratio > 1.0f) ratio = 1.0f;
  float inverse = 1.0f - ratio;
  float lerp = 1.0f - inverse * inverse * inverse;
  anim.current = anim.start + (anim.target - anim.start) * lerp;
  return anim.current;
}

Clay_Dimensions measureText(Clay_StringSlice text, Clay_TextElementConfig *config, void * /*userData*/) {
  auto *flags = static_cast<n8v::detail::TextStyleFlags *>(config->userData);
  n8v::FontFamily family = flags ? flags->font : n8v::FontFamily::DejaVuSans;
  bool bold = flags && flags->bold;
  bool italic = flags && flags->italic;
  return n8v::activeBackend().measureText(std::string_view(text.chars, (size_t)text.length), family, config->fontSize, bold, italic);
}

void clayErrorHandler(Clay_ErrorData errorData) { std::fprintf(stderr, "[n8v] Clay error: %.*s\n", (int)errorData.errorText.length, errorData.errorText.chars); }

void ensureInitialized() {
  if (initialized) return;
  initialized = true;

  uint32_t minMemorySize = Clay_MinMemorySize();
  clayMemory.resize(minMemorySize);
  Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(minMemorySize, clayMemory.data());

  Clay_Initialize(arena, n8v::activeBackend().windowSize(), Clay_ErrorHandler{clayErrorHandler, nullptr});
  Clay_SetMeasureTextFunction(measureText, nullptr);
}

Clay_String internString(std::string_view text) {
  textStorage.emplace_back(text);
  const std::string &stored = textStorage.back();
  return Clay_String{false, (int32_t)stored.size(), stored.data()};
}

void dispatchClick(Clay_ElementId /*elementId*/, Clay_PointerData pointerData, void *userData) {
  if (pointerData.state != CLAY_POINTER_DATA_PRESSED_THIS_FRAME) return;
  auto *callback = static_cast<std::function<void()> *>(userData);
  if (callback && *callback) (*callback)();
}

float frameDelta() {
  using clock = std::chrono::steady_clock;
  static clock::time_point last;
  static bool started = false;
  clock::time_point now = clock::now();
  if (!started) {
    started = true;
    last = now;
    return 0.0f;
  }
  float delta = std::chrono::duration<float>(now - last).count();
  last = now;
  return std::min(delta, 0.1f);
}

void dispatchLinkClick(Clay_ElementId /*elementId*/, Clay_PointerData pointerData, void *userData) {
  if (pointerData.state != CLAY_POINTER_DATA_PRESSED_THIS_FRAME) return;
  auto *url = static_cast<std::string *>(userData);
  if (url) n8v::detail::openUrl(*url);
}

void dispatchCheckboxToggle(Clay_ElementId /*elementId*/, Clay_PointerData pointerData, void *userData) {
  if (pointerData.state != CLAY_POINTER_DATA_PRESSED_THIS_FRAME) return;
  auto *meta = static_cast<n8v::detail::NativeWidgetMeta *>(userData);
  if (!meta || !meta->checked) return;
  *meta->checked = !*meta->checked;
  if (meta->onChange && *meta->onChange) (*meta->onChange)(*meta->checked);
}

void dispatchRadioSelect(Clay_ElementId /*elementId*/, Clay_PointerData pointerData, void *userData) {
  if (pointerData.state != CLAY_POINTER_DATA_PRESSED_THIS_FRAME) return;
  auto *meta = static_cast<n8v::detail::NativeWidgetMeta *>(userData);
  if (!meta || !meta->radioSelected || *meta->radioSelected == meta->radioValue) return;
  *meta->radioSelected = meta->radioValue;
  if (meta->onRadioChange && *meta->onRadioChange) (*meta->onRadioChange)(meta->radioValue);
}

} // namespace

namespace n8v::detail {

void beginFrame() {
  ensureInitialized();
  currentDelta = frameDelta();
  widgetOrdinal = 0;
  pendingCursor = CursorKind::Default;
  textStorage.clear();
  urlStorage.clear();
  placeholderStorage.clear();
  clickCallbacks.clear();
  changeCallbacks.clear();
  entryChangeCallbacks.clear();
  radioChangeCallbacks.clear();
  textStyleStorage.clear();
  widgetMetaStorage.clear();

  Backend &backend = activeBackend();
  backend.beginFrame();
  Clay_SetLayoutDimensions(backend.windowSize());
  Clay_BeginLayout();
}

void endFrame() {
  Clay_RenderCommandArray commands = Clay_EndLayout(currentDelta);
  Backend &backend = activeBackend();
  backend.present(commands);
  backend.setCursor(pendingCursor);
}

void openFlex(const FlexOptions &options) {
  Clay__OpenElement();

  Clay_ElementDeclaration decl = {};
  decl.layout.layoutDirection = options.direction == Direction::Horizontal ? CLAY_LEFT_TO_RIGHT : CLAY_TOP_TO_BOTTOM;
  decl.layout.childGap = options.gap;
  decl.layout.padding = toClay(options.padding);
  decl.layout.childAlignment = {toClayX(options.hAlign), toClayY(options.vAlign)};
  decl.layout.sizing.width = toClay(options.width);
  decl.layout.sizing.height = toClay(options.height);

  Clay__ConfigureOpenElement(decl);
}

void closeFlex() { Clay__CloseElement(); }

void entry(const EntryOptions &options) {
  Clay__OpenElement();

  const int ordinal = widgetOrdinal++;
  if (Clay_Hovered()) pendingCursor = CursorKind::Text;
  const EntryPaint paint = activePaint().entry();

  bool hasValue = options.value && !options.value->empty();
  std::string maskedBuffer;
  std::string_view displayText;
  if (hasValue && options.password) {
    size_t count = 0;
    for (size_t i = 0; i < options.value->size();) {
      unsigned char c = (unsigned char)(*options.value)[i];
      i += (c & 0x80) == 0 ? 1 : (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : (c & 0xF8) == 0xF0 ? 4 : 1;
      ++count;
    }
    maskedBuffer.assign(count, '*');
    displayText = maskedBuffer;
  } else {
    displayText = hasValue ? std::string_view(*options.value) : options.placeholder;
  }

  Clay_ElementDeclaration decl = {};
  decl.layout.padding = toClay(paint.padding);
  decl.layout.sizing.width = CLAY_SIZING_GROW(0);
  decl.backgroundColor = toClay(paint.background);
  decl.cornerRadius = {paint.cornerRadius.topLeft, paint.cornerRadius.topRight, paint.cornerRadius.bottomLeft, paint.cornerRadius.bottomRight};

  Clay_Dimensions nativeSize = activeBackend().measureNativeChrome(NativeWidgetKind::Entry, options.placeholder, paint.fontSize);
  if (nativeSize.height > 0) {
    decl.layout.sizing.height = CLAY_SIZING_FIXED(nativeSize.height);
  }

  placeholderStorage.emplace_back(options.placeholder);
  const bool hasOnChange = static_cast<bool>(options.onChange);
  if (hasOnChange) entryChangeCallbacks.push_back(options.onChange);

  widgetMetaStorage.push_back(NativeWidgetMeta{});
  NativeWidgetMeta &meta = widgetMetaStorage.back();
  meta.kind = NativeWidgetKind::Entry;
  meta.ordinal = ordinal;
  meta.entryValue = options.value;
  meta.placeholder = &placeholderStorage.back();
  meta.password = options.password;
  meta.onEntryChange = hasOnChange ? &entryChangeCallbacks.back() : nullptr;
  decl.userData = &meta;

  Clay__ConfigureOpenElement(decl);

  textStyleStorage.push_back(n8v::detail::TextStyleFlags{paint.font, false, false, false, true, ordinal});
  Clay_TextElementConfig textConfig = {};
  textConfig.textColor = toClay(hasValue ? paint.textColor : paint.placeholderColor);
  textConfig.fontSize = paint.fontSize;
  textConfig.wrapMode = CLAY_TEXT_WRAP_NONE;
  textConfig.userData = &textStyleStorage.back();
  CLAY_TEXT(internString(displayText), textConfig);

  Clay__CloseElement();
}

void LeafBuilder::operator()(std::string_view label) && {
  if (isButton) {
    Clay__OpenElement();

    const int ordinal = widgetOrdinal++;
    const bool hovered = Clay_Hovered();
    if (hovered) pendingCursor = CursorKind::Pointer;
    const bool pressed = hovered && activeBackend().pointerDown();
    const ButtonPaint paint = activePaint().button(buttonOptions.style, hovered, pressed);

    Clay_ElementDeclaration decl = {};
    decl.layout.padding = toClay(paint.padding);
    decl.backgroundColor = toClay(paint.background);

    float radius = easeRadius(ordinal, paint.cornerRadius.topLeft, paint.transitionSeconds);
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

void CheckboxBuilder::operator()(std::string_view label) && {
  Clay__OpenElement();

  const int ordinal = widgetOrdinal++;
  const bool hovered = Clay_Hovered();
  if (hovered) pendingCursor = CursorKind::Pointer;
  const bool pressed = hovered && activeBackend().pointerDown();
  const bool checkedValue = options.checked && *options.checked;
  const CheckboxPaint paint = activePaint().checkbox(checkedValue, hovered, pressed);
  const TextPaint labelPaint = activePaint().text({});

  Clay_Dimensions labelDims = activeBackend().measureText(label, labelPaint.font, labelPaint.fontSize, false, false);
  float indicatorSize = labelDims.height;
  float indicatorGap = indicatorSize * 0.4f;

  Clay_ElementDeclaration decl = {};
  Padding pad = paint.padding;
  pad.left = (uint16_t)(indicatorSize + indicatorGap);
  decl.layout.padding = toClay(pad);
  decl.backgroundColor = toClay(paint.background);
  float radius = easeRadius(ordinal, paint.cornerRadius.topLeft, paint.transitionSeconds);
  decl.cornerRadius = {radius, radius, radius, radius};
  if (paint.transitionSeconds > 0.0f) {
    decl.transition.handler = Clay_EaseOut;
    decl.transition.duration = paint.transitionSeconds;
    decl.transition.properties = CLAY_TRANSITION_PROPERTY_BACKGROUND_COLOR;
  }

  Clay_Dimensions nativeSize = activeBackend().measureNativeChrome(NativeWidgetKind::Checkbox, label, labelPaint.fontSize);
  if (nativeSize.width > 0 && nativeSize.height > 0) {
    decl.layout.sizing.width = CLAY_SIZING_FIXED(nativeSize.width);
    decl.layout.sizing.height = CLAY_SIZING_FIXED(nativeSize.height);
  }

  const bool hasOnChange = static_cast<bool>(options.onChange);
  if (hasOnChange) {
    changeCallbacks.push_back(std::move(options.onChange));
    widgetMetaStorage.push_back(NativeWidgetMeta{NativeWidgetKind::Checkbox, ordinal, nullptr, nullptr, options.checked, &changeCallbacks.back()});
  } else {
    widgetMetaStorage.push_back(NativeWidgetMeta{NativeWidgetKind::Checkbox, ordinal, nullptr, nullptr, options.checked, nullptr});
  }
  decl.userData = &widgetMetaStorage.back();

  Clay__ConfigureOpenElement(decl);

  if (options.checked) {
    Clay_OnHover(dispatchCheckboxToggle, &widgetMetaStorage.back());
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

void RadioBuilder::operator()(std::string_view label) && {
  Clay__OpenElement();

  const int ordinal = widgetOrdinal++;
  const bool hovered = Clay_Hovered();
  if (hovered) pendingCursor = CursorKind::Pointer;
  const bool pressed = hovered && activeBackend().pointerDown();
  const bool selectedValue = options.selected && *options.selected == options.value;
  const RadioPaint paint = activePaint().radio(selectedValue, hovered, pressed);
  const TextPaint labelPaint = activePaint().text({});

  Clay_Dimensions labelDims = activeBackend().measureText(label, labelPaint.font, labelPaint.fontSize, false, false);
  float indicatorSize = labelDims.height;
  float indicatorGap = indicatorSize * 0.4f;

  Clay_ElementDeclaration decl = {};
  Padding pad = paint.padding;
  pad.left = (uint16_t)(indicatorSize + indicatorGap);
  decl.layout.padding = toClay(pad);
  decl.backgroundColor = toClay(paint.background);
  float radius = easeRadius(ordinal, indicatorSize / 2.0f, paint.transitionSeconds);
  decl.cornerRadius = {radius, radius, radius, radius};
  if (paint.transitionSeconds > 0.0f) {
    decl.transition.handler = Clay_EaseOut;
    decl.transition.duration = paint.transitionSeconds;
    decl.transition.properties = CLAY_TRANSITION_PROPERTY_BACKGROUND_COLOR;
  }

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
