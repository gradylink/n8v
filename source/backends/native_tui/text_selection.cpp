#include "tui_backend_impl.hpp"

#include "backends/text_edit_utils.hpp"
#include "core/text_style_flags.hpp"

#include <string>
#include <string_view>

namespace n8v::detail {

void TuiBackend::handleTextClick(int ordinal, std::string_view text, size_t hitOffset) {
  int64_t now = nowMs();
  if (ordinal == lastClickOrdinal_ && (now - lastClickMs_) < 400) {
    clickCount_ = (clickCount_ % 3) + 1;
  } else {
    clickCount_ = 1;
  }
  lastClickMs_ = now;
  lastClickOrdinal_ = ordinal;
  lastActivityMs_ = now;

  entry_ = EntryEditState{};
  textSel_.ordinal = ordinal;
  textSel_.text = std::string(text);
  textSel_.active = true;

  if (clickCount_ == 2) {
    size_t probe = hitOffset < text.size() ? hitOffset : prevCodepointStart(text, hitOffset);
    if (probe < text.size() && isWordByte((unsigned char)text[probe])) {
      textSel_.anchor = wordStartAt(text, hitOffset);
      textSel_.cursor = wordEndAt(text, hitOffset);
    } else {
      textSel_.cursor = textSel_.anchor = hitOffset;
    }
    textMouseSelecting_ = false;
  } else if (clickCount_ == 3) {
    textSel_.anchor = 0;
    textSel_.cursor = text.size();
    textMouseSelecting_ = false;
  } else {
    textSel_.cursor = textSel_.anchor = hitOffset;
    textMouseSelecting_ = true;
  }
}

void TuiBackend::moveTextCursor(size_t newPos, bool extendSelection) {
  textSel_.cursor = newPos;
  if (!extendSelection) textSel_.anchor = newPos;
}

void TuiBackend::handleTextKey(const TuiKeyEvent &key) {
  if (!textSel_.active) return;
  lastActivityMs_ = nowMs();
  const std::string &s = textSel_.text;
  bool ctrl = key.ctrl;
  bool shift = key.shift;

  switch (key.code) {
  case TuiKeyCode::Left: {
    size_t target = (!shift && textSel_.hasSelection()) ? textSel_.selStart() : (ctrl ? wordLeft(s, textSel_.cursor) : prevCodepointStart(s, textSel_.cursor));
    moveTextCursor(target, shift);
    break;
  }
  case TuiKeyCode::Right: {
    size_t target = (!shift && textSel_.hasSelection()) ? textSel_.selEnd() : (ctrl ? wordRight(s, textSel_.cursor) : nextCodepointStart(s, textSel_.cursor));
    moveTextCursor(target, shift);
    break;
  }
  case TuiKeyCode::Home:
    moveTextCursor(0, shift);
    break;
  case TuiKeyCode::End:
    moveTextCursor(s.size(), shift);
    break;
  case TuiKeyCode::LetterA:
    if (ctrl) {
      textSel_.anchor = 0;
      textSel_.cursor = s.size();
    }
    break;
  case TuiKeyCode::LetterC:
    if (ctrl && textSel_.hasSelection()) {
      internalClipboard_ = s.substr(textSel_.selStart(), textSel_.selEnd() - textSel_.selStart());
    }
    break;
  default:
    break;
  }
}

bool TuiBackend::renderSelectableText(const Clay_RenderCommand &command, const TextStyleFlags &flags, std::string_view text) {
  int ord = flags.ordinal;
  const Clay_BoundingBox &box = command.boundingBox;

  bool hit = pointerX_ >= box.x && pointerX_ <= box.x + box.width && pointerY_ >= box.y && pointerY_ <= box.y + box.height;

  if (justClicked_ && hit) {
    float localX = pointerX_ - box.x;
    size_t hitOffset = hitTestOffset(text, localX);
    handleTextClick(ord, text, hitOffset);
  } else if (textMouseSelecting_ && textSel_.active && textSel_.ordinal == ord) {
    float localX = pointerX_ - box.x;
    textSel_.cursor = hitTestOffset(text, localX);
    lastActivityMs_ = nowMs();
  }

  if (textSel_.active && textSel_.ordinal == ord) {
    textSel_.text = std::string(text);
    if (textSel_.hasSelection()) {
      drawSelectionHighlight(box, text, textSel_.selStart(), textSel_.selEnd());
    }
    drawText(command);
    return true;
  }
  return false;
}

} // namespace n8v::detail
