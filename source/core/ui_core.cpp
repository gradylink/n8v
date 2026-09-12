#include <n8v/n8v_c.h>

#include "core/backend.hpp"

#include "core/clay_convert.hpp"
#include "core/text_style_flags.hpp"
#include "core/ui_core_internal.hpp"

#include <clay.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <string_view>
#include <vector>

namespace {

using namespace n8v::detail::ui_internal;

bool initialized = false;
std::vector<char> clayMemory;

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

} // namespace

extern "C" {

void n8v_begin_frame(void) {
  ensureInitialized();
  currentDelta = frameDelta();
  widgetOrdinal = 0;
  pendingCursor = n8v::CursorKind::Default;

  n8v::Backend &backend = n8v::activeBackend();
  backend.beginFrame();

  textStorage.clear();
  textStyleStorage.clear();
  widgetMetaStorage.clear();
  resetLeafFrameState();
  resetCheckboxFrameState();
  resetRadioFrameState();
  resetEntryFrameState();
  resetDropdownFrameState();
  resetSliderFrameState();
  resetImageFrameState();

  Clay_SetLayoutDimensions(backend.windowSize());
  Clay_BeginLayout();
}

void n8v_end_frame(void) {
  Clay_RenderCommandArray commands = Clay_EndLayout(currentDelta);
  n8v::Backend &backend = n8v::activeBackend();
  backend.present(commands);
  backend.setCursor(pendingCursor);
}

void n8v_open_flex(n8v_flex_options options) {
  Clay__OpenElement();

  Clay_ElementDeclaration decl = {};
  decl.layout.layoutDirection = options.direction == N8V_DIRECTION_HORIZONTAL ? CLAY_LEFT_TO_RIGHT : CLAY_TOP_TO_BOTTOM;
  decl.layout.childGap = options.gap;
  decl.layout.padding = n8v::detail::toClay(toPadding(options.padding));
  decl.layout.childAlignment = {n8v::detail::toClayX(toAlign(options.h_align)), n8v::detail::toClayY(toAlign(options.v_align))};
  decl.layout.sizing.width = n8v::detail::toClay(toSizing(options.width));
  decl.layout.sizing.height = n8v::detail::toClay(toSizing(options.height));

  Clay__ConfigureOpenElement(decl);
}

void n8v_close_flex(void) { Clay__CloseElement(); }

} // extern "C"
