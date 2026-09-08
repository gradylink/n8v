#include "sdl2_backend.hpp"

#include "backends/clay_fallback/text/line_layout.hpp"
#include "core/native_widget_meta.hpp"
#include "core/text_style_flags.hpp"

#include <SDL2/SDL.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace n8v::detail {
namespace {

size_t utf8CodepointLen(std::string_view s, size_t pos) {
  if (pos >= s.size()) return 0;
  unsigned char c = (unsigned char)s[pos];
  if ((c & 0x80) == 0) return 1;
  if ((c & 0xE0) == 0xC0) return 2;
  if ((c & 0xF0) == 0xE0) return 3;
  if ((c & 0xF8) == 0xF0) return 4;
  return 1;
}

size_t prevCodepointStart(std::string_view s, size_t pos) {
  if (pos == 0) return 0;
  size_t i = pos - 1;
  while (i > 0 && (static_cast<unsigned char>(s[i]) & 0xC0) == 0x80) --i;
  return i;
}

size_t nextCodepointStart(std::string_view s, size_t pos) { return pos >= s.size() ? s.size() : pos + utf8CodepointLen(s, pos); }

bool isWordByte(unsigned char c) { return std::isalnum(c) || c == '_' || c >= 0x80; }

size_t wordLeft(std::string_view s, size_t pos) {
  size_t i = pos;
  while (i > 0 && !isWordByte((unsigned char)s[prevCodepointStart(s, i)])) i = prevCodepointStart(s, i);
  while (i > 0 && isWordByte((unsigned char)s[prevCodepointStart(s, i)])) i = prevCodepointStart(s, i);
  return i;
}

size_t wordRight(std::string_view s, size_t pos) {
  size_t i = pos;
  while (i < s.size() && isWordByte((unsigned char)s[i])) i = nextCodepointStart(s, i);
  while (i < s.size() && !isWordByte((unsigned char)s[i])) i = nextCodepointStart(s, i);
  return i;
}

size_t wordStartAt(std::string_view s, size_t pos) {
  size_t i = pos;
  while (i > 0 && isWordByte((unsigned char)s[prevCodepointStart(s, i)])) i = prevCodepointStart(s, i);
  return i;
}

size_t wordEndAt(std::string_view s, size_t pos) {
  size_t i = pos;
  while (i < s.size() && isWordByte((unsigned char)s[i])) i = nextCodepointStart(s, i);
  return i;
}

std::vector<size_t> codepointByteOffsets(std::string_view s) {
  std::vector<size_t> offsets;
  size_t i = 0;
  while (i < s.size()) {
    offsets.push_back(i);
    i += utf8CodepointLen(s, i);
  }
  offsets.push_back(s.size());
  return offsets;
}

size_t realOffsetToDisplayOffset(std::string_view realValue, size_t realByteOffset, bool isPassword) {
  if (!isPassword) return realByteOffset;
  std::vector<size_t> offsets = codepointByteOffsets(realValue);
  for (size_t k = 0; k < offsets.size(); ++k) {
    if (offsets[k] == realByteOffset) return k;
  }
  return offsets.size() - 1;
}

size_t displayOffsetToRealOffset(std::string_view realValue, size_t displayByteOffset, bool isPassword) {
  if (!isPassword) return std::min(displayByteOffset, realValue.size());
  std::vector<size_t> offsets = codepointByteOffsets(realValue);
  size_t k = std::min(displayByteOffset, offsets.size() - 1);
  return offsets[k];
}

class Sdl2Backend final : public Backend {
public:
  ~Sdl2Backend() override { shutdown(); }

  bool initialize(int width, int height, std::string_view title) override {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
      std::fprintf(stderr, "[n8v] SDL_Init failed: %s\n", SDL_GetError());
      return false;
    }

    window_ = SDL_CreateWindow(std::string(title).c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!window_) {
      std::fprintf(stderr, "[n8v] SDL_CreateWindow failed: %s\n", SDL_GetError());
      return false;
    }

    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer_) {
      std::fprintf(stderr, "[n8v] SDL_CreateRenderer failed: %s\n", SDL_GetError());
      return false;
    }

