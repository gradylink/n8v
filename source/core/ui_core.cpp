#include <n8v/backend.hpp>
#include <n8v/style.hpp>
#include <n8v/ui.hpp>

#include "core/clay_convert.hpp"
#include "core/open_url.hpp"
#include "core/text_style_flags.hpp"

#include <clay.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <deque>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

static bool initialized = false;
static std::vector<char> clayMemory;

static std::deque<std::string> textStorage;
static std::deque<std::string> urlStorage;
static std::deque<std::function<void()>> clickCallbacks;
static std::deque<n8v::detail::TextStyleFlags> textStyleStorage;

static float currentDelta = 0.0f;
static int buttonOrdinal = 0;

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

} // namespace

namespace n8v::detail {

void beginFrame() {
  ensureInitialized();
  currentDelta = frameDelta();
  buttonOrdinal = 0;
  textStorage.clear();
  urlStorage.clear();
  clickCallbacks.clear();
  textStyleStorage.clear();

  Backend &backend = activeBackend();
  backend.beginFrame();
  Clay_SetLayoutDimensions(backend.windowSize());
  Clay_BeginLayout();
}

void endFrame() {
  Clay_RenderCommandArray commands = Clay_EndLayout(currentDelta);
  activeBackend().present(commands);
}

void openFlex(const FlexOptions &options) {
  Clay__OpenElement();

  Clay_ElementDeclaration decl = {};
  decl.layout.layoutDirection = options.direction == Direction::Horizontal ? CLAY_LEFT_TO_RIGHT : CLAY_TOP_TO_BOTTOM;
  decl.layout.childGap = options.gap;
  decl.layout.padding = toClay(options.padding);

  Clay__ConfigureOpenElement(decl);
}

void closeFlex() { Clay__CloseElement(); }

void LeafBuilder::operator()(std::string_view label) && {
  if (isButton) {
    Clay__OpenElement();

    const bool hovered = Clay_Hovered();
    const bool pressed = hovered && activeBackend().pointerDown();
    const ButtonPaint paint = activePaint().button(buttonOptions.style, hovered, pressed);

    Clay_ElementDeclaration decl = {};
    decl.layout.padding = toClay(paint.padding);
    decl.backgroundColor = toClay(paint.background);
    float radius = easeRadius(buttonOrdinal++, paint.cornerRadius.topLeft, paint.transitionSeconds);
    decl.cornerRadius = {radius, radius, radius, radius};
    if (paint.transitionSeconds > 0.0f) {
      decl.transition.handler = Clay_EaseOut;
      decl.transition.duration = paint.transitionSeconds;
      decl.transition.properties = CLAY_TRANSITION_PROPERTY_BACKGROUND_COLOR;
    }
    Clay__ConfigureOpenElement(decl);

    if (buttonOptions.onClick) {
      clickCallbacks.push_back(std::move(buttonOptions.onClick));
      Clay_OnHover(dispatchClick, &clickCallbacks.back());
    }

    textStyleStorage.push_back(n8v::detail::TextStyleFlags{paint.font, false, false, false});
    Clay_TextElementConfig textConfig = {};
    textConfig.textColor = toClay(paint.textColor);
    textConfig.fontSize = paint.fontSize;
    textConfig.userData = &textStyleStorage.back();
    CLAY_TEXT(internString(label), textConfig);

    Clay__CloseElement();
  } else if (!textOptions.url.empty()) {
    Clay__OpenElement();
    Clay_ElementDeclaration decl = {};
    Clay__ConfigureOpenElement(decl);

    urlStorage.emplace_back(textOptions.url);
    Clay_OnHover(dispatchLinkClick, &urlStorage.back());

    const TextPaint textPaint = activePaint().text(textOptions);
    textStyleStorage.push_back(n8v::detail::TextStyleFlags{textPaint.font, textOptions.bold, textOptions.italic, true});
    Clay_TextElementConfig textConfig = {};
    textConfig.textColor = toClay(textPaint.color);
    textConfig.fontSize = textPaint.fontSize;
    textConfig.userData = &textStyleStorage.back();
    CLAY_TEXT(internString(label), textConfig);

    Clay__CloseElement();
  } else {
    const TextPaint textPaint = activePaint().text(textOptions);
    textStyleStorage.push_back(n8v::detail::TextStyleFlags{textPaint.font, textOptions.bold, textOptions.italic, false});
    Clay_TextElementConfig textConfig = {};
    textConfig.textColor = toClay(textPaint.color);
    textConfig.fontSize = textPaint.fontSize;
    textConfig.userData = &textStyleStorage.back();
    CLAY_TEXT(internString(label), textConfig);
  }
}

} // namespace n8v::detail
