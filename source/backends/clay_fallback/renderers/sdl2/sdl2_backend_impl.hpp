#pragma once

#include "core/native_widget_meta.hpp"
#include "core/text_style_flags.hpp"

#include "core/backend.hpp"
#include <n8v/n8v_c.h>

#include <SDL2/SDL.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace n8v::detail {

struct FontGeneration;

class Sdl2Backend final : public Backend {
public:
  ~Sdl2Backend() override { shutdown(); }

  bool initialize(int width, int height, std::string_view title) override;
  bool pumpEvents() override;
  bool pointerDown() const override { return pointerDown_; }
  bool isEntryFocused(int ordinal) const override { return entry_.ordinal == ordinal; }
  Clay_Vector2 consumeScrollDelta() override {
    Clay_Vector2 delta = pendingScrollDelta_;
    pendingScrollDelta_ = {0, 0};
    return delta;
  }
  bool ownsScrollMath() const override { return true; }
  bool rendersNativeChrome() const override { return false; }
  Clay_Dimensions windowSize() const override;
  Clay_Dimensions measureText(std::string_view text, FontFamily family, uint16_t fontSize, bool bold, bool italic) const override;
  void beginFrame() override;
  void present(Clay_RenderCommandArray commands) override;
  void setCursor(CursorKind cursor) override;
  void shutdown() override;

private:
  struct EntryEditState {
    std::string *value = nullptr;
    n8v_string_buf *buf = nullptr;
    n8v_text_change_fn onChange = nullptr;
    void *onChangeUserdata = nullptr;
    int ordinal = -1;
    size_t cursor = 0;
    size_t anchor = 0;
    bool hasSelection() const { return anchor != cursor; }
    size_t selStart() const { return std::min(cursor, anchor); }
    size_t selEnd() const { return std::max(cursor, anchor); }
  };

  struct TextSelState {
    int ordinal = -1;
    std::string text;
    size_t cursor = 0;
    size_t anchor = 0;
    bool active = false;
    bool selectToEnd = false;
    bool hasSelection() const { return active && anchor != cursor; }
    size_t selStart() const { return std::min(cursor, anchor); }
    size_t selEnd() const { return std::max(cursor, anchor); }
  };

  bool blinkOn() const { return ((SDL_GetTicks() - lastActivityTicks_) / 500) % 2 == 0; }

  void fireEntryChange();
  void handleEntryClick(NativeWidgetMeta *meta, size_t hitOffset);
  void eraseEntrySelection();
  void insertAtCursor(std::string_view text);
  void moveEntryCursor(size_t newPos, bool extendSelection);
  void handleEntryKey(SDL_Keysym keysym);
  void renderEntryText(NativeWidgetMeta *meta, const Clay_RenderCommand &command, bool clicked, bool dragging);

  void handleTextClick(int ordinal, std::string_view lineText, size_t localHitOffset, size_t lineOffset);
  void moveTextCursor(size_t newPos, bool extendSelection);
  void handleTextKey(SDL_Keysym keysym);
  bool renderSelectableText(const Clay_RenderCommand &command, const TextStyleFlags &flags, std::string_view text);

  void renderCheckboxOrRadioIndicator(NativeWidgetMeta &meta, const Clay_BoundingBox &labelBox, bool isRadio);
  void drawIconGlyph(const Clay_BoundingBox &box, const char *iconName, const Clay_Color &color);
  void drawRadioDot(const Clay_BoundingBox &box, const Clay_Color &color, float scale);
  void drawDropdownChevron(const Clay_BoundingBox &box, const Clay_Color &color, bool pointsUp);
  void renderSwitchIndicator(NativeWidgetMeta &meta, const Clay_BoundingBox &labelBox);

  void drawImage(NativeWidgetMeta &meta, const Clay_BoundingBox &box, const Clay_CornerRadius &corner);

  size_t hitTestOffset(std::string_view text, FontFamily family, uint16_t fontSize, float localX, bool bold = false, bool italic = false) const;
  float caretPixelX(std::string_view text, FontFamily family, uint16_t fontSize, size_t byteOffset, bool bold = false, bool italic = false) const;
  void
  drawCursorCaret(const Clay_BoundingBox &textBox, std::string_view value, FontFamily family, uint16_t fontSize, size_t byteOffset, bool bold = false, bool italic = false);
  void drawSelectionHighlight(
    const Clay_BoundingBox &textBox, std::string_view value, FontFamily family, uint16_t fontSize, size_t selStart, size_t selEnd, bool bold = false, bool italic = false
  );
  void uploadAtlas(FontGeneration &gen);
  void drawText(const Clay_RenderCommand &command, size_t selStartByte = SIZE_MAX, size_t selEndByte = SIZE_MAX, SDL_Color selectedTint = SDL_Color{255, 255, 255, 255});
  void drawFocusRing(const Clay_BoundingBox &box, const Clay_CornerRadius &cornerRadius);
  void drawThickLine(float x0, float y0, float x1, float y1, float thickness, const Clay_Color &color);
  void drawStrokeCap(float cx, float cy, float thickness, const Clay_Color &color);
  void drawFilledRect(const Clay_BoundingBox &box, const Clay_Color &color);
  static void appendArc(std::vector<SDL_Vertex> &vertices, const SDL_Color &tint, float cx, float cy, float radius, float startAngle, float endAngle);
  static void appendArcN(std::vector<SDL_FPoint> &points, float cx, float cy, float radius, float startAngle, float endAngle, int segments);
  void drawRoundedRectBorder(const Clay_BoundingBox &box, const Clay_Color &color, const Clay_CornerRadius &corner, float strokeWidth);
  void drawRoundedRect(const Clay_BoundingBox &box, const Clay_Color &color, const Clay_CornerRadius &corner);

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
  bool textMouseSelecting_ = false;
  EntryEditState entry_;
  TextSelState textSel_;
  int textLineTrackOrdinal_ = -1;
  const char *textLineTrackBase_ = nullptr;
  Uint32 lastActivityTicks_ = 0;
  Uint32 lastClickTicks_ = 0;
  int lastClickOrdinal_ = -1;
  int clickCount_ = 0;
  bool debugModeChecked_ = false;
  std::unordered_map<const void *, SDL_Texture *> imageTextures_;
  Clay_Vector2 pendingScrollDelta_{0, 0};
  std::vector<SDL_Rect> clipStack_;
};

} // namespace n8v::detail
