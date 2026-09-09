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
static std::deque<std::vector<std::string>> dropdownItemsStorage;
static std::deque<std::function<void(int)>> dropdownChangeCallbacks;
static std::deque<std::function<void(float)>> sliderChangeCallbacks;
static std::deque<int> dropdownOrdinalStorage;

struct DropdownItemClick {
  n8v::detail::NativeWidgetMeta *meta;
  int index;
};
static std::deque<DropdownItemClick> dropdownItemClickStorage;

static std::unordered_map<int, bool> dropdownOpenState;

static int draggingSliderOrdinal = -1;

Clay_ElementId sliderTrackId(int ordinal) { return Clay__HashStringWithOffset(CLAY_STRING("n8v-slider-track"), (uint32_t)ordinal, 0); }

static float currentDelta = 0.0f;
static int widgetOrdinal = 0;
static n8v::CursorKind pendingCursor = n8v::CursorKind::Default;

struct ValueAnimation {
  float start = 0.0f;
  float current = 0.0f;
  float target = 0.0f;
  float elapsed = 0.0f;
};
static std::unordered_map<int, ValueAnimation> valueAnimations;

int animKey(int ordinal, int slot) { return ordinal * 64 + slot; }

float easeValue(int key, float target, float duration) {
  ValueAnimation &anim = valueAnimations[key];
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

n8v::Color easeColor(int ordinal, int slotBase, n8v::Color target, float duration) {
  return {
    easeValue(animKey(ordinal, slotBase + 0), target.r, duration),
    easeValue(animKey(ordinal, slotBase + 1), target.g, duration),
    easeValue(animKey(ordinal, slotBase + 2), target.b, duration),
    easeValue(animKey(ordinal, slotBase + 3), target.a, duration),
  };
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

void dispatchDropdownToggle(Clay_ElementId /*elementId*/, Clay_PointerData pointerData, void *userData) {
  if (pointerData.state != CLAY_POINTER_DATA_PRESSED_THIS_FRAME) return;
  auto *meta = static_cast<n8v::detail::NativeWidgetMeta *>(userData);
  if (!meta) return;
  bool &open = dropdownOpenState[meta->ordinal];
  open = !open;
}

void dispatchDropdownClose(Clay_ElementId /*elementId*/, Clay_PointerData pointerData, void *userData) {
  if (pointerData.state != CLAY_POINTER_DATA_PRESSED_THIS_FRAME) return;
  auto *ordinal = static_cast<int *>(userData);
  if (!ordinal) return;
  dropdownOpenState[*ordinal] = false;
}

void dispatchDropdownSelect(Clay_ElementId /*elementId*/, Clay_PointerData pointerData, void *userData) {
  if (pointerData.state != CLAY_POINTER_DATA_PRESSED_THIS_FRAME) return;
  auto *click = static_cast<DropdownItemClick *>(userData);
  if (!click || !click->meta) return;
  if (click->meta->dropdownSelected) *click->meta->dropdownSelected = click->index;
  if (click->meta->onDropdownChange && *click->meta->onDropdownChange) (*click->meta->onDropdownChange)(click->index);
  dropdownOpenState[click->meta->ordinal] = false;
}

void dispatchSliderDrag(Clay_ElementId elementId, Clay_PointerData pointerData, void *userData) {
  if (pointerData.state != CLAY_POINTER_DATA_PRESSED_THIS_FRAME) return;
  auto *meta = static_cast<n8v::detail::NativeWidgetMeta *>(userData);
  if (!meta || !meta->sliderValue) return;
  draggingSliderOrdinal = meta->ordinal;
  Clay_ElementData data = Clay_GetElementData(elementId);
  if (!data.found || data.boundingBox.width <= 0.0f) return;
  float fraction = (pointerData.position.x - data.boundingBox.x) / data.boundingBox.width;
  if (fraction < 0.0f) fraction = 0.0f;
  if (fraction > 1.0f) fraction = 1.0f;
  float newValue = meta->sliderMin + fraction * (meta->sliderMax - meta->sliderMin);
  if (newValue == *meta->sliderValue) return;
  *meta->sliderValue = newValue;
  if (meta->onSliderChange && *meta->onSliderChange) (*meta->onSliderChange)(newValue);
}

} // namespace

namespace n8v::detail {

void beginFrame() {
  ensureInitialized();
  currentDelta = frameDelta();
  widgetOrdinal = 0;
  pendingCursor = CursorKind::Default;

  Backend &backend = activeBackend();
  backend.beginFrame();

  textStorage.clear();
  urlStorage.clear();
  placeholderStorage.clear();
  clickCallbacks.clear();
  changeCallbacks.clear();
  entryChangeCallbacks.clear();
  radioChangeCallbacks.clear();
  textStyleStorage.clear();
  widgetMetaStorage.clear();
  dropdownItemsStorage.clear();
  dropdownChangeCallbacks.clear();
  sliderChangeCallbacks.clear();
  dropdownOrdinalStorage.clear();
  dropdownItemClickStorage.clear();

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

  bool hasValue = options.value && !options.value->empty();
  const bool focused = activeBackend().isEntryFocused(ordinal);
  const EntryPaint paint = activePaint().entry(focused, hasValue);
  const bool floatingLabelStyle = paint.labelColor.a > 0.0f;
  const bool labelFloated = floatingLabelStyle && (hasValue || focused);

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
  } else if (hasValue) {
    displayText = *options.value;
  } else if (!floatingLabelStyle) {
    displayText = options.placeholder;
  } else {
    displayText = " ";
  }

  Clay_ElementDeclaration decl = {};
  decl.layout.padding = toClay(paint.padding);
  decl.layout.sizing.width = CLAY_SIZING_GROW(0);
  decl.backgroundColor = toClay(paint.background);
  decl.cornerRadius = {paint.cornerRadius.topLeft, paint.cornerRadius.topRight, paint.cornerRadius.bottomLeft, paint.cornerRadius.bottomRight};
  if (paint.borderWidth > 0.0f) {
    decl.border.color = toClay(paint.borderColor);
    uint16_t bw = (uint16_t)paint.borderWidth;
    decl.border.width = {bw, bw, bw, bw, 0};
  }

  Clay_Dimensions nativeSize = activeBackend().measureNativeChrome(NativeWidgetKind::Entry, options.placeholder, paint.fontSize);
  float fieldHeight = (float)paint.padding.top + (float)paint.fontSize * 1.2f + (float)paint.padding.bottom;
  if (nativeSize.height > 0) {
    decl.layout.sizing.height = CLAY_SIZING_FIXED(nativeSize.height);
    fieldHeight = nativeSize.height;
  } else if (floatingLabelStyle) {
    decl.layout.sizing.height = CLAY_SIZING_FIXED(fieldHeight);
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
  meta.entryHasCustomBorder = paint.borderWidth > 0.0f;
  meta.onEntryChange = hasOnChange ? &entryChangeCallbacks.back() : nullptr;
  decl.userData = &meta;

  Clay__ConfigureOpenElement(decl);

  {
    textStyleStorage.push_back(n8v::detail::TextStyleFlags{paint.font, false, false, false, true, ordinal});
    Clay_TextElementConfig textConfig = {};
    textConfig.textColor = toClay(hasValue ? paint.textColor : paint.placeholderColor);
    textConfig.fontSize = paint.fontSize;
    textConfig.wrapMode = CLAY_TEXT_WRAP_NONE;
    textConfig.userData = &textStyleStorage.back();
    CLAY_TEXT(internString(displayText), textConfig);
  }

  if (floatingLabelStyle && !options.placeholder.empty()) {
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
    Clay_Color labelBackdrop = toClay(paint.background);
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
    labelTextConfig.textColor = toClay(paint.labelColor);
    labelTextConfig.fontSize = labelFontSize;
    labelTextConfig.wrapMode = CLAY_TEXT_WRAP_NONE;
    labelTextConfig.userData = &textStyleStorage.back();
    CLAY_TEXT(internString(options.placeholder), labelTextConfig);

    Clay__CloseElement();
  }

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
  float indicatorSize = paint.indicatorSize > 0.0f ? paint.indicatorSize : labelDims.height;
  float indicatorGap = indicatorSize * 0.4f;

  Clay_ElementDeclaration decl = {};
  Padding pad = paint.padding;
  pad.left = (uint16_t)(indicatorSize + indicatorGap);
  decl.layout.padding = toClay(pad);
  decl.backgroundColor = {0, 0, 0, 1};

  float radius = easeValue(animKey(ordinal, 0), paint.cornerRadius.topLeft, paint.transitionSeconds);

  Clay_Dimensions nativeSize = activeBackend().measureNativeChrome(NativeWidgetKind::Checkbox, label, labelPaint.fontSize);
  if (nativeSize.width > 0 && nativeSize.height > 0) {
    decl.layout.sizing.width = CLAY_SIZING_FIXED(nativeSize.width);
    decl.layout.sizing.height = CLAY_SIZING_FIXED(nativeSize.height);
  }

  const bool hasOnChange = static_cast<bool>(options.onChange);
  if (hasOnChange) changeCallbacks.push_back(std::move(options.onChange));

  widgetMetaStorage.push_back(NativeWidgetMeta{});
  NativeWidgetMeta &meta = widgetMetaStorage.back();
  meta.kind = NativeWidgetKind::Checkbox;
  meta.ordinal = ordinal;
  meta.checked = options.checked;
  meta.onChange = hasOnChange ? &changeCallbacks.back() : nullptr;
  meta.indicatorFillColor = easeColor(ordinal, 4, paint.background, paint.transitionSeconds);
  meta.indicatorBorderColor = easeColor(ordinal, 8, paint.borderColor, paint.transitionSeconds);
  meta.indicatorBorderWidth = easeValue(animKey(ordinal, 12), paint.borderWidth, paint.transitionSeconds);
  meta.indicatorGlyphColor = easeColor(ordinal, 16, paint.checkColor, paint.transitionSeconds);
  meta.indicatorCornerRadius = radius;
  meta.indicatorSize = indicatorSize;
  decl.userData = &meta;

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

void dropdown(const DropdownOptions &options) {
  Clay__OpenElement();

  const int ordinal = widgetOrdinal++;
  const bool hovered = Clay_Hovered();
  if (hovered) pendingCursor = CursorKind::Pointer;
  const bool pressed = hovered && activeBackend().pointerDown();
  const bool open = dropdownOpenState[ordinal];
  const bool hasSelection = options.selected && *options.selected >= 0 && (size_t)*options.selected < options.items.size();
  const DropdownPaint paint = activePaint().dropdown(open, hasSelection, hovered, pressed);

  const bool floatingLabelStyle = paint.labelColor.a > 0.0f;
  const bool labelFloated = floatingLabelStyle && (hasSelection || open);
  std::string_view selectedText = hasSelection ? options.items[(size_t)*options.selected] : std::string_view{};

  Clay_ElementDeclaration decl = {};
  decl.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
  decl.layout.sizing.width = CLAY_SIZING_GROW(0);
  decl.backgroundColor = toClay(paint.background);
  decl.cornerRadius = {paint.cornerRadius.topLeft, paint.cornerRadius.topRight, paint.cornerRadius.bottomLeft, paint.cornerRadius.bottomRight};
  if (!floatingLabelStyle) decl.layout.padding = toClay(paint.padding);

  Clay_Dimensions nativeSize = activeBackend().measureNativeChrome(NativeWidgetKind::Dropdown, hasSelection ? selectedText : options.placeholder, paint.fontSize);
  const bool hasNativeChrome = nativeSize.height > 0;
  float contentHeight = (float)paint.padding.top + (float)paint.labelFontSize * 1.2f + (float)paint.fontSize * 1.2f + (float)paint.padding.bottom;
  if (hasNativeChrome) {
    decl.layout.sizing.height = CLAY_SIZING_FIXED(nativeSize.height);
  } else if (floatingLabelStyle) {
    decl.layout.sizing.height = CLAY_SIZING_FIXED(contentHeight + paint.indicatorWidth);
  }

  dropdownItemsStorage.push_back({});
  std::vector<std::string> &itemsCopy = dropdownItemsStorage.back();
  itemsCopy.reserve(options.items.size());
  for (std::string_view item : options.items) itemsCopy.emplace_back(item);

  const bool hasOnChange = static_cast<bool>(options.onChange);
  if (hasOnChange) dropdownChangeCallbacks.push_back(options.onChange);

  widgetMetaStorage.push_back(NativeWidgetMeta{});
  NativeWidgetMeta &meta = widgetMetaStorage.back();
  meta.kind = NativeWidgetKind::Dropdown;
  meta.ordinal = ordinal;
  meta.dropdownItems = &itemsCopy;
  meta.dropdownSelected = options.selected;
  meta.onDropdownChange = hasOnChange ? &dropdownChangeCallbacks.back() : nullptr;
  decl.userData = &meta;

  Clay__ConfigureOpenElement(decl);

  if (!hasNativeChrome) {
    Clay_OnHover(dispatchDropdownToggle, &meta);
  }

  if (!hasNativeChrome && floatingLabelStyle) {
    Clay__OpenElement();
    Clay_ElementDeclaration contentRowDecl = {};
    contentRowDecl.layout.padding = toClay(paint.padding);
    contentRowDecl.layout.sizing.width = CLAY_SIZING_GROW(0);
    contentRowDecl.layout.sizing.height = CLAY_SIZING_GROW(0);
    contentRowDecl.layout.childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_BOTTOM};
    Clay__ConfigureOpenElement(contentRowDecl);

    std::string_view lineText = hasSelection ? selectedText : std::string_view{"\xC2\xA0"};
    textStyleStorage.push_back(n8v::detail::TextStyleFlags{paint.font, false, false, false, true, ordinal});
    Clay_TextElementConfig valueTextConfig = {};
    valueTextConfig.textColor = toClay(paint.textColor);
    valueTextConfig.fontSize = paint.fontSize;
    valueTextConfig.wrapMode = CLAY_TEXT_WRAP_NONE;
    valueTextConfig.userData = &textStyleStorage.back();
    CLAY_TEXT(internString(lineText), valueTextConfig);

    Clay__OpenElement();
    Clay_ElementDeclaration chevronDecl = {};
    chevronDecl.layout.sizing.width = CLAY_SIZING_FIXED(12.0f);
    chevronDecl.layout.sizing.height = CLAY_SIZING_FIXED(12.0f);
    chevronDecl.backgroundColor = {0, 0, 0, 1};
    widgetMetaStorage.push_back(NativeWidgetMeta{});
    NativeWidgetMeta &chevronMeta = widgetMetaStorage.back();
    chevronMeta.kind = NativeWidgetKind::DropdownChevron;
    chevronMeta.ordinal = ordinal;
    chevronMeta.chevronColor = paint.labelColor;
    chevronMeta.chevronPointsUp = open;
    chevronDecl.userData = &chevronMeta;
    Clay__ConfigureOpenElement(chevronDecl);
    Clay__CloseElement();

    Clay__CloseElement(); // contentRow

    if (!options.placeholder.empty()) {
      Clay__OpenElement();
      Clay_ElementDeclaration labelDecl = {};
      labelDecl.floating.attachTo = CLAY_ATTACH_TO_PARENT;
      labelDecl.floating.attachPoints = {CLAY_ATTACH_POINT_LEFT_TOP, CLAY_ATTACH_POINT_LEFT_TOP};
      labelDecl.floating.pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH;
      labelDecl.floating.zIndex = 10;
      if (paint.transitionSeconds > 0.0f) {
        labelDecl.transition.handler = Clay_EaseOut;
        labelDecl.transition.duration = paint.transitionSeconds;
        labelDecl.transition.properties = CLAY_TRANSITION_PROPERTY_BOUNDING_BOX;
      }
      uint16_t labelFontSize = (uint16_t)easeValue(animKey(ordinal, 32), labelFloated ? paint.labelFontSize : paint.fontSize, paint.transitionSeconds);
      if (labelFloated) {
        labelDecl.floating.offset = {(float)paint.padding.left, (float)paint.padding.top};
      } else {
        labelDecl.floating.offset = {(float)paint.padding.left, (contentHeight - (float)labelFontSize) / 2.0f};
      }
      Clay__ConfigureOpenElement(labelDecl);

      textStyleStorage.push_back(n8v::detail::TextStyleFlags{paint.font, false, false, false, true, ordinal});
      Clay_TextElementConfig labelTextConfig = {};
      labelTextConfig.textColor = toClay(paint.labelColor);
      labelTextConfig.fontSize = labelFontSize;
      labelTextConfig.wrapMode = CLAY_TEXT_WRAP_NONE;
      labelTextConfig.userData = &textStyleStorage.back();
      CLAY_TEXT(internString(options.placeholder), labelTextConfig);

      Clay__CloseElement();
    }

    if (paint.indicatorWidth > 0.0f) {
      Clay__OpenElement();
      Clay_ElementDeclaration indicatorDecl = {};
      indicatorDecl.layout.sizing.width = CLAY_SIZING_GROW(0);
      indicatorDecl.layout.sizing.height = CLAY_SIZING_FIXED(paint.indicatorWidth);
      indicatorDecl.backgroundColor = toClay(paint.indicatorColor);
      Clay__ConfigureOpenElement(indicatorDecl);
      Clay__CloseElement();
    }
  } else {
    textStyleStorage.push_back(n8v::detail::TextStyleFlags{paint.font, false, false, false, true, ordinal});
    Clay_TextElementConfig textConfig = {};
    textConfig.textColor = toClay(hasSelection ? paint.textColor : paint.placeholderColor);
    textConfig.fontSize = paint.fontSize;
    textConfig.wrapMode = CLAY_TEXT_WRAP_NONE;
    textConfig.userData = &textStyleStorage.back();
    CLAY_TEXT(internString(hasSelection ? selectedText : options.placeholder), textConfig);
  }

  if (!hasNativeChrome && open && !itemsCopy.empty()) {
    Clay__OpenElement();
    dropdownOrdinalStorage.push_back(ordinal);
    Clay_ElementDeclaration backdropDecl = {};
    Clay_Dimensions winSize = activeBackend().windowSize();
    backdropDecl.layout.sizing.width = CLAY_SIZING_FIXED(winSize.width);
    backdropDecl.layout.sizing.height = CLAY_SIZING_FIXED(winSize.height);
    backdropDecl.floating.attachTo = CLAY_ATTACH_TO_ROOT;
    backdropDecl.floating.zIndex = 900;
    Clay__ConfigureOpenElement(backdropDecl);
    Clay_OnHover(dispatchDropdownClose, &dropdownOrdinalStorage.back());
    Clay__CloseElement();

    Clay__OpenElement();
    Clay_ElementDeclaration popupDecl = {};
    popupDecl.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
    popupDecl.layout.sizing.width = CLAY_SIZING_GROW(0);
    popupDecl.backgroundColor = toClay(paint.popupBackground);
    Clay_CornerRadius popupRadius = floatingLabelStyle ? Clay_CornerRadius{0, 0, paint.cornerRadius.topLeft, paint.cornerRadius.topRight} : decl.cornerRadius;
    popupDecl.cornerRadius = popupRadius;
    popupDecl.floating.attachTo = CLAY_ATTACH_TO_PARENT;
    popupDecl.floating.attachPoints = {CLAY_ATTACH_POINT_LEFT_TOP, CLAY_ATTACH_POINT_LEFT_BOTTOM};
    if (!floatingLabelStyle) popupDecl.floating.offset.y = 6.0f;
    popupDecl.floating.zIndex = 1000;
    Clay__ConfigureOpenElement(popupDecl);

    for (size_t i = 0; i < itemsCopy.size(); ++i) {
      Clay__OpenElement();

      const bool itemHovered = Clay_Hovered();
      if (itemHovered) pendingCursor = CursorKind::Pointer;
      const bool itemSelected = hasSelection && (size_t)*options.selected == i;

      Clay_ElementDeclaration rowDecl = {};
      rowDecl.layout.padding = toClay(paint.padding);
      rowDecl.layout.sizing.width = CLAY_SIZING_GROW(0);
      Color rowBg = itemHovered ? paint.itemHoverBackground : (itemSelected && paint.itemSelectedBackground.a > 0.0f ? paint.itemSelectedBackground : paint.popupBackground);
      rowDecl.backgroundColor = toClay(rowBg);
      bool isFirstRow = i == 0, isLastRow = i == itemsCopy.size() - 1;
      rowDecl.cornerRadius = {
        isFirstRow ? popupRadius.topLeft : 0.0f,
        isFirstRow ? popupRadius.topRight : 0.0f,
        isLastRow ? popupRadius.bottomLeft : 0.0f,
        isLastRow ? popupRadius.bottomRight : 0.0f
      };
      Clay__ConfigureOpenElement(rowDecl);

      dropdownItemClickStorage.push_back(DropdownItemClick{&meta, (int)i});
      Clay_OnHover(dispatchDropdownSelect, &dropdownItemClickStorage.back());

      textStyleStorage.push_back(n8v::detail::TextStyleFlags{paint.font, false, false, false, true, ordinal});
      Clay_TextElementConfig itemTextConfig = {};
      itemTextConfig.textColor = toClay(paint.textColor);
      itemTextConfig.fontSize = paint.fontSize;
      itemTextConfig.wrapMode = CLAY_TEXT_WRAP_NONE;
      itemTextConfig.userData = &textStyleStorage.back();
      CLAY_TEXT(internString(itemsCopy[i]), itemTextConfig);

      Clay__CloseElement();
    }

    Clay__CloseElement(); // popup
  }

  Clay__CloseElement();
}

void slider(const SliderOptions &options) {
  const int ordinal = widgetOrdinal++;

  if (draggingSliderOrdinal == ordinal && options.value) {
    Clay_PointerData globalPointer = Clay_GetPointerState();
    if (globalPointer.state == CLAY_POINTER_DATA_PRESSED || globalPointer.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME) {
      Clay_ElementData data = Clay_GetElementData(sliderTrackId(ordinal));
      if (data.found && data.boundingBox.width > 0.0f) {
        float dragFraction = (globalPointer.position.x - data.boundingBox.x) / data.boundingBox.width;
        if (dragFraction < 0.0f) dragFraction = 0.0f;
        if (dragFraction > 1.0f) dragFraction = 1.0f;
        float newValue = options.min + dragFraction * (options.max - options.min);
        if (newValue != *options.value) {
          *options.value = newValue;
          if (options.onChange) options.onChange(newValue);
        }
      }
    } else {
      draggingSliderOrdinal = -1;
    }
  }

  Clay__OpenElementWithId(sliderTrackId(ordinal));

  const bool hovered = Clay_Hovered();
  if (hovered) pendingCursor = CursorKind::Pointer;
  const bool pressed = hovered && activeBackend().pointerDown();
  const SliderPaint paint = activePaint().slider(hovered, pressed);

  Clay_Dimensions nativeSize = activeBackend().measureNativeChrome(NativeWidgetKind::Slider, {}, 0);
  const bool hasNativeChrome = nativeSize.height > 0;

  float outerHeight = hasNativeChrome ? nativeSize.height : (paint.trackHeight > paint.thumbHeight ? paint.trackHeight : paint.thumbHeight);

  float range = options.max - options.min;
  float rawValue = options.value ? *options.value : options.min;
  if (rawValue < options.min) rawValue = options.min;
  if (rawValue > options.max) rawValue = options.max;
  float fraction = range > 0.0f ? (rawValue - options.min) / range : 0.0f;

  Clay_ElementDeclaration decl = {};
  decl.layout.sizing.width = CLAY_SIZING_GROW(0);
  decl.layout.sizing.height = CLAY_SIZING_FIXED(outerHeight);
  decl.layout.childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER};
  decl.backgroundColor = {0, 0, 0, 1};

  const bool hasOnChange = static_cast<bool>(options.onChange);
  if (hasOnChange) sliderChangeCallbacks.push_back(options.onChange);

  widgetMetaStorage.push_back(NativeWidgetMeta{});
  NativeWidgetMeta &meta = widgetMetaStorage.back();
  meta.kind = NativeWidgetKind::Slider;
  meta.ordinal = ordinal;
  meta.sliderValue = options.value;
  meta.sliderMin = options.min;
  meta.sliderMax = options.max;
  meta.onSliderChange = hasOnChange ? &sliderChangeCallbacks.back() : nullptr;
  decl.userData = &meta;

  Clay__ConfigureOpenElement(decl);

  if (!hasNativeChrome && options.value) {
    Clay_OnHover(dispatchSliderDrag, &meta);
  }

  if (!hasNativeChrome) {
    float trackRadius = paint.trackHeight / 2.0f;
    float thumbRadius = (paint.thumbWidth < paint.thumbHeight ? paint.thumbWidth : paint.thumbHeight) / 2.0f;

    if (paint.trackGap > 0.0f) {
      Clay__OpenElement();
      Clay_ElementDeclaration fillDecl = {};
      fillDecl.layout.sizing.width = CLAY_SIZING_PERCENT(fraction);
      fillDecl.layout.sizing.height = CLAY_SIZING_FIXED(paint.trackHeight);
      fillDecl.backgroundColor = toClay(paint.fillColor);
      fillDecl.cornerRadius = {trackRadius, 2.0f, trackRadius, 2.0f};
      Clay__ConfigureOpenElement(fillDecl);
      Clay__CloseElement();

      Clay__OpenElement();
      Clay_ElementDeclaration gapDecl = {};
      gapDecl.layout.sizing.width = CLAY_SIZING_FIXED(paint.trackGap * 2.0f);
      gapDecl.layout.sizing.height = CLAY_SIZING_GROW(0);
      gapDecl.layout.childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER};
      Clay__ConfigureOpenElement(gapDecl);

      Clay__OpenElement();
      Clay_ElementDeclaration thumbDecl = {};
      thumbDecl.layout.sizing.width = CLAY_SIZING_FIXED(paint.thumbWidth);
      thumbDecl.layout.sizing.height = CLAY_SIZING_FIXED(paint.thumbHeight);
      thumbDecl.backgroundColor = toClay(paint.thumbColor);
      thumbDecl.cornerRadius = {thumbRadius, thumbRadius, thumbRadius, thumbRadius};
      if (paint.thumbBorderWidth > 0.0f) {
        thumbDecl.border.color = toClay(paint.thumbBorderColor);
        uint16_t thumbBw = (uint16_t)paint.thumbBorderWidth;
        thumbDecl.border.width = {thumbBw, thumbBw, thumbBw, thumbBw, 0};
      }
      Clay__ConfigureOpenElement(thumbDecl);
      Clay__CloseElement(); // thumb

      Clay__CloseElement(); // gap

      Clay__OpenElement();
      Clay_ElementDeclaration trackDecl = {};
      trackDecl.layout.sizing.width = CLAY_SIZING_GROW(0);
      trackDecl.layout.sizing.height = CLAY_SIZING_FIXED(paint.trackHeight);
      trackDecl.backgroundColor = toClay(paint.trackColor);
      trackDecl.cornerRadius = {2.0f, trackRadius, 2.0f, trackRadius};
      Clay__ConfigureOpenElement(trackDecl);
      Clay__CloseElement();
    } else {
      Clay__OpenElement();
      Clay_ElementDeclaration barDecl = {};
      barDecl.layout.sizing.width = CLAY_SIZING_GROW(0);
      barDecl.layout.sizing.height = CLAY_SIZING_FIXED(paint.trackHeight);
      barDecl.layout.childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER};
      Clay__ConfigureOpenElement(barDecl);

      Clay__OpenElement();
      Clay_ElementDeclaration fillDecl = {};
      fillDecl.layout.sizing.width = CLAY_SIZING_PERCENT(fraction);
      fillDecl.layout.sizing.height = CLAY_SIZING_GROW(0);
      fillDecl.backgroundColor = toClay(paint.fillColor);
      fillDecl.cornerRadius = {trackRadius, trackRadius, trackRadius, trackRadius};
      Clay__ConfigureOpenElement(fillDecl);
      Clay__CloseElement();

      Clay__OpenElement();
      Clay_ElementDeclaration trackDecl = {};
      trackDecl.layout.sizing.width = CLAY_SIZING_GROW(0);
      trackDecl.layout.sizing.height = CLAY_SIZING_GROW(0);
      trackDecl.backgroundColor = toClay(paint.trackColor);
      trackDecl.cornerRadius = {trackRadius, trackRadius, trackRadius, trackRadius};
      Clay__ConfigureOpenElement(trackDecl);
      Clay__CloseElement();

      Clay__CloseElement(); // bar

      Clay_ElementData outerData = Clay_GetElementData(sliderTrackId(ordinal));
      float outerWidthPx = outerData.found ? outerData.boundingBox.width : 0.0f;
      float thumbHalfWidth = paint.thumbWidth / 2.0f;

      Clay__OpenElement();
      Clay_ElementDeclaration thumbWrapDecl = {};
      thumbWrapDecl.floating.attachTo = CLAY_ATTACH_TO_PARENT;
      thumbWrapDecl.floating.attachPoints = {CLAY_ATTACH_POINT_LEFT_CENTER, CLAY_ATTACH_POINT_LEFT_CENTER};
      thumbWrapDecl.floating.pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH;
      thumbWrapDecl.floating.offset = {fraction * outerWidthPx - thumbHalfWidth, 0.0f};
      Clay__ConfigureOpenElement(thumbWrapDecl);

      Clay__OpenElement();
      Clay_ElementDeclaration thumbDecl = {};
      thumbDecl.layout.sizing.width = CLAY_SIZING_FIXED(paint.thumbWidth);
      thumbDecl.layout.sizing.height = CLAY_SIZING_FIXED(paint.thumbHeight);
      thumbDecl.backgroundColor = toClay(paint.thumbColor);
      thumbDecl.cornerRadius = {thumbRadius, thumbRadius, thumbRadius, thumbRadius};
      if (paint.thumbBorderWidth > 0.0f) {
        thumbDecl.border.color = toClay(paint.thumbBorderColor);
        uint16_t thumbBw = (uint16_t)paint.thumbBorderWidth;
        thumbDecl.border.width = {thumbBw, thumbBw, thumbBw, thumbBw, 0};
      }
      Clay__ConfigureOpenElement(thumbDecl);
      Clay__CloseElement(); // thumb

      Clay__CloseElement(); // thumbWrap
    }
  }

  Clay__CloseElement(); // outer
}

} // namespace n8v::detail