    SDL_StartTextInput();

    return true;
  }

  bool pumpEvents() override {
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
        }
        break;
      case SDL_TEXTINPUT:
        if (entry_.value) insertAtCursor(event.text.text);
        break;
      case SDL_KEYDOWN:
        handleEntryKey(event.key.keysym);
        break;
      default:
        break;
      }
    }
    return true;
  }

  bool pointerDown() const override { return pointerDown_; }

  Clay_Dimensions windowSize() const override {
    int width = 0, height = 0;
    SDL_GetWindowSize(window_, &width, &height);
    return {(float)width, (float)height};
  }

  Clay_Dimensions measureText(std::string_view text, FontFamily family, uint16_t fontSize, bool bold, bool italic) const override {
    return measureLine(text, family, fontSize, bold, italic);
  }

  void beginFrame() override {
    Clay_SetPointerState({pointerX_, pointerY_}, pointerDown_);
    SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
    SDL_RenderClear(renderer_);
  }

  void present(Clay_RenderCommandArray commands) override {
    bool pendingIsCheckbox = false;
    Clay_Color pendingCheckboxColor{};
    Clay_CornerRadius pendingCheckboxRadius{};
    bool pendingChecked = false;

    NativeWidgetMeta *pendingEntryMeta = nullptr;
    bool pendingEntryClicked = false;
    bool pendingEntryDragging = false;

    for (int32_t i = 0; i < commands.length; ++i) {
      Clay_RenderCommand *command = Clay_RenderCommandArray_Get(&commands, i);
      switch (command->commandType) {
      case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
        auto *meta = static_cast<NativeWidgetMeta *>(command->userData);
        pendingIsCheckbox = meta && meta->kind == NativeWidgetKind::Checkbox;
        if (pendingIsCheckbox) {
          pendingCheckboxColor = command->renderData.rectangle.backgroundColor;
          pendingCheckboxRadius = command->renderData.rectangle.cornerRadius;
          pendingChecked = meta->checked && *meta->checked;
          break;
        }

        pendingEntryMeta = nullptr;
        pendingEntryClicked = false;
        pendingEntryDragging = false;
        if (meta && meta->kind == NativeWidgetKind::Entry) {
          pendingEntryMeta = meta;
          const Clay_BoundingBox &box = command->boundingBox;
          bool hit = pointerX_ >= box.x && pointerX_ <= box.x + box.width && pointerY_ >= box.y && pointerY_ <= box.y + box.height;
          if (justClicked_ && hit) {
            pendingEntryClicked = true;
          } else if (justClicked_ && entry_.ordinal == meta->ordinal) {
            entry_ = EntryEditState{};
          } else if (mouseSelecting_ && entry_.ordinal == meta->ordinal) {
            pendingEntryDragging = true;
          }
        }

        const Clay_Color &color = command->renderData.rectangle.backgroundColor;
        if (color.a > 0.0f) drawRoundedRect(command->boundingBox, color, command->renderData.rectangle.cornerRadius);

        if (meta && meta->kind == NativeWidgetKind::Entry && meta->ordinal == entry_.ordinal) {
          drawFocusRing(command->boundingBox);
        }
        break;
      }
      case CLAY_RENDER_COMMAND_TYPE_TEXT: {
        if (pendingIsCheckbox) {
          float squareSize = command->boundingBox.height;
          float gap = squareSize * 0.4f;
          Clay_BoundingBox squareBox{command->boundingBox.x - squareSize - gap, command->boundingBox.y, squareSize, squareSize};
          drawRoundedRect(squareBox, pendingCheckboxColor, pendingCheckboxRadius);
          if (pendingChecked) drawCheckmark(squareBox);
          pendingIsCheckbox = false;
        }

        if (pendingEntryMeta) {
          auto *flags = static_cast<TextStyleFlags *>(command->userData);
          uint16_t fontSize = command->renderData.text.fontSize;
          FontFamily family = flags ? flags->font : FontFamily::DejaVuSans;
          const std::string &value = *pendingEntryMeta->entryValue;
          bool isPassword = pendingEntryMeta->password;
          std::string_view displayText(command->renderData.text.stringContents.chars, (size_t)command->renderData.text.stringContents.length);

          bool hasValue = !value.empty();
          if (pendingEntryClicked) {
            size_t realHit = 0;
            if (hasValue) {
              float localX = pointerX_ - command->boundingBox.x;
              size_t displayHit = hitTestOffset(displayText, family, fontSize, localX);
              realHit = displayOffsetToRealOffset(value, displayHit, isPassword);
            }
            handleEntryClick(pendingEntryMeta, realHit);
          } else if (pendingEntryDragging) {
            if (hasValue) {
              float localX = pointerX_ - command->boundingBox.x;
              size_t displayHit = hitTestOffset(displayText, family, fontSize, localX);
              entry_.cursor = displayOffsetToRealOffset(value, displayHit, isPassword);
            } else {
              entry_.cursor = 0;
            }
            lastActivityTicks_ = SDL_GetTicks();
          }

          bool isFocused = pendingEntryMeta->ordinal == entry_.ordinal;
          bool hasSel = isFocused && entry_.hasSelection();
          size_t displaySelStart = realOffsetToDisplayOffset(value, entry_.selStart(), isPassword);
          size_t displaySelEnd = realOffsetToDisplayOffset(value, entry_.selEnd(), isPassword);
          if (hasSel) drawSelectionHighlight(command->boundingBox, displayText, family, fontSize, displaySelStart, displaySelEnd);
          if (hasSel) {
            drawText(*command, displaySelStart, displaySelEnd);
          } else {
            drawText(*command);
          }
          if (isFocused && blinkOn()) {
            size_t displayCursor = realOffsetToDisplayOffset(value, entry_.cursor, isPassword);
            drawCursorCaret(command->boundingBox, displayText, family, fontSize, displayCursor);
          }

          pendingEntryMeta = nullptr;
          pendingEntryClicked = false;
          pendingEntryDragging = false;
          break;
        }

        drawText(*command);
        break;
      }
      default:
        pendingIsCheckbox = false;
        pendingEntryMeta = nullptr;
        pendingEntryClicked = false;
        pendingEntryDragging = false;
        break;
      }
    }
    justClicked_ = false;
    SDL_RenderPresent(renderer_);
  }

  void setCursor(CursorKind cursor) override {
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

  void shutdown() override {
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

private:
  struct EntryEditState {
    std::string *value = nullptr;
    std::function<void(std::string_view)> onChange;
    int ordinal = -1;
    size_t cursor = 0;
    size_t anchor = 0;
    bool hasSelection() const { return anchor != cursor; }
    size_t selStart() const { return std::min(cursor, anchor); }
    size_t selEnd() const { return std::max(cursor, anchor); }
  };

  bool blinkOn() const { return ((SDL_GetTicks() - lastActivityTicks_) / 500) % 2 == 0; }

  void fireEntryChange() {
    if (entry_.value && entry_.onChange) entry_.onChange(*entry_.value);
  }

  void handleEntryClick(NativeWidgetMeta *meta, size_t hitOffset) {
    Uint32 now = SDL_GetTicks();
    if (meta->ordinal == lastClickOrdinal_ && (now - lastClickTicks_) < 400) {
      clickCount_ = (clickCount_ % 3) + 1;
    } else {
      clickCount_ = 1;
    }
    lastClickTicks_ = now;
    lastClickOrdinal_ = meta->ordinal;
    lastActivityTicks_ = now;

    entry_.value = meta->entryValue;
    entry_.onChange = meta->onEntryChange ? *meta->onEntryChange : std::function<void(std::string_view)>{};
    entry_.ordinal = meta->ordinal;

    const std::string &value = *meta->entryValue;
    if (clickCount_ == 2) {
      size_t probe = hitOffset < value.size() ? hitOffset : prevCodepointStart(value, hitOffset);
      if (probe < value.size() && isWordByte((unsigned char)value[probe])) {
        entry_.anchor = wordStartAt(value, hitOffset);
        entry_.cursor = wordEndAt(value, hitOffset);
      } else {
        entry_.cursor = entry_.anchor = hitOffset;
      }
      mouseSelecting_ = false;
    } else if (clickCount_ == 3) {
      entry_.anchor = 0;
      entry_.cursor = value.size();
      mouseSelecting_ = false;
    } else {
      entry_.cursor = entry_.anchor = hitOffset;
      mouseSelecting_ = true;
    }
  }

  void eraseEntrySelection() {
    if (!entry_.value || !entry_.hasSelection()) return;
    size_t s = entry_.selStart(), e = entry_.selEnd();
    entry_.value->erase(s, e - s);
    entry_.cursor = entry_.anchor = s;
  }

  void insertAtCursor(std::string_view text) {
    if (!entry_.value || text.empty()) return;
    lastActivityTicks_ = SDL_GetTicks();
    if (entry_.hasSelection()) eraseEntrySelection();
    entry_.value->insert(entry_.cursor, text);
    entry_.cursor += text.size();
    entry_.anchor = entry_.cursor;
    fireEntryChange();
  }

  void moveEntryCursor(size_t newPos, bool extendSelection) {
    entry_.cursor = newPos;
    if (!extendSelection) entry_.anchor = newPos;
  }

  void handleEntryKey(SDL_Keysym keysym) {
    if (!entry_.value) return;
    lastActivityTicks_ = SDL_GetTicks();
    std::string &s = *entry_.value;
    bool ctrl = (keysym.mod & KMOD_CTRL) != 0;
    bool shift = (keysym.mod & KMOD_SHIFT) != 0;

    switch (keysym.sym) {
    case SDLK_LEFT: {
      size_t target = (!shift && entry_.hasSelection()) ? entry_.selStart() : (ctrl ? wordLeft(s, entry_.cursor) : prevCodepointStart(s, entry_.cursor));
      moveEntryCursor(target, shift);
      break;
    }
    case SDLK_RIGHT: {
      size_t target = (!shift && entry_.hasSelection()) ? entry_.selEnd() : (ctrl ? wordRight(s, entry_.cursor) : nextCodepointStart(s, entry_.cursor));
      moveEntryCursor(target, shift);
      break;
    }
    case SDLK_HOME:
      moveEntryCursor(0, shift);
      break;
    case SDLK_END:
      moveEntryCursor(s.size(), shift);
      break;
    case SDLK_a:
      if (ctrl) {
        entry_.anchor = 0;
        entry_.cursor = s.size();
      }
      break;
    case SDLK_c:
    case SDLK_x:
      if (ctrl && entry_.hasSelection()) {
        std::string selected = s.substr(entry_.selStart(), entry_.selEnd() - entry_.selStart());
        SDL_SetClipboardText(selected.c_str());
        if (keysym.sym == SDLK_x) {
          eraseEntrySelection();
          fireEntryChange();
        }
      }
      break;
    case SDLK_v:
      if (ctrl) {
        char *clip = SDL_GetClipboardText();
        if (clip) {
          insertAtCursor(clip);
          SDL_free(clip);
        }
      }
      break;
    case SDLK_BACKSPACE:
      if (entry_.hasSelection()) {
        eraseEntrySelection();
      } else if (entry_.cursor > 0) {
        size_t start = prevCodepointStart(s, entry_.cursor);
        s.erase(start, entry_.cursor - start);
        entry_.cursor = entry_.anchor = start;
      } else {
        return;
      }
      fireEntryChange();
      break;
    case SDLK_DELETE:
      if (entry_.hasSelection()) {
        eraseEntrySelection();
      } else if (entry_.cursor < s.size()) {
        size_t end = nextCodepointStart(s, entry_.cursor);
        s.erase(entry_.cursor, end - entry_.cursor);
      } else {
        return;
      }
      fireEntryChange();
      break;
    default:
      break;
    }
  }

  size_t hitTestOffset(std::string_view text, FontFamily family, uint16_t fontSize, float localX) const {
    if (text.empty()) return 0;
    LineLayoutResult layout;
    if (!layoutLine(text, family, fontSize, false, false, layout)) return text.size();
    std::vector<size_t> offsets = codepointByteOffsets(text);
    size_t count = offsets.size() - 1;
    if (count == 0 || layout.caretX.size() != count) return text.size();

    float bestDist = std::abs(localX);
    size_t bestIndex = 0;
    for (size_t k = 1; k <= count; ++k) {
      float dist = std::abs(localX - layout.caretX[k - 1]);
      if (dist < bestDist) {
        bestDist = dist;
        bestIndex = k;
      }
    }
    return offsets[bestIndex];
  }

  float caretPixelX(std::string_view text, FontFamily family, uint16_t fontSize, size_t byteOffset) const {
    if (byteOffset == 0 || text.empty()) return 0.0f;
    LineLayoutResult layout;
    if (!layoutLine(text, family, fontSize, false, false, layout)) return 0.0f;
    std::vector<size_t> offsets = codepointByteOffsets(text);
    for (size_t k = 0; k < offsets.size(); ++k) {
      if (offsets[k] == byteOffset) return k == 0 ? 0.0f : layout.caretX[k - 1];
    }
    return layout.width;
  }

  void drawCursorCaret(const Clay_BoundingBox &textBox, std::string_view value, FontFamily family, uint16_t fontSize, size_t byteOffset) {
    float x = std::round(textBox.x + caretPixelX(value, family, fontSize, byteOffset));
    int y0 = (int)textBox.y, y1 = (int)(textBox.y + textBox.height);
    SDL_SetRenderDrawColor(renderer_, 20, 20, 20, 255);
    SDL_RenderDrawLine(renderer_, (int)x, y0, (int)x, y1);
  }

  void drawSelectionHighlight(const Clay_BoundingBox &textBox, std::string_view value, FontFamily family, uint16_t fontSize, size_t selStart, size_t selEnd) {
    float x0 = caretPixelX(value, family, fontSize, selStart);
    float x1 = caretPixelX(value, family, fontSize, selEnd);
    Clay_BoundingBox box{textBox.x + x0, textBox.y, x1 - x0, textBox.height};
    drawFilledRect(box, Clay_Color{50, 100, 220, 255});
  }

  void uploadAtlas(FontGeneration &gen) {
    std::vector<uint8_t> rgba((size_t)gen.atlasWidth * (size_t)gen.atlasHeight * 4);
    for (size_t i = 0; i < gen.pixels.size(); ++i) {
      rgba[i * 4 + 0] = 255;
      rgba[i * 4 + 1] = 255;
      rgba[i * 4 + 2] = 255;
      rgba[i * 4 + 3] = gen.pixels[i];
    }

    if (gen.backendHandle) {
      SDL_DestroyTexture((SDL_Texture *)gen.backendHandle);
      gen.backendHandle = nullptr;
    }

    SDL_Texture *texture = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, gen.atlasWidth, gen.atlasHeight);
    if (texture) {
      SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
      SDL_UpdateTexture(texture, nullptr, rgba.data(), gen.atlasWidth * 4);
    }

    gen.backendHandle = texture;
    gen.destroyBackendHandle = [](void *handle) { SDL_DestroyTexture((SDL_Texture *)handle); };
    gen.dirty = false;
  }

  void drawText(const Clay_RenderCommand &command, size_t selStartByte = SIZE_MAX, size_t selEndByte = SIZE_MAX, SDL_Color selectedTint = SDL_Color{255, 255, 255, 255}) {
    const Clay_TextRenderData &textData = command.renderData.text;
    auto *flags = static_cast<TextStyleFlags *>(command.userData);
    FontFamily family = flags ? flags->font : FontFamily::DejaVuSans;
    bool bold = flags && flags->bold;
    bool italic = flags && flags->italic;
    bool underline = flags && flags->underline;

    LineLayoutResult layout;
    std::string_view text(textData.stringContents.chars, (size_t)textData.stringContents.length);
    if (!layoutLine(text, family, textData.fontSize, bold, italic, layout) || !layout.generation) {
      Clay_Color block = textData.textColor;
      block.a = 160;
      drawFilledRect(command.boundingBox, block);
      return;
    }

    if (layout.generation->dirty) uploadAtlas(*layout.generation);
    auto *texture = (SDL_Texture *)layout.generation->backendHandle;
    if (!texture) return;

    SDL_Color tint{(Uint8)textData.textColor.r, (Uint8)textData.textColor.g, (Uint8)textData.textColor.b, (Uint8)textData.textColor.a};

    bool hasSelRange = selStartByte != SIZE_MAX && selEndByte > selStartByte;
    size_t selStartCp = SIZE_MAX, selEndCp = SIZE_MAX;
    if (hasSelRange) {
      std::vector<size_t> offsets = codepointByteOffsets(text);
      for (size_t k = 0; k < offsets.size(); ++k) {
        if (offsets[k] == selStartByte) selStartCp = k;
        if (offsets[k] == selEndByte) selEndCp = k;
      }
    }

    std::vector<SDL_Vertex> vertices;
    std::vector<int> indices;
    vertices.reserve(layout.quads.size() * 4);
    indices.reserve(layout.quads.size() * 6);

    float originX = std::round(command.boundingBox.x);
    float baselineY = std::round(command.boundingBox.y + layout.generation->ascent * layout.scale);

    for (const GlyphQuad &quad : layout.quads) {
      float x0 = originX + quad.x0 * layout.scale;
      float y0 = baselineY + quad.y0 * layout.scale;
      float x1 = originX + quad.x1 * layout.scale;
      float y1 = baselineY + quad.y1 * layout.scale;

      SDL_Color glyphTint = (hasSelRange && quad.codepointIndex >= selStartCp && quad.codepointIndex < selEndCp) ? selectedTint : tint;

      int base = (int)vertices.size();
      vertices.push_back({{x0, y0}, glyphTint, {quad.s0, quad.t0}});
      vertices.push_back({{x1, y0}, glyphTint, {quad.s1, quad.t0}});
      vertices.push_back({{x1, y1}, glyphTint, {quad.s1, quad.t1}});
      vertices.push_back({{x0, y1}, glyphTint, {quad.s0, quad.t1}});
      indices.insert(indices.end(), {base, base + 1, base + 2, base + 2, base + 3, base});
    }

    if (!vertices.empty()) {
      SDL_RenderGeometry(renderer_, texture, vertices.data(), (int)vertices.size(), indices.data(), (int)indices.size());
    }

    if (underline) {
      SDL_SetRenderDrawColor(renderer_, tint.r, tint.g, tint.b, tint.a);
      int underlineY = (int)std::round(command.boundingBox.y + command.boundingBox.height - 1.0f);
      SDL_RenderDrawLine(renderer_, (int)originX, underlineY, (int)std::round(originX + layout.width), underlineY);
    }
  }

  void drawFocusRing(const Clay_BoundingBox &box) {
    SDL_SetRenderDrawColor(renderer_, 60, 110, 220, 255);
    int x0 = (int)box.x, y0 = (int)box.y, x1 = (int)(box.x + box.width) - 1, y1 = (int)(box.y + box.height) - 1;
    SDL_RenderDrawLine(renderer_, x0, y0, x1, y0);
    SDL_RenderDrawLine(renderer_, x1, y0, x1, y1);
    SDL_RenderDrawLine(renderer_, x1, y1, x0, y1);
    SDL_RenderDrawLine(renderer_, x0, y1, x0, y0);
  }

  void drawCheckmark(const Clay_BoundingBox &box) {
    SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
    float pad = box.width * 0.22f;
    float x0 = box.x + pad, y0 = box.y + box.height * 0.55f;
    float xm = box.x + box.width * 0.42f, ym = box.y + box.height - pad;
    float x1 = box.x + box.width - pad, y1 = box.y + pad * 0.7f;
    SDL_RenderDrawLine(renderer_, (int)x0, (int)y0, (int)xm, (int)ym);
    SDL_RenderDrawLine(renderer_, (int)xm, (int)ym, (int)x1, (int)y1);
  }

  void drawFilledRect(const Clay_BoundingBox &box, const Clay_Color &color) {
    SDL_SetRenderDrawColor(renderer_, (Uint8)color.r, (Uint8)color.g, (Uint8)color.b, (Uint8)color.a);
    SDL_Rect rect{(int)box.x, (int)box.y, (int)box.width, (int)box.height};
    SDL_RenderFillRect(renderer_, &rect);
  }

  static void appendArc(std::vector<SDL_Vertex> &vertices, const SDL_Color &tint, float cx, float cy, float radius, float startAngle, float endAngle) {
    int segments = std::clamp((int)(radius * 0.6f) + 2, 3, 20);
    for (int i = 0; i <= segments; ++i) {
      float angle = startAngle + (endAngle - startAngle) * ((float)i / (float)segments);
      vertices.push_back({{cx + std::cos(angle) * radius, cy + std::sin(angle) * radius}, tint, {0, 0}});
    }
  }

  void drawRoundedRect(const Clay_BoundingBox &box, const Clay_Color &color, const Clay_CornerRadius &corner) {
    float limit = std::min(box.width, box.height) * 0.5f;
    float tl = std::clamp(corner.topLeft, 0.0f, limit);
    float tr = std::clamp(corner.topRight, 0.0f, limit);
    float br = std::clamp(corner.bottomRight, 0.0f, limit);
    float bl = std::clamp(corner.bottomLeft, 0.0f, limit);

    if (tl <= 0.5f && tr <= 0.5f && br <= 0.5f && bl <= 0.5f) {
      drawFilledRect(box, color);
      return;
    }

    const float pi = 3.14159265358979323846f;
    SDL_Color tint{(Uint8)color.r, (Uint8)color.g, (Uint8)color.b, (Uint8)color.a};

    std::vector<SDL_Vertex> vertices;
    vertices.push_back({{box.x + box.width * 0.5f, box.y + box.height * 0.5f}, tint, {0, 0}});

    appendArc(vertices, tint, box.x + tl, box.y + tl, tl, pi, pi * 1.5f);
    appendArc(vertices, tint, box.x + box.width - tr, box.y + tr, tr, pi * 1.5f, pi * 2.0f);
    appendArc(vertices, tint, box.x + box.width - br, box.y + box.height - br, br, 0.0f, pi * 0.5f);
    appendArc(vertices, tint, box.x + bl, box.y + box.height - bl, bl, pi * 0.5f, pi);

    int perimeter = (int)vertices.size() - 1;
    std::vector<int> indices;
    indices.reserve((size_t)perimeter * 3);
    for (int i = 0; i < perimeter; ++i) {
      indices.push_back(0);
      indices.push_back(1 + i);
      indices.push_back(1 + (i + 1) % perimeter);
    }

    SDL_RenderGeometry(renderer_, nullptr, vertices.data(), (int)vertices.size(), indices.data(), (int)indices.size());
  }

  SDL_Window *window_ = nullptr;
  SDL_Renderer *renderer_ = nullptr;
  float pointerX_ = 0.0f, pointerY_ = 0.0f;
  bool pointerDown_ = false;
  SDL_Cursor *defaultCursor_ = nullptr;
  SDL_Cursor *pointerCursor_ = nullptr;
  SDL_Cursor *textCursor_ = nullptr;
  CursorKind currentCursorKind_ = CursorKind::Default;

  bool justClicked_ = false;
  bool mouseSelecting_ = false;
  EntryEditState entry_;
  Uint32 lastActivityTicks_ = 0;
  Uint32 lastClickTicks_ = 0;
  int lastClickOrdinal_ = -1;
  int clickCount_ = 0;
};

} // namespace

std::unique_ptr<Backend> makeSdl2Backend() { return std::make_unique<Sdl2Backend>(); }

} // namespace n8v::detail
