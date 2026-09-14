#pragma once

#include "screen_grid.hpp"

#include "core/backend.hpp"
#include "core/native_widget_meta.hpp"
#include "core/text_style_flags.hpp"

#include <n8v/n8v_c.h>

#include <ftxui/screen/color.hpp>

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <termios.h>
#include <unordered_map>
#include <vector>

namespace n8v::detail {

inline constexpr float cellPxW = 8.0f;
inline constexpr float cellPxH = 32.0f;

inline constexpr float indicatorIndent = 3.0f * cellPxW;
inline constexpr float switchIndent = 4.0f * cellPxW;

enum class TuiKeyCode { None, Left, Right, Home, End, Backspace, Delete, LetterA, LetterC, LetterV, LetterX };

struct TuiKeyEvent {
  TuiKeyCode code = TuiKeyCode::None;
  bool ctrl = false;
  bool shift = false;
};

struct KittyImageState {
  uint32_t id = 0;
  bool transmitted = false;
  bool visible = false;
};

struct FocusableWidget {
  int ordinal = -1;
  NativeWidgetKind kind = NativeWidgetKind::Button;
  CellRect box{};
};

class TuiBackend final : public Backend {
public:
  ~TuiBackend() override { shutdown(); }

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
  Clay_Dimensions measureNativeChrome(NativeWidgetKind kind, std::string_view text, uint16_t fontSize, bool hasIcon = false) const override;
  Clay_Dimensions cellSize() const override { return {cellPxW, cellPxH}; }
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
    bool hasSelection() const { return active && anchor != cursor; }
    size_t selStart() const { return std::min(cursor, anchor); }
    size_t selEnd() const { return std::max(cursor, anchor); }
  };

  int64_t nowMs() const;
  bool blinkOn() const { return (((nowMs() - lastActivityMs_) / 500) % 2) == 0; }

  void enterRawMode();
  void leaveRawMode();
  void pollTerminalSize();
  void readInput();
  void handleCsiSequence(std::string_view seq);
  void handleSgrMouse(std::string_view seq);
  void handleByte(unsigned char b, std::string_view rest, size_t &consumed);

  void focusNext(bool backward);
  void activateFocused();
  void releasePendingSyntheticClick();

  void fireEntryChange();
  void handleEntryClick(NativeWidgetMeta *meta, size_t hitOffset);
  void eraseEntrySelection();
  void insertAtCursor(std::string_view text);
  void moveEntryCursor(size_t newPos, bool extendSelection);
  void handleEntryKey(const TuiKeyEvent &key);
  void renderEntryText(NativeWidgetMeta *meta, const Clay_RenderCommand &command, bool clicked, bool dragging);

  void handleTextClick(int ordinal, std::string_view text, size_t hitOffset);
  void moveTextCursor(size_t newPos, bool extendSelection);
  void handleTextKey(const TuiKeyEvent &key);
  bool renderSelectableText(const Clay_RenderCommand &command, const TextStyleFlags &flags, std::string_view text);

  void renderCheckboxOrRadioIndicator(NativeWidgetMeta &meta, const Clay_BoundingBox &labelBox, bool isRadio);
  void drawDropdownChevron(const Clay_BoundingBox &box, const Clay_Color &color, bool pointsUp);
  void renderSwitchIndicator(NativeWidgetMeta &meta, const Clay_BoundingBox &labelBox);

  void drawImage(NativeWidgetMeta &meta, const Clay_BoundingBox &box, const Clay_CornerRadius &corner);
  bool drawIconGlyph(NativeWidgetMeta &meta, const Clay_BoundingBox &box);

  void renderSlider(NativeWidgetMeta &meta, const Clay_RenderCommand &command);

  void renderDropdownBox(NativeWidgetMeta &meta, const Clay_RenderCommand &command);
  void renderDropdownPopup(NativeWidgetMeta &meta, const CellRect &boxRect);
  void closeDropdownIfClickedOutside();
  bool openDropdownPopupOverlapsRows(int rowStart, int rowCount) const;
  void moveDropdownHighlight(int delta);
  void confirmDropdownHighlighted();

  CellRect cellRect(const Clay_BoundingBox &box) const;
  size_t hitTestOffset(std::string_view text, float localX) const;
  float caretPixelX(std::string_view text, size_t byteOffset) const;
  void drawCursorCaret(const Clay_BoundingBox &textBox, std::string_view value, size_t byteOffset);
  void drawSelectionHighlight(const Clay_BoundingBox &textBox, std::string_view value, size_t selStart, size_t selEnd);
  void drawText(const Clay_RenderCommand &command);
  void drawTextAt(int x, int y, std::string_view text, const Clay_Color &color, bool bold = false);
  void drawFocusRing(const Clay_BoundingBox &box, const Clay_CornerRadius &cornerRadius);
  void drawFilledRect(const Clay_BoundingBox &box, const Clay_Color &color);
  void drawRoundedRectBorder(const Clay_BoundingBox &box, const Clay_Color &color, const Clay_CornerRadius &corner, float strokeWidth);
  void drawRoundedRect(const Clay_BoundingBox &box, const Clay_Color &color, const Clay_CornerRadius &corner);
  void renderScrollbarIfNeeded(uint32_t elementId, const CellRect &viewport);

  ScreenGrid grid_;
  int cols_ = 80, rows_ = 24;
  termios savedTermios_{};
  bool rawModeActive_ = false;
  bool shouldQuit_ = false;
  bool sizeDirty_ = true;

  float pointerX_ = 0.0f, pointerY_ = 0.0f;
  bool pointerDown_ = false;
  bool justClicked_ = false;
  bool mouseSelecting_ = false;
  bool textMouseSelecting_ = false;
  CursorKind currentCursorKind_ = CursorKind::Default;

  EntryEditState entry_;
  TextSelState textSel_;
  std::string internalClipboard_;
  int64_t lastActivityMs_ = 0;
  int64_t lastClickMs_ = 0;
  int lastClickOrdinal_ = -1;
  int clickCount_ = 0;

  bool kittySupported_ = false;
  std::unordered_map<const void *, KittyImageState> kittyImages_;
  uint32_t nextKittyImageId_ = 1;
  std::string pendingImageEscapes_;

  int draggingSliderOrdinal_ = -1;
  int pendingSliderKeyStep_ = 0;

  int focusedOrdinal_ = -1;
  bool releaseSyntheticClickNextFrame_ = false;
  std::vector<FocusableWidget> focusables_;
  std::vector<FocusableWidget> pendingFocusables_;

  int openDropdownOrdinal_ = -1;
  CellRect openDropdownBoxRect_{};
  NativeWidgetMeta *openDropdownMeta_ = nullptr;
  int dropdownHighlightIndex_ = -1;

  bool hasScrollable_ = false;
  CellRect lastScrollableViewport_{};

  Clay_Vector2 pendingScrollDelta_{0, 0};
  std::vector<unsigned char> inputBuf_;
};

} // namespace n8v::detail
