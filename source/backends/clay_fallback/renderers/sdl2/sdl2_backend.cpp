#include "sdl2_backend.hpp"
#include "sdl2_backend_impl.hpp"

#include "backends/clay_fallback/text/line_layout.hpp"
#include "core/native_widget_meta.hpp"
#include "core/text_style_flags.hpp"

#include <SDL2/SDL_video.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <string_view>

namespace n8v::detail {

namespace {
constexpr float WheelScrollScale = 3.0f;
} // namespace

bool Sdl2Backend::initialize(int width, int height, std::string_view title) {
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    std::fprintf(stderr, "[n8v] SDL_Init failed: %s\n", SDL_GetError());
    return false;
  }

  SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
  SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);
  window_ =
    SDL_CreateWindow(std::string(title).c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);
  if (!window_) {
    std::fprintf(stderr, "[n8v] SDL_CreateWindow failed: %s\n", SDL_GetError());
    return false;
  }

  renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!renderer_) {
    std::fprintf(stderr, "[n8v] SDL_CreateRenderer failed: %s\n", SDL_GetError());
    return false;
  }

  SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);

  SDL_StartTextInput();

  return true;
}

bool Sdl2Backend::pumpEvents() {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    switch (event.type) {
    case SDL_QUIT:
      return false;
    case SDL_MOUSEMOTION:
      pointerX_ = (float)event.motion.x;
      pointerY_ = (float)event.motion.y;
      break;
    case SDL_MOUSEBUTTONDOWN:
      if (event.button.button == SDL_BUTTON_LEFT) {
        pointerDown_ = true;
        justClicked_ = true;
      }
      break;
    case SDL_MOUSEBUTTONUP:
      if (event.button.button == SDL_BUTTON_LEFT) {
        pointerDown_ = false;
        mouseSelecting_ = false;
        textMouseSelecting_ = false;
      }
      break;
    case SDL_MOUSEWHEEL:
      pendingScrollDelta_.x += (float)event.wheel.x * WheelScrollScale;
      pendingScrollDelta_.y += (float)event.wheel.y * WheelScrollScale;
      break;
    case SDL_TEXTINPUT:
      if (entry_.value) insertAtCursor(event.text.text);
      break;
    case SDL_KEYDOWN:
      if (entry_.value) handleEntryKey(event.key.keysym);
      else if (textSel_.active) handleTextKey(event.key.keysym);
      break;
    default:
      break;
    }
  }
  return true;
}

Clay_Dimensions Sdl2Backend::windowSize() const {
  int width = 0, height = 0;
  SDL_GetWindowSize(window_, &width, &height);
  return {(float)width, (float)height};
}

Clay_Dimensions Sdl2Backend::measureText(std::string_view text, FontFamily family, uint16_t fontSize, bool bold, bool italic) const {
  return measureLine(text, family, fontSize, bold, italic);
}

void Sdl2Backend::beginFrame() {
  if (!debugModeChecked_) {
    debugModeChecked_ = true;
    const char *debug = std::getenv("N8V_DEBUG");
    if (debug && std::string_view(debug) == "1") Clay_SetDebugModeEnabled(true);
  }

  Clay_SetPointerState({pointerX_, pointerY_}, pointerDown_);
  SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
  SDL_RenderClear(renderer_);
}

