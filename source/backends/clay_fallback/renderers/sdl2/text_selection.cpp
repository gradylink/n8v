#include "sdl2_backend_impl.hpp"
#include "backends/text_edit_utils.hpp"

#include "core/text_style_flags.hpp"

#include <SDL2/SDL.h>

#include <algorithm>
#include <string>
#include <string_view>

namespace n8v::detail {

void Sdl2Backend::handleTextClick(int ordinal, std::string_view lineText, size_t localHitOffset, size_t lineOffset) {
  Uint32 now = SDL_GetTicks();
  if (ordinal == lastClickOrdinal_ && (now - lastClickTicks_) < 400) {
    clickCount_ = (clickCount_ % 3) + 1;
  } else {
    clickCount_ = 1;
  }
  lastClickTicks_ = now;
  lastClickOrdinal_ = ordinal;
  lastActivityTicks_ = now;

  entry_ = EntryEditState{};
  textSel_.ordinal = ordinal;
  textSel_.text = std::string(lineText);
  textSel_.active = true;
  textSel_.selectToEnd = false;

  if (clickCount_ == 2) {
    size_t probe = localHitOffset < lineText.size() ? localHitOffset : prevCodepointStart(lineText, localHitOffset);
    if (probe < lineText.size() && isWordByte((unsigned char)lineText[probe])) {
      textSel_.anchor = lineOffset + wordStartAt(lineText, localHitOffset);
      textSel_.cursor = lineOffset + wordEndAt(lineText, localHitOffset);
    } else {
      textSel_.cursor = textSel_.anchor = lineOffset + localHitOffset;
    }
    textMouseSelecting_ = false;
  } else if (clickCount_ == 3) {
    textSel_.anchor = 0;
    textSel_.cursor = lineOffset + lineText.size();
    textSel_.selectToEnd = true;
    textMouseSelecting_ = false;
  } else {
    textSel_.cursor = textSel_.anchor = lineOffset + localHitOffset;
    textMouseSelecting_ = true;
  }
}

void Sdl2Backend::moveTextCursor(size_t newPos, bool extendSelection) {
  textSel_.cursor = newPos;
  if (!extendSelection) textSel_.anchor = newPos;
}

void Sdl2Backend::handleTextKey(SDL_Keysym keysym) {
  if (!textSel_.active) return;
  lastActivityTicks_ = SDL_GetTicks();
  const std::string &s = textSel_.text;
  bool ctrl = (keysym.mod & KMOD_CTRL) != 0;
  bool shift = (keysym.mod & KMOD_SHIFT) != 0;

  switch (keysym.sym) {
  case SDLK_LEFT: {
    size_t target = (!shift && textSel_.hasSelection()) ? textSel_.selStart() : (ctrl ? wordLeft(s, textSel_.cursor) : prevCodepointStart(s, textSel_.cursor));
    moveTextCursor(target, shift);
    break;
  }
  case SDLK_RIGHT: {
    size_t target = (!shift && textSel_.hasSelection()) ? textSel_.selEnd() : (ctrl ? wordRight(s, textSel_.cursor) : nextCodepointStart(s, textSel_.cursor));
    moveTextCursor(target, shift);
    break;
  }
  case SDLK_HOME:
    moveTextCursor(0, shift);
    break;
  case SDLK_END:
    moveTextCursor(s.size(), shift);
    break;
  case SDLK_a:
    if (ctrl) {
      textSel_.anchor = 0;
      textSel_.cursor = s.size();
    }
    break;
  case SDLK_c:
    if (ctrl && textSel_.hasSelection()) {
      std::string selected = s.substr(textSel_.selStart(), textSel_.selEnd() - textSel_.selStart());
      SDL_SetClipboardText(selected.c_str());
    }
    break;
  default:
    break;
  }
}

bool Sdl2Backend::renderSelectableText(const Clay_RenderCommand &command, const TextStyleFlags &flags, std::string_view text) {
  int ord = flags.ordinal;
  uint16_t fontSize = command.renderData.text.fontSize;
  FontFamily family = flags.font;
  bool bold = flags.bold;
  bool italic = flags.italic;
  const Clay_BoundingBox &box = command.boundingBox;

  if (ord != textLineTrackOrdinal_) {
    textLineTrackOrdinal_ = ord;
    textLineTrackBase_ = text.data();
  }
  size_t lineOffset = (size_t)(text.data() - textLineTrackBase_);

  bool hit = pointerX_ >= box.x && pointerX_ <= box.x + box.width && pointerY_ >= box.y && pointerY_ <= box.y + box.height;

  if (hit && currentCursorKind_ == CursorKind::Default) {
    setCursor(CursorKind::Text);
  }

  if (justClicked_ && hit) {
    float localX = pointerX_ - box.x;
    size_t localHitOffset = hitTestOffset(text, family, fontSize, localX, bold, italic);
    handleTextClick(ord, text, localHitOffset, lineOffset);
  } else if (textMouseSelecting_ && textSel_.active && textSel_.ordinal == ord && hit) {
    float localX = pointerX_ - box.x;
    size_t localHitOffset = hitTestOffset(text, family, fontSize, localX, bold, italic);
    textSel_.cursor = lineOffset + localHitOffset;
    lastActivityTicks_ = SDL_GetTicks();
  }

  if (textSel_.active && textSel_.ordinal == ord) {
    size_t lineEndGlobal = lineOffset + text.size();
    std::string_view spanned(textLineTrackBase_, lineEndGlobal);
    if (spanned.size() > textSel_.text.size()) textSel_.text = std::string(spanned);

    if (textSel_.selectToEnd) textSel_.cursor = lineEndGlobal;

    if (textSel_.hasSelection()) {
      size_t gSelStart = textSel_.selStart();
      size_t gSelEnd = textSel_.selEnd();
      if (gSelEnd > lineOffset && gSelStart < lineEndGlobal) {
        size_t localSelStart = gSelStart > lineOffset ? gSelStart - lineOffset : 0;
        size_t localSelEnd = std::min(gSelEnd - lineOffset, text.size());
        drawSelectionHighlight(box, text, family, fontSize, localSelStart, localSelEnd, bold, italic);
        drawText(command, localSelStart, localSelEnd);
      } else {
        drawText(command);
      }
    } else {
      drawText(command);
    }
    return true;
  }
  return false;
}

} // namespace n8v::detail
