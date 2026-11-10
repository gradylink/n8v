#include "backends/text_edit_utils.hpp"
#include "sdl2_backend_impl.hpp"

#include "core/native_widget_meta.hpp"
#include "core/text_style_flags.hpp"
#include "core/ui_core_internal.hpp"

#include <SDL2/SDL.h>

#include "core/entry_lines.hpp"

#include <algorithm>
#include <string>
#include <string_view>

namespace n8v::detail {

void Sdl2Backend::fireEntryChange() {
  if (!entry_.value) return;
  if (entry_.buf) ui_internal::writeToStringBuf(*entry_.value, *entry_.buf);
  if (entry_.onChange) entry_.onChange(entry_.buf ? entry_.buf->data : entry_.value->c_str(), entry_.buf ? entry_.buf->length : entry_.value->size(), entry_.onChangeUserdata);
}

void Sdl2Backend::handleEntryClick(NativeWidgetMeta *meta, size_t hitOffset) {
  const int key = stableWidgetKey(*meta);
  Uint32 now = SDL_GetTicks();
  if (key == lastClickOrdinal_ && (now - lastClickTicks_) < 400) {
    clickCount_ = (clickCount_ % 3) + 1;
  } else {
    clickCount_ = 1;
  }
  lastClickTicks_ = now;
  lastClickOrdinal_ = key;
  lastActivityTicks_ = now;

  textSel_ = TextSelState{};
  entry_.value = meta->entryValue;
  entry_.buf = meta->entryBuf;
  entry_.onChange = meta->onEntryChange;
  entry_.onChangeUserdata = meta->onEntryChangeUserdata;
  entry_.onSubmit = meta->onEntrySubmit;
  entry_.onSubmitUserdata = meta->onEntrySubmitUserdata;
  entry_.multiline = meta->entryMultiline;
  entry_.submitMode = meta->entrySubmitMode;
  entry_.ordinal = key;

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

void Sdl2Backend::eraseEntrySelection() {
  if (!entry_.value || !entry_.hasSelection()) return;
  size_t s = entry_.selStart(), e = entry_.selEnd();
  entry_.value->erase(s, e - s);
  entry_.cursor = entry_.anchor = s;
}

void Sdl2Backend::insertAtCursor(std::string_view text) {
  if (!entry_.value || text.empty()) return;
  lastActivityTicks_ = SDL_GetTicks();
  scrollToCaret_ = true;
  if (entry_.hasSelection()) eraseEntrySelection();
  entry_.value->insert(entry_.cursor, text);
  entry_.cursor += text.size();
  entry_.anchor = entry_.cursor;
  fireEntryChange();
}

void Sdl2Backend::moveEntryCursor(size_t newPos, bool extendSelection) {
  entry_.cursor = newPos;
  if (!extendSelection) entry_.anchor = newPos;
}

void Sdl2Backend::handleEntryKey(SDL_Keysym keysym) {
  if (!entry_.value) return;
  lastActivityTicks_ = SDL_GetTicks();
  scrollToCaret_ = true;
  std::string &s = *entry_.value;
  bool ctrl = (keysym.mod & KMOD_CTRL) != 0;
  bool shift = (keysym.mod & KMOD_SHIFT) != 0;

  switch (keysym.sym) {
  case SDLK_RETURN:
  case SDLK_RETURN2:
  case SDLK_KP_ENTER:
    if (entry_.multiline) {
#ifdef __APPLE__
      const bool command = (keysym.mod & KMOD_GUI) != 0;
#else
      const bool command = ctrl;
#endif
      if (enterSubmits(entry_.submitMode, command, shift)) {
        if (entry_.onSubmit) entry_.onSubmit(entry_.onSubmitUserdata);
      } else {
        insertAtCursor("\n");
      }
    } else if (entry_.onSubmit) {
      entry_.onSubmit(entry_.onSubmitUserdata);
    }
    break;
  case SDLK_UP:
  case SDLK_DOWN:
    if (entry_.multiline) moveEntryCursorVertically(keysym.sym == SDLK_UP ? -1 : 1, shift);
    break;
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
    if (entry_.multiline && !ctrl) moveEntryCursorToLineEdge(false, shift);
    else moveEntryCursor(0, shift);
    break;
  case SDLK_END:
    if (entry_.multiline && !ctrl) moveEntryCursorToLineEdge(true, shift);
    else moveEntryCursor(s.size(), shift);
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
    } else if (ctrl) {
      size_t start = wordLeft(s, entry_.cursor);
      s.erase(start, entry_.cursor - start);
      entry_.cursor = entry_.anchor = start;
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
    } else if (ctrl) {
      size_t end = wordRight(s, entry_.cursor);
      s.erase(entry_.cursor, end - entry_.cursor);
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

void Sdl2Backend::renderEntryText(NativeWidgetMeta *meta, const Clay_RenderCommand &command, bool clicked, bool dragging) {
  auto *flags = static_cast<TextStyleFlags *>(command.userData);
  uint16_t fontSize = command.renderData.text.fontSize;
  FontFamily family = flags ? flags->font : FontFamily::DejaVuSans;
  const std::string &value = *meta->entryValue;
  bool isPassword = meta->password;
  std::string_view displayText(command.renderData.text.stringContents.chars, (size_t)command.renderData.text.stringContents.length);

  bool hasValue = !value.empty();
  if (clicked) {
    size_t realHit = 0;
    if (hasValue) {
      float localX = pointerX_ - command.boundingBox.x;
      size_t displayHit = hitTestOffset(displayText, family, fontSize, localX);
      realHit = displayOffsetToRealOffset(value, displayHit, isPassword);
    }
    handleEntryClick(meta, realHit);
  } else if (dragging) {
    if (hasValue) {
      float localX = pointerX_ - command.boundingBox.x;
      size_t displayHit = hitTestOffset(displayText, family, fontSize, localX);
      entry_.cursor = displayOffsetToRealOffset(value, displayHit, isPassword);
    } else {
      entry_.cursor = 0;
    }
    lastActivityTicks_ = SDL_GetTicks();
  }

  bool isFocused = stableWidgetKey(*meta) == entry_.ordinal;
  bool hasSel = isFocused && entry_.hasSelection();
  size_t displaySelStart = realOffsetToDisplayOffset(value, entry_.selStart(), isPassword);
  size_t displaySelEnd = realOffsetToDisplayOffset(value, entry_.selEnd(), isPassword);
  if (hasSel) drawSelectionHighlight(command.boundingBox, displayText, family, fontSize, displaySelStart, displaySelEnd);
  if (hasSel) {
    drawText(command, displaySelStart, displaySelEnd);
  } else {
    drawText(command);
  }
  if (isFocused && !hasSel && blinkOn()) {
    size_t displayCursor = realOffsetToDisplayOffset(value, entry_.cursor, isPassword);
    drawCursorCaret(command.boundingBox, displayText, family, fontSize, displayCursor);
  }
}

void Sdl2Backend::renderEntryLine(NativeWidgetMeta *meta, const Clay_RenderCommand &command) {
  auto *flags = static_cast<TextStyleFlags *>(command.userData);
  const FontFamily family = flags ? flags->font : FontFamily::DejaVuSans;
  const uint16_t fontSize = command.renderData.text.fontSize;
  const std::string &value = *meta->entryValue;
  const Clay_BoundingBox &box = command.boundingBox;
  const int key = stableWidgetKey(*meta);
  if (key == entry_.ordinal) entry_.submitMode = meta->entrySubmitMode;

  if (value.empty() || !meta->entryDisplayBase) {
    drawText(command);
    multilineLines_.push_back(EntryLineBox{0, 0, box, std::string_view(), family, fontSize});
    return;
  }

  std::string_view text(command.renderData.text.stringContents.chars, (size_t)command.renderData.text.stringContents.length);
  const size_t start = std::min((size_t)(text.data() - meta->entryDisplayBase), value.size());
  const size_t length = std::min(text.size(), value.size() - start);

  const size_t previousEnd = multilineLines_.empty() ? 0 : multilineLines_.back().end;
  const size_t newlines = (size_t)std::count(value.begin() + (ptrdiff_t)previousEnd, value.begin() + (ptrdiff_t)start, '\n');
  const size_t blanks = multilineLines_.empty() ? newlines : (newlines > 0 ? newlines - 1 : 0);
  for (size_t k = 0; k < blanks; ++k) {
    const size_t at = multilineLines_.empty() ? k : previousEnd + 1 + k;
    multilineLines_.push_back(
      EntryLineBox{at, at, Clay_BoundingBox{box.x, box.y - (float)(blanks - k) * box.height, box.width, box.height}, std::string_view(), family, fontSize}
    );
  }

  EntryLineBox line{start, start + length, box, text, family, fontSize};
  multilineLines_.push_back(line);

  if (key == entry_.ordinal && entry_.hasSelection()) {
    const size_t selStart = entry_.selStart(), selEnd = entry_.selEnd();
    if (selEnd > line.start && selStart < line.end) {
      const size_t localStart = selStart > line.start ? selStart - line.start : 0;
      const size_t localEnd = std::min(selEnd - line.start, length);
      drawSelectionHighlight(box, text, family, fontSize, localStart, localEnd);
      drawText(command, localStart, localEnd);
      return;
    }
  }
  drawText(command);
}

size_t Sdl2Backend::entryLineForCursor(const std::vector<EntryLineBox> &lines, size_t cursor) const {
  for (size_t i = 0; i < lines.size(); ++i) {
    if (cursor > lines[i].end) continue;
    if (cursor == lines[i].end && i + 1 < lines.size() && lines[i + 1].start == cursor) return i + 1;
    return i;
  }
  return lines.empty() ? 0 : lines.size() - 1;
}

size_t Sdl2Backend::entryOffsetAt(const std::vector<EntryLineBox> &lines, float x, float y) const {
  size_t index = lines.size() - 1;
  for (size_t i = 0; i < lines.size(); ++i) {
    if (y < lines[i].box.y + lines[i].box.height) {
      index = i;
      break;
    }
  }
  const EntryLineBox &line = lines[index];
  const size_t local = hitTestOffset(line.text, line.family, line.fontSize, x - line.box.x);
  return line.start + std::min(local, line.end - line.start);
}

void Sdl2Backend::moveEntryCursorVertically(int direction, bool extendSelection) {
  const std::vector<EntryLineBox> &lines = focusedLines_;
  if (!entry_.value || lines.empty()) return;
  const size_t index = entryLineForCursor(lines, entry_.cursor);
  if (direction < 0 && index == 0) return moveEntryCursor(0, extendSelection);
  if (direction > 0 && index + 1 == lines.size()) return moveEntryCursor(entry_.value->size(), extendSelection);

  const EntryLineBox &from = lines[index];
  const size_t fromLocal = std::min(entry_.cursor - std::min(entry_.cursor, from.start), from.text.size());
  const float x = caretPixelX(from.text, from.family, from.fontSize, fromLocal);
  const EntryLineBox &to = lines[(size_t)((ptrdiff_t)index + direction)];
  moveEntryCursor(to.start + std::min(hitTestOffset(to.text, to.family, to.fontSize, x), to.end - to.start), extendSelection);
}

void Sdl2Backend::moveEntryCursorToLineEdge(bool toEnd, bool extendSelection) {
  const std::vector<EntryLineBox> &lines = focusedLines_;
  if (!entry_.value || lines.empty()) return moveEntryCursor(toEnd ? entry_.value->size() : 0, extendSelection);
  const size_t index = entryLineForCursor(lines, entry_.cursor);
  size_t target = toEnd ? lines[index].end : lines[index].start;
  if (toEnd && index + 1 < lines.size() && lines[index + 1].start == target && target > lines[index].start) target = prevCodepointStart(*entry_.value, target);
  moveEntryCursor(target, extendSelection);
}

void Sdl2Backend::finishMultilineEntry() {
  NativeWidgetMeta *meta = multilineMeta_;
  multilineMeta_ = nullptr;
  if (!meta || !meta->entryValue) return;
  const std::vector<EntryLineBox> &lines = multilineLines_;
  const bool hasText = !meta->entryValue->empty() && !lines.empty();

  if (multilineClicked_) {
    handleEntryClick(meta, hasText ? entryOffsetAt(lines, pointerX_, pointerY_) : 0);
    scrollToCaret_ = true;
  } else if (multilineDragging_ && hasText) {
    entry_.cursor = entryOffsetAt(lines, pointerX_, pointerY_);
    lastActivityTicks_ = SDL_GetTicks();
    scrollToCaret_ = true;
  }

  if (stableWidgetKey(*meta) != entry_.ordinal || lines.empty()) return;
  focusedLines_ = lines;

  const EntryLineBox &caretLine = lines[entryLineForCursor(lines, entry_.cursor)];
  if (!entry_.hasSelection() && blinkOn()) {
    drawCursorCaret(
      caretLine.box,
      caretLine.text,
      caretLine.family,
      caretLine.fontSize,
      std::min(entry_.cursor - std::min(entry_.cursor, caretLine.start), caretLine.text.size())
    );
  }

  if (scrollToCaret_) {
    scrollToCaret_ = false;
    constexpr float kMargin = 4.0f;
    float shift = 0.0f;
    if (caretLine.box.y < multilineBox_.y + kMargin) {
      shift = multilineBox_.y + kMargin - caretLine.box.y;
    } else if (caretLine.box.y + caretLine.box.height > multilineBox_.y + multilineBox_.height - kMargin) {
      shift = (multilineBox_.y + multilineBox_.height - kMargin) - (caretLine.box.y + caretLine.box.height);
    }
    if (shift != 0.0f) {
      Clay_ScrollContainerData scroll = Clay_GetScrollContainerData(Clay_ElementId{multilineId_});
      if (scroll.found && scroll.scrollPosition) scroll.scrollPosition->y += shift;
    }
  }
}

} // namespace n8v::detail
