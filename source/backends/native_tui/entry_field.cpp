#include "tui_backend_impl.hpp"

#include "backends/text_edit_utils.hpp"
#include "core/native_widget_meta.hpp"
#include "core/text_style_flags.hpp"
#include "core/ui_core_internal.hpp"

#include <string>
#include <string_view>

namespace n8v::detail {

void TuiBackend::fireEntryChange() {
  if (!entry_.value) return;
  if (entry_.buf) ui_internal::writeToStringBuf(*entry_.value, *entry_.buf);
  if (entry_.onChange) entry_.onChange(entry_.buf ? entry_.buf->data : entry_.value->c_str(), entry_.buf ? entry_.buf->length : entry_.value->size(), entry_.onChangeUserdata);
}

void TuiBackend::handleEntryClick(NativeWidgetMeta *meta, size_t hitOffset) {
  int64_t now = nowMs();
  if (meta->ordinal == lastClickOrdinal_ && (now - lastClickMs_) < 400) {
    clickCount_ = (clickCount_ % 3) + 1;
  } else {
    clickCount_ = 1;
  }
  lastClickMs_ = now;
  lastClickOrdinal_ = meta->ordinal;
  lastActivityMs_ = now;

  textSel_ = TextSelState{};
  entry_.value = meta->entryValue;
  entry_.buf = meta->entryBuf;
  entry_.onChange = meta->onEntryChange;
  entry_.onChangeUserdata = meta->onEntryChangeUserdata;
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

void TuiBackend::eraseEntrySelection() {
  if (!entry_.value || !entry_.hasSelection()) return;
  size_t s = entry_.selStart(), e = entry_.selEnd();
  entry_.value->erase(s, e - s);
  entry_.cursor = entry_.anchor = s;
}

void TuiBackend::insertAtCursor(std::string_view text) {
  if (!entry_.value || text.empty()) return;
  lastActivityMs_ = nowMs();
  if (entry_.hasSelection()) eraseEntrySelection();
  entry_.value->insert(entry_.cursor, text);
  entry_.cursor += text.size();
  entry_.anchor = entry_.cursor;
  fireEntryChange();
}

void TuiBackend::moveEntryCursor(size_t newPos, bool extendSelection) {
  entry_.cursor = newPos;
  if (!extendSelection) entry_.anchor = newPos;
}

void TuiBackend::handleEntryKey(const TuiKeyEvent &key) {
  if (!entry_.value) return;
  lastActivityMs_ = nowMs();
  std::string &s = *entry_.value;
  bool ctrl = key.ctrl;
  bool shift = key.shift;

  switch (key.code) {
  case TuiKeyCode::Left: {
    size_t target = (!shift && entry_.hasSelection()) ? entry_.selStart() : (ctrl ? wordLeft(s, entry_.cursor) : prevCodepointStart(s, entry_.cursor));
    moveEntryCursor(target, shift);
    break;
  }
  case TuiKeyCode::Right: {
    size_t target = (!shift && entry_.hasSelection()) ? entry_.selEnd() : (ctrl ? wordRight(s, entry_.cursor) : nextCodepointStart(s, entry_.cursor));
    moveEntryCursor(target, shift);
    break;
  }
  case TuiKeyCode::Home:
    moveEntryCursor(0, shift);
    break;
  case TuiKeyCode::End:
    moveEntryCursor(s.size(), shift);
    break;
  case TuiKeyCode::LetterA:
    if (ctrl) {
      entry_.anchor = 0;
      entry_.cursor = s.size();
    }
    break;
  case TuiKeyCode::LetterC:
  case TuiKeyCode::LetterX:
    if (ctrl && entry_.hasSelection()) {
      internalClipboard_ = s.substr(entry_.selStart(), entry_.selEnd() - entry_.selStart());
      if (key.code == TuiKeyCode::LetterX) {
        eraseEntrySelection();
        fireEntryChange();
      }
    }
    break;
  case TuiKeyCode::LetterV:
    if (ctrl && !internalClipboard_.empty()) insertAtCursor(internalClipboard_);
    break;
  case TuiKeyCode::Backspace:
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
  case TuiKeyCode::Delete:
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

void TuiBackend::renderEntryText(NativeWidgetMeta *meta, const Clay_RenderCommand &command, bool clicked, bool dragging) {
  const std::string &value = *meta->entryValue;
  bool isPassword = meta->password;
  std::string_view displayText(command.renderData.text.stringContents.chars, (size_t)command.renderData.text.stringContents.length);

  bool hasValue = !value.empty();
  if (clicked) {
    size_t realHit = 0;
    if (hasValue) {
      float localX = pointerX_ - command.boundingBox.x;
      size_t displayHit = hitTestOffset(displayText, localX);
      realHit = displayOffsetToRealOffset(value, displayHit, isPassword);
    }
    handleEntryClick(meta, realHit);
  } else if (dragging) {
    if (hasValue) {
      float localX = pointerX_ - command.boundingBox.x;
      size_t displayHit = hitTestOffset(displayText, localX);
      entry_.cursor = displayOffsetToRealOffset(value, displayHit, isPassword);
    } else {
      entry_.cursor = 0;
    }
    lastActivityMs_ = nowMs();
  }

  bool isFocused = meta->ordinal == entry_.ordinal;
  bool hasSel = isFocused && entry_.hasSelection();
  size_t displaySelStart = realOffsetToDisplayOffset(value, entry_.selStart(), isPassword);
  size_t displaySelEnd = realOffsetToDisplayOffset(value, entry_.selEnd(), isPassword);
  if (hasSel) drawSelectionHighlight(command.boundingBox, displayText, displaySelStart, displaySelEnd);
  drawText(command);
  if (isFocused && !hasSel && blinkOn()) {
    size_t displayCursor = realOffsetToDisplayOffset(value, entry_.cursor, isPassword);
    drawCursorCaret(command.boundingBox, displayText, displayCursor);
  }
}

} // namespace n8v::detail