void Sdl2Backend::present(Clay_RenderCommandArray commands) {
  clipStack_.clear();
  SDL_RenderSetClipRect(renderer_, nullptr);

  bool pendingIsCheckbox = false;
  bool pendingIsRadio = false;
  NativeWidgetMeta *pendingIndicatorMeta = nullptr;

  NativeWidgetMeta *pendingEntryMeta = nullptr;
  bool pendingEntryClicked = false;
  bool pendingEntryDragging = false;

  for (int32_t i = 0; i < commands.length; ++i) {
    Clay_RenderCommand *command = Clay_RenderCommandArray_Get(&commands, i);
    switch (command->commandType) {
    case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
      auto *meta = static_cast<NativeWidgetMeta *>(command->userData);
      pendingIsCheckbox = meta && meta->kind == NativeWidgetKind::Checkbox;
      pendingIsRadio = meta && meta->kind == NativeWidgetKind::Radio;
      if (pendingIsCheckbox || pendingIsRadio) {
        pendingIndicatorMeta = meta;
        break;
      }
      if (meta && meta->kind == NativeWidgetKind::DropdownChevron) {
        drawDropdownChevron(command->boundingBox, Clay_Color{meta->chevronColor.r, meta->chevronColor.g, meta->chevronColor.b, meta->chevronColor.a}, meta->chevronPointsUp);
        break;
      }

      pendingEntryMeta = nullptr;
      pendingEntryClicked = false;
      pendingEntryDragging = false;
      if (meta && meta->kind == NativeWidgetKind::Entry) {
        pendingEntryMeta = meta;
        bool hit = Clay_PointerOver(Clay_ElementId{command->id});
        if (justClicked_ && hit) {
          pendingEntryClicked = true;
          textSel_ = TextSelState{};
        } else if (justClicked_ && entry_.ordinal == meta->ordinal) {
          entry_ = EntryEditState{};
        } else if (mouseSelecting_ && entry_.ordinal == meta->ordinal) {
          pendingEntryDragging = true;
        }
      }

      const Clay_Color &color = command->renderData.rectangle.backgroundColor;
      if (color.a > 0.0f) drawRoundedRect(command->boundingBox, color, command->renderData.rectangle.cornerRadius);

      if (meta && meta->kind == NativeWidgetKind::Entry && meta->ordinal == entry_.ordinal && !meta->entryHasCustomBorder) {
        drawFocusRing(command->boundingBox, command->renderData.rectangle.cornerRadius);
      }
      break;
    }
    case CLAY_RENDER_COMMAND_TYPE_TEXT: {
      if ((pendingIsCheckbox || pendingIsRadio) && pendingIndicatorMeta) {
        renderCheckboxOrRadioIndicator(*pendingIndicatorMeta, command->boundingBox, pendingIsRadio);
        pendingIsCheckbox = false;
        pendingIsRadio = false;
        pendingIndicatorMeta = nullptr;
      }

      if (pendingEntryMeta) {
        renderEntryText(pendingEntryMeta, *command, pendingEntryClicked, pendingEntryDragging);
        pendingEntryMeta = nullptr;
        pendingEntryClicked = false;
        pendingEntryDragging = false;
        break;
      }

      auto *flags = static_cast<TextStyleFlags *>(command->userData);
      std::string_view text(command->renderData.text.stringContents.chars, (size_t)command->renderData.text.stringContents.length);
      if (flags && !flags->ownedByWidget && !text.empty()) {
        if (renderSelectableText(*command, *flags, text)) break;
      }

      drawText(*command);
      break;
    }
    case CLAY_RENDER_COMMAND_TYPE_IMAGE: {
      auto *meta = static_cast<NativeWidgetMeta *>(command->userData);
      if (meta) drawImage(*meta, command->boundingBox, command->renderData.image.cornerRadius);
      break;
    }
    case CLAY_RENDER_COMMAND_TYPE_BORDER: {
      const Clay_BorderRenderData &border = command->renderData.border;
      float width = (float)border.width.left;
      if (width > 0.0f && border.color.a > 0.0f) {
        drawRoundedRectBorder(command->boundingBox, border.color, border.cornerRadius, width);
      }
      break;
    }
    case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START: {
      const Clay_BoundingBox &box = command->boundingBox;
      SDL_Rect rect{(int)box.x, (int)box.y, (int)box.width, (int)box.height};
      if (!clipStack_.empty()) SDL_IntersectRect(&rect, &clipStack_.back(), &rect);
      clipStack_.push_back(rect);
      SDL_RenderSetClipRect(renderer_, &rect);
      break;
    }
    case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END:
      if (!clipStack_.empty()) clipStack_.pop_back();
      SDL_RenderSetClipRect(renderer_, clipStack_.empty() ? nullptr : &clipStack_.back());
      break;
    default:
      pendingIsCheckbox = false;
      pendingIsRadio = false;
      pendingEntryMeta = nullptr;
      pendingEntryClicked = false;
      pendingEntryDragging = false;
      break;
    }
  }
  justClicked_ = false;
  SDL_RenderPresent(renderer_);
}

void Sdl2Backend::setCursor(CursorKind cursor) {
  if (cursor == currentCursorKind_) return;
  currentCursorKind_ = cursor;

  SDL_Cursor **handle = &defaultCursor_;
  SDL_SystemCursor sys = SDL_SYSTEM_CURSOR_ARROW;
  if (cursor == CursorKind::Pointer) {
    handle = &pointerCursor_;
    sys = SDL_SYSTEM_CURSOR_HAND;
  } else if (cursor == CursorKind::Text) {
    handle = &textCursor_;
    sys = SDL_SYSTEM_CURSOR_IBEAM;
  }
  if (!*handle) *handle = SDL_CreateSystemCursor(sys);
  if (*handle) SDL_SetCursor(*handle);
}

void Sdl2Backend::shutdown() {
  for (auto &[source, texture] : imageTextures_) SDL_DestroyTexture(texture);
  imageTextures_.clear();
  if (pointerCursor_) {
    SDL_FreeCursor(pointerCursor_);
    pointerCursor_ = nullptr;
  }
  if (defaultCursor_) {
    SDL_FreeCursor(defaultCursor_);
    defaultCursor_ = nullptr;
  }
  if (textCursor_) {
    SDL_FreeCursor(textCursor_);
    textCursor_ = nullptr;
  }
  if (renderer_) {
    SDL_DestroyRenderer(renderer_);
    renderer_ = nullptr;
  }
  if (window_) {
    SDL_DestroyWindow(window_);
    window_ = nullptr;
  }
  SDL_Quit();
}

std::unique_ptr<Backend> makeSdl2Backend() { return std::make_unique<Sdl2Backend>(); }

} // namespace n8v::detail
