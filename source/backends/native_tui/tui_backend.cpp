#include "tui_backend.hpp"
#include "tui_backend_impl.hpp"

#include "backends/text_edit_utils.hpp"
#include "core/native_widget_meta.hpp"
#include "core/text_style_flags.hpp"

#include <ftxui/screen/string.hpp>
#include <ftxui/screen/terminal.hpp>

#include <poll.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <vector>

namespace n8v::detail {

bool tuiBackendLikelyUsable() { return ::isatty(STDOUT_FILENO) && ::isatty(STDIN_FILENO); }

namespace {

bool detectKittySupport() {
  if (const char *id = std::getenv("KITTY_WINDOW_ID"); id && *id) return true;
  if (const char *term = std::getenv("TERM"); term && std::strstr(term, "kitty")) return true;
  if (const char *prog = std::getenv("TERM_PROGRAM")) {
    if (std::strcmp(prog, "WezTerm") == 0 || std::strcmp(prog, "ghostty") == 0) return true;
  }
  return false;
}

} // namespace

int64_t TuiBackend::nowMs() const { return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

void TuiBackend::enterRawMode() {
  termios raw{};
  if (::tcgetattr(STDIN_FILENO, &savedTermios_) != 0) return;
  raw = savedTermios_;
  raw.c_lflag &= ~(unsigned)(ECHO | ICANON | ISIG | IEXTEN);
  raw.c_iflag &= ~(unsigned)(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
  raw.c_oflag &= ~(unsigned)(OPOST);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 0;
  ::tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
  rawModeActive_ = true;
}

void TuiBackend::leaveRawMode() {
  if (!rawModeActive_) return;
  ::tcsetattr(STDIN_FILENO, TCSAFLUSH, &savedTermios_);
  rawModeActive_ = false;
}

void TuiBackend::pollTerminalSize() {
  ftxui::Dimensions dim = ftxui::Terminal::Size();
  int cols = dim.dimx > 0 ? dim.dimx : 80;
  int rows = dim.dimy > 0 ? dim.dimy : 24;
  if (cols != cols_ || rows != rows_) {
    cols_ = cols;
    rows_ = rows;
    sizeDirty_ = true;
  }
}

bool TuiBackend::initialize(int, int, std::string_view title) {
  enterRawMode();
  std::fputs("\x1b[?1049h", stdout);
  std::fputs("\x1b[?25l", stdout);
  std::fputs("\x1b[?1002h\x1b[?1006h", stdout);
  if (!title.empty()) std::fprintf(stdout, "\x1b]0;%.*s\x07", (int)title.size(), title.data());
  std::fflush(stdout);

  kittySupported_ = detectKittySupport();
  pollTerminalSize();
  grid_.resize(cols_, rows_);
  std::fputs("\x1b[2J", stdout);
  std::fflush(stdout);
  lastActivityMs_ = nowMs();
  return true;
}

bool TuiBackend::pumpEvents() {
  pollTerminalSize();
  releasePendingSyntheticClick();
  pollfd pfd{STDIN_FILENO, POLLIN, 0};
  ::poll(&pfd, 1, 16);
  readInput();
  return !shouldQuit_;
}

void TuiBackend::focusNext(bool backward) {
  if (focusables_.empty()) return;
  int previousOrdinal = focusedOrdinal_;
  int idx = -1;
  for (size_t k = 0; k < focusables_.size(); ++k) {
    if (focusables_[k].ordinal == focusedOrdinal_) {
      idx = (int)k;
      break;
    }
  }
  if (idx < 0) {
    idx = backward ? (int)focusables_.size() - 1 : 0;
  } else {
    idx = backward ? idx - 1 : idx + 1;
    if (idx < 0) idx = (int)focusables_.size() - 1;
    if (idx >= (int)focusables_.size()) idx = 0;
  }
  focusedOrdinal_ = focusables_[(size_t)idx].ordinal;
  if (focusedOrdinal_ != previousOrdinal && entry_.ordinal == previousOrdinal) entry_ = EntryEditState{};
  lastActivityMs_ = nowMs();
}

void TuiBackend::activateFocused() {
  for (const FocusableWidget &w : focusables_) {
    if (w.ordinal != focusedOrdinal_) continue;
    pointerX_ = ((float)w.box.x + (float)w.box.w * 0.5f) * cellPxW;
    pointerY_ = ((float)w.box.y + (float)w.box.h * 0.5f) * cellPxH;
    justClicked_ = true;
    pointerDown_ = true;
    releaseSyntheticClickNextFrame_ = true;
    lastActivityMs_ = nowMs();
    return;
  }
}

void TuiBackend::releasePendingSyntheticClick() {
  if (!releaseSyntheticClickNextFrame_) return;
  pointerDown_ = false;
  mouseSelecting_ = false;
  textMouseSelecting_ = false;
  releaseSyntheticClickNextFrame_ = false;
}

void TuiBackend::handleCsiSequence(std::string_view seq) {
  if (seq.empty()) return;
  char final = seq.back();
  std::string_view params = seq.substr(0, seq.size() - 1);
  auto toInt = [](std::string_view s) {
    int v = 0;
    for (char c : s)
      if (c >= '0' && c <= '9') v = v * 10 + (c - '0');
    return v;
  };
  int p1 = 0, p2 = 0;
  size_t semi = params.find(';');
  if (semi == std::string_view::npos) {
    p1 = toInt(params);
  } else {
    p1 = toInt(params.substr(0, semi));
    p2 = toInt(params.substr(semi + 1));
  }
  bool shift = false, ctrl = false;
  if (p2 > 0) {
    int m = p2 - 1;
    shift = (m & 1) != 0;
    ctrl = (m & 4) != 0;
  }

  auto dispatch = [&](TuiKeyCode code) {
    TuiKeyEvent ev{code, ctrl, shift};
    if (entry_.value) handleEntryKey(ev);
    else if (textSel_.active) handleTextKey(ev);
  };

  bool editing = entry_.value || textSel_.active;
  switch (final) {
  case 'C':
    if (editing) dispatch(TuiKeyCode::Right);
    else pendingSliderKeyStep_ = 1;
    break;
  case 'D':
    if (editing) dispatch(TuiKeyCode::Left);
    else pendingSliderKeyStep_ = -1;
    break;
  case 'A': // up
    if (openDropdownOrdinal_ >= 0) moveDropdownHighlight(-1);
    else if (!editing && hasScrollable_) {
      pointerX_ = ((float)lastScrollableViewport_.x + (float)lastScrollableViewport_.w * 0.5f) * cellPxW;
      pointerY_ = ((float)lastScrollableViewport_.y + (float)lastScrollableViewport_.h * 0.5f) * cellPxH;
      pendingScrollDelta_.y += cellPxH / 10.0f;
    } else if (!editing) {
      focusNext(true);
    }
    break;
  case 'B': // down
    if (openDropdownOrdinal_ >= 0) moveDropdownHighlight(1);
    else if (!editing && hasScrollable_) {
      pointerX_ = ((float)lastScrollableViewport_.x + (float)lastScrollableViewport_.w * 0.5f) * cellPxW;
      pointerY_ = ((float)lastScrollableViewport_.y + (float)lastScrollableViewport_.h * 0.5f) * cellPxH;
      pendingScrollDelta_.y -= cellPxH / 10.0f;
    } else if (!editing) {
      focusNext(false);
    }
    break;
  case 'H':
    dispatch(TuiKeyCode::Home);
    break;
  case 'F':
    dispatch(TuiKeyCode::End);
    break;
  case 'Z':
    focusNext(true); // shift+tab
    break;
  case '~':
    if (p1 == 1 || p1 == 7) dispatch(TuiKeyCode::Home);
    else if (p1 == 4 || p1 == 8) dispatch(TuiKeyCode::End);
    else if (p1 == 3) dispatch(TuiKeyCode::Delete);
    break;
  default:
    break;
  }
}

void TuiBackend::handleSgrMouse(std::string_view seq) {
  if (seq.size() < 2 || seq.front() != '<') return;
  char final = seq.back();
  std::string_view body = seq.substr(1, seq.size() - 2);

  size_t pos = 0;
  auto nextInt = [&]() {
    size_t start = pos;
    while (pos < body.size() && body[pos] != ';') ++pos;
    int v = 0;
    for (size_t k = start; k < pos; ++k) v = v * 10 + (body[k] - '0');
    if (pos < body.size()) ++pos;
    return v;
  };
  int cb = nextInt();
  int cx = nextInt();
  int cy = nextInt();

  pointerX_ = ((float)cx - 1.0f + 0.5f) * cellPxW;
  pointerY_ = ((float)cy - 1.0f + 0.5f) * cellPxH;
  lastActivityMs_ = nowMs();

  bool release = final == 'm';
  bool wheel = (cb & 64) != 0;
  bool motion = (cb & 32) != 0;
  int button = cb & 3;

  if (wheel) {
    switch (button) {
    case 0:
      pendingScrollDelta_.y += cellPxH / 10.0f;
      break;
    case 1:
      pendingScrollDelta_.y -= cellPxH / 10.0f;
      break;
    case 2:
      pendingScrollDelta_.x -= cellPxW * 4.0f / 10.0f;
      break;
    case 3:
      pendingScrollDelta_.x += cellPxW * 4.0f / 10.0f;
      break;
    default:
      break;
    }
    return;
  }
  if (motion) return;
  if (!release && button == 0) {
    pointerDown_ = true;
    justClicked_ = true;
  } else if (release) {
    pointerDown_ = false;
    mouseSelecting_ = false;
    textMouseSelecting_ = false;
  }
}

void TuiBackend::readInput() {
  unsigned char buf[4096];
  for (;;) {
    ssize_t n = ::read(STDIN_FILENO, buf, sizeof(buf));
    if (n <= 0) break;
    inputBuf_.insert(inputBuf_.end(), buf, buf + n);
  }

  size_t i = 0;
  while (i < inputBuf_.size()) {
    unsigned char b = inputBuf_[i];

    if (b == 0x1b) {
      if (i + 1 >= inputBuf_.size()) break;
      if (inputBuf_[i + 1] == '[') {
        size_t j = i + 2;
        while (j < inputBuf_.size() && !((inputBuf_[j] >= 'A' && inputBuf_[j] <= 'Z') || (inputBuf_[j] >= 'a' && inputBuf_[j] <= 'z') || inputBuf_[j] == '~')) ++j;
        if (j >= inputBuf_.size()) break;
        std::string_view seq(reinterpret_cast<const char *>(&inputBuf_[i + 2]), j - (i + 2) + 1);
        if (!seq.empty() && seq.front() == '<') handleSgrMouse(seq);
        else handleCsiSequence(seq);
        i = j + 1;
        continue;
      }
      if (inputBuf_[i + 1] == 'O') {
        if (i + 2 >= inputBuf_.size()) break;
        unsigned char f = inputBuf_[i + 2];
        TuiKeyEvent ev;
        if (f == 'H') ev.code = TuiKeyCode::Home;
        else if (f == 'F') ev.code = TuiKeyCode::End;
        if (ev.code != TuiKeyCode::None) {
          if (entry_.value) handleEntryKey(ev);
          else if (textSel_.active) handleTextKey(ev);
        }
        i += 3;
        continue;
      }
      ++i;
      continue;
    }

    if (b == 0x03) {
      shouldQuit_ = true;
      ++i;
      continue;
    }
    if (b == 0x7f || b == 0x08) {
      TuiKeyEvent ev{TuiKeyCode::Backspace, false, false};
      if (entry_.value) handleEntryKey(ev);
      else if (textSel_.active) handleTextKey(ev);
      ++i;
      continue;
    }
    if (b == 0x09) { // tab
      focusNext(false);
      ++i;
      continue;
    }
    if (b == 0x0d || b == 0x0a) { // enter
      if (openDropdownOrdinal_ >= 0 && openDropdownOrdinal_ == focusedOrdinal_) confirmDropdownHighlighted();
      else activateFocused();
      ++i;
      continue;
    }
    if (b == 0x20 && !entry_.value) { // space
      if (openDropdownOrdinal_ >= 0 && openDropdownOrdinal_ == focusedOrdinal_) confirmDropdownHighlighted();
      else activateFocused();
      ++i;
      continue;
    }
    if (b >= 1 && b <= 26) {
      TuiKeyEvent ev{};
      ev.ctrl = true;
      switch (b) {
      case 1:
        ev.code = TuiKeyCode::LetterA;
        break;
      case 22:
        ev.code = TuiKeyCode::LetterV;
        break;
      case 24:
        ev.code = TuiKeyCode::LetterX;
        break;
      default:
        break;
      }
      if (ev.code != TuiKeyCode::None) {
        if (entry_.value) handleEntryKey(ev);
        else if (textSel_.active) handleTextKey(ev);
      }
      ++i;
      continue;
    }

    size_t start = i;
    while (i < inputBuf_.size() && inputBuf_[i] != 0x1b && inputBuf_[i] >= 0x20) ++i;
    if (i > start) {
      insertAtCursor(std::string_view(reinterpret_cast<const char *>(&inputBuf_[start]), i - start));
    } else {
      ++i;
    }
  }
  inputBuf_.erase(inputBuf_.begin(), inputBuf_.begin() + (ptrdiff_t)i);
}

Clay_Dimensions TuiBackend::windowSize() const { return {(float)cols_ * cellPxW, (float)rows_ * cellPxH}; }

Clay_Dimensions TuiBackend::measureText(std::string_view text, FontFamily, uint16_t, bool, bool) const {
  if (text.empty()) return {0.0f, cellPxH};
  int width = ftxui::string_width(std::string(text));
  return {(float)width * cellPxW, cellPxH};
}

Clay_Dimensions TuiBackend::measureNativeChrome(NativeWidgetKind kind, std::string_view, uint16_t, bool) const {
  if (kind == NativeWidgetKind::Slider || kind == NativeWidgetKind::Dropdown) return {0.0f, cellPxH};
  if (kind == NativeWidgetKind::Checkbox || kind == NativeWidgetKind::Radio) return {indicatorIndent, cellPxH};
  if (kind == NativeWidgetKind::Switch) return {switchIndent, cellPxH};
  if (kind == NativeWidgetKind::Entry || kind == NativeWidgetKind::Button || kind == NativeWidgetKind::Link) return {0.0f, cellPxH};
  return {0.0f, 0.0f};
}

void TuiBackend::beginFrame() {
  if (sizeDirty_) {
    grid_.resize(cols_, rows_);
    std::fputs("\x1b[2J", stdout);
    sizeDirty_ = false;
  }
  Clay_SetPointerState({pointerX_, pointerY_}, pointerDown_);
  grid_.fillBackground({0, 0, cols_, rows_}, ftxui::Color::RGB(255, 255, 255));
  pendingImageEscapes_.clear();
}

void TuiBackend::present(Clay_RenderCommandArray commands) {
  closeDropdownIfClickedOutside();

  std::vector<std::pair<uint32_t, CellRect>> clipStack;
  pendingFocusables_.clear();
  hasScrollable_ = false;
  bool pendingIsCheckbox = false;
  bool pendingIsRadio = false;
  bool pendingIsSwitch = false;
  NativeWidgetMeta *pendingIndicatorMeta = nullptr;

  NativeWidgetMeta *pendingEntryMeta = nullptr;
  bool pendingEntryClicked = false;
  bool pendingEntryDragging = false;

  for (int32_t i = 0; i < commands.length; ++i) {
    Clay_RenderCommand *command = Clay_RenderCommandArray_Get(&commands, i);
    switch (command->commandType) {
    case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
      auto *meta = static_cast<NativeWidgetMeta *>(command->userData);
      if (
        meta && (meta->kind == NativeWidgetKind::Button || meta->kind == NativeWidgetKind::Checkbox || meta->kind == NativeWidgetKind::Radio ||
                 meta->kind == NativeWidgetKind::Switch || meta->kind == NativeWidgetKind::Entry || meta->kind == NativeWidgetKind::Dropdown ||
                 meta->kind == NativeWidgetKind::Slider || meta->kind == NativeWidgetKind::Link)
      ) {
        CellRect box = cellRect(command->boundingBox);
        pendingFocusables_.push_back({meta->ordinal, meta->kind, box});
        if (meta->ordinal == focusedOrdinal_) grid_.setGlyph(std::max(0, box.x - 1), box.y, "›", ftxui::Color::RGB(40, 90, 200), true);
      }
      pendingIsCheckbox = meta && meta->kind == NativeWidgetKind::Checkbox;
      pendingIsRadio = meta && meta->kind == NativeWidgetKind::Radio;
      pendingIsSwitch = meta && meta->kind == NativeWidgetKind::Switch;
      if (pendingIsCheckbox || pendingIsRadio || pendingIsSwitch) {
        pendingIndicatorMeta = meta;
        break;
      }
      if (meta && meta->kind == NativeWidgetKind::DropdownChevron) {
        drawDropdownChevron(command->boundingBox, Clay_Color{meta->chevronColor.r, meta->chevronColor.g, meta->chevronColor.b, meta->chevronColor.a}, meta->chevronPointsUp);
        break;
      }
      if (meta && meta->kind == NativeWidgetKind::Slider) {
        renderSlider(*meta, *command);
        break;
      }
      if (meta && meta->kind == NativeWidgetKind::Dropdown) {
        renderDropdownBox(*meta, *command);
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
      if (pendingIsSwitch && pendingIndicatorMeta) {
        renderSwitchIndicator(*pendingIndicatorMeta, command->boundingBox);
        pendingIsSwitch = false;
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
      if (meta && !drawIconGlyph(*meta, command->boundingBox)) drawImage(*meta, command->boundingBox, command->renderData.image.cornerRadius);
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
      CellRect clipRect = cellRect(command->boundingBox);
      grid_.pushClip(clipRect);
      clipStack.push_back({command->id, clipRect});
      break;
    }
    case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END:
      grid_.popClip();
      if (!clipStack.empty()) {
        renderScrollbarIfNeeded(clipStack.back().first, clipStack.back().second);
        clipStack.pop_back();
      }
      break;
    default:
      pendingIsCheckbox = false;
      pendingIsRadio = false;
      pendingIsSwitch = false;
      pendingEntryMeta = nullptr;
      pendingEntryClicked = false;
      pendingEntryDragging = false;
      break;
    }
  }

  if (openDropdownOrdinal_ >= 0 && openDropdownMeta_) renderDropdownPopup(*openDropdownMeta_, openDropdownBoxRect_);

  focusables_.swap(pendingFocusables_);
  if (focusedOrdinal_ < 0 && !focusables_.empty()) focusedOrdinal_ = focusables_.front().ordinal;

  justClicked_ = false;

  std::string frame = grid_.render();
  std::fputs("\x1b[H", stdout);
  std::fwrite(frame.data(), 1, frame.size(), stdout);
  if (!pendingImageEscapes_.empty()) std::fwrite(pendingImageEscapes_.data(), 1, pendingImageEscapes_.size(), stdout);
  std::fflush(stdout);
}

void TuiBackend::setCursor(CursorKind cursor) { currentCursorKind_ = cursor; }

void TuiBackend::shutdown() {
  if (!rawModeActive_) return;
  std::fputs("\x1b[?1002l\x1b[?1006l", stdout);
  std::fputs("\x1b[?25h", stdout);
  std::fputs("\x1b[?1049l", stdout);
  std::fflush(stdout);
  leaveRawMode();
}

std::unique_ptr<Backend> makeTuiBackend() { return std::make_unique<TuiBackend>(); }

} // namespace n8v::detail
