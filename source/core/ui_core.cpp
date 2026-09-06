#include <n8v/backend.hpp>
#include <n8v/style.hpp>
#include <n8v/ui.hpp>

#include "core/clay_convert.hpp"
#include "core/open_url.hpp"
#include "core/text_style_flags.hpp"

#include <clay.h>

#include <cstdio>
#include <deque>
#include <functional>
#include <string>
#include <vector>

namespace {

static bool initialized = false;
static std::vector<char> clayMemory;

static std::deque<std::string> textStorage;
static std::deque<std::string> urlStorage;
static std::deque<std::function<void()>> clickCallbacks;
static std::deque<n8v::detail::TextStyleFlags> textStyleStorage;

Clay_Dimensions measureText(Clay_StringSlice text, Clay_TextElementConfig *config, void * /*userData*/) {
  auto *flags = static_cast<n8v::detail::TextStyleFlags *>(config->userData);
  bool bold = flags && flags->bold;
  bool italic = flags && flags->italic;
  return n8v::activeBackend().measureText(std::string_view(text.chars, (size_t)text.length), config->fontSize, bold, italic);
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

void dispatchClick(Clay_ElementId /*elementId*/, Clay_PointerData pointerData, intptr_t userData) {
  if (pointerData.state != CLAY_POINTER_DATA_PRESSED_THIS_FRAME) return;
  auto *callback = reinterpret_cast<std::function<void()> *>(userData);
  if (callback && *callback) (*callback)();
}

void dispatchLinkClick(Clay_ElementId /*elementId*/, Clay_PointerData pointerData, intptr_t userData) {
  if (pointerData.state != CLAY_POINTER_DATA_PRESSED_THIS_FRAME) return;
  auto *url = reinterpret_cast<std::string *>(userData);
  if (url) n8v::detail::openUrl(*url);
}

} // namespace

namespace n8v::detail {

void beginFrame() {
  ensureInitialized();
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
  Clay_RenderCommandArray commands = Clay_EndLayout();
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

    // Clay_Hovered() reads last frame's hit-test result for whatever element is
    // currently open - correct usage per clay.h's own doc comment, one-frame lag and all.
    const bool hovered = Clay_Hovered();
    const ButtonPaint paint = activePaint().button(buttonOptions.style, hovered);

    Clay_ElementDeclaration decl = {};
    decl.layout.padding = toClay(paint.padding);
    decl.backgroundColor = toClay(hovered ? paint.hoverBackground : paint.background);
    decl.cornerRadius = toClay(paint.cornerRadius);
    Clay__ConfigureOpenElement(decl);

    if (buttonOptions.onClick) {
      clickCallbacks.push_back(std::move(buttonOptions.onClick));
      Clay_OnHover(dispatchClick, reinterpret_cast<intptr_t>(&clickCallbacks.back()));
    }

    textStyleStorage.push_back(n8v::detail::TextStyleFlags{});
    Clay_TextElementConfig textConfig = {};
    textConfig.textColor = toClay(paint.textColor);
    textConfig.fontSize = 16;
    textConfig.userData = &textStyleStorage.back();
    CLAY_TEXT(internString(label), Clay__StoreTextElementConfig(textConfig));

    Clay__CloseElement();
  } else if (!textOptions.url.empty()) {
    Clay__OpenElement();
    Clay_ElementDeclaration decl = {};
    Clay__ConfigureOpenElement(decl);

    urlStorage.emplace_back(textOptions.url);
    Clay_OnHover(dispatchLinkClick, reinterpret_cast<intptr_t>(&urlStorage.back()));

    textStyleStorage.push_back(n8v::detail::TextStyleFlags{textOptions.bold, textOptions.italic, true});
    Clay_TextElementConfig textConfig = {};
    textConfig.textColor = toClay(activePaint().text(textOptions));
    textConfig.fontSize = 16;
    textConfig.userData = &textStyleStorage.back();
    CLAY_TEXT(internString(label), Clay__StoreTextElementConfig(textConfig));

    Clay__CloseElement();
  } else {
    textStyleStorage.push_back(n8v::detail::TextStyleFlags{textOptions.bold, textOptions.italic, false});
    Clay_TextElementConfig textConfig = {};
    textConfig.textColor = toClay(activePaint().text(textOptions));
    textConfig.fontSize = 16;
    textConfig.userData = &textStyleStorage.back();
    CLAY_TEXT(internString(label), Clay__StoreTextElementConfig(textConfig));
  }
}

} // namespace n8v::detail
