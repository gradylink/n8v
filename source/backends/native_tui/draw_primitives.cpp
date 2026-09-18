#include "color.hpp"
#include "tui_backend_impl.hpp"

#include "backends/text_edit_utils.hpp"
#include "core/text_style_flags.hpp"

#include <ftxui/screen/string.hpp>

#include <cmath>
#include <string>
#include <vector>

namespace n8v::detail {

CellRect TuiBackend::cellRect(const Clay_BoundingBox &box) const {
  int x0 = (int)std::lround(box.x / cellPxW);
  int y0 = (int)std::lround(box.y / cellPxH);
  int x1 = (int)std::lround((box.x + box.width) / cellPxW);
  int y1 = (int)std::lround((box.y + box.height) / cellPxH);
  int w = x1 - x0, h = y1 - y0;
  if (w <= 0 && box.width > 0.0f) w = 1;
  if (h <= 0 && box.height > 0.0f) h = 1;
  return {x0, y0, std::max(w, 0), std::max(h, 0)};
}

namespace {
std::vector<float> cellWidthTable(std::string_view text, std::vector<size_t> &offsets) {
  offsets = codepointByteOffsets(text);
  std::vector<float> cum(offsets.size() - 1);
  float running = 0.0f;
  for (size_t k = 0; k + 1 < offsets.size(); ++k) {
    running += (float)ftxui::string_width(std::string(text.substr(offsets[k], offsets[k + 1] - offsets[k])));
    cum[k] = running;
  }
  return cum;
}
} // namespace

size_t TuiBackend::hitTestOffset(std::string_view text, float localX) const {
  if (text.empty()) return 0;
  std::vector<size_t> offsets;
  std::vector<float> cum = cellWidthTable(text, offsets);
  if (cum.empty()) return text.size();

  float localCell = localX / cellPxW;
  float bestDist = std::abs(localCell);
  size_t bestIndex = 0;
  for (size_t k = 1; k <= cum.size(); ++k) {
    float dist = std::abs(localCell - cum[k - 1]);
    if (dist < bestDist) {
      bestDist = dist;
      bestIndex = k;
    }
  }
  return offsets[bestIndex];
}

float TuiBackend::caretPixelX(std::string_view text, size_t byteOffset) const {
  if (byteOffset == 0 || text.empty()) return 0.0f;
  std::vector<size_t> offsets;
  std::vector<float> cum = cellWidthTable(text, offsets);
  for (size_t k = 0; k < offsets.size(); ++k) {
    if (offsets[k] == byteOffset) return (k == 0 ? 0.0f : cum[k - 1]) * cellPxW;
  }
  return cum.empty() ? 0.0f : cum.back() * cellPxW;
}

void TuiBackend::drawCursorCaret(const Clay_BoundingBox &textBox, std::string_view value, size_t byteOffset) {
  CellRect rect = cellRect(textBox);
  int x = rect.x + (int)std::lround(caretPixelX(value, byteOffset) / cellPxW);
  grid_.invertCell(x, rect.y);
}

void TuiBackend::drawSelectionHighlight(const Clay_BoundingBox &textBox, std::string_view value, size_t selStart, size_t selEnd) {
  float x0 = caretPixelX(value, selStart);
  float x1 = caretPixelX(value, selEnd);
  Clay_BoundingBox box{textBox.x + x0, textBox.y, x1 - x0, textBox.height};
  grid_.fillBackground(cellRect(box), ftxui::Color::RGB(50, 100, 220));
}

void TuiBackend::drawText(const Clay_RenderCommand &command) {
  const Clay_TextRenderData &textData = command.renderData.text;
  auto *flags = static_cast<TextStyleFlags *>(command.userData);
  bool bold = flags && flags->bold;
  bool underline = flags && flags->underline;
  bool strikethrough = flags && flags->strikethrough;
  std::string_view text(textData.stringContents.chars, (size_t)textData.stringContents.length);
  if (text.empty()) return;

  CellRect rect = cellRect(command.boundingBox);
  ftxui::Color fg = toFtxuiColor(textData.textColor);

  std::vector<size_t> offsets = codepointByteOffsets(text);
  int x = rect.x, y = rect.y;
  for (size_t k = 0; k + 1 < offsets.size(); ++k) {
    std::string glyph(text.substr(offsets[k], offsets[k + 1] - offsets[k]));
    int w = ftxui::string_width(glyph);
    if (w <= 0) continue;
    grid_.setGlyph(x, y, glyph, fg, bold, underline, /*inverted=*/false, strikethrough);
    if (w >= 2) grid_.markWideContinuation(x + 1, y);
    x += w;
  }
}

void TuiBackend::drawTextAt(int x, int y, std::string_view text, const Clay_Color &color, bool bold) {
  ftxui::Color fg = toFtxuiColor(color);
  std::vector<size_t> offsets = codepointByteOffsets(text);
  int cx = x;
  for (size_t k = 0; k + 1 < offsets.size(); ++k) {
    std::string glyph(text.substr(offsets[k], offsets[k + 1] - offsets[k]));
    int w = ftxui::string_width(glyph);
    if (w <= 0) continue;
    grid_.setGlyph(cx, y, glyph, fg, bold);
    if (w >= 2) grid_.markWideContinuation(cx + 1, y);
    cx += w;
  }
}

void TuiBackend::renderScrollbarIfNeeded(uint32_t elementId, const CellRect &viewport) {
  Clay_ScrollContainerData data = Clay_GetScrollContainerData(Clay_ElementId{elementId});
  if (!data.found || !data.config.vertical) return;
  float viewportH = data.scrollContainerDimensions.height;
  float contentH = data.contentDimensions.height;
  if (contentH <= viewportH + 1.0f || viewport.w <= 0 || viewport.h <= 0) return; // nothing to scroll

  // Remembered so Up/Down can scroll this container by keyboard even without a mouse hovering
  // it - see readInput()'s handling of CSI 'A'/'B'.
  hasScrollable_ = true;
  lastScrollableViewport_ = viewport;

  int barX = viewport.x + viewport.w - 1;
  float maxScroll = std::max(contentH - viewportH, 1.0f);
  float scrolled = data.scrollPosition ? std::clamp(-data.scrollPosition->y, 0.0f, maxScroll) : 0.0f;

  // Clay_ScrollContainerData.scrollPosition is documented as writable specifically for external
  // scrollbar control. Re-evaluated every frame, so this doubles as click-to-jump (one frame,
  // pointerDown_ true) and drag (pointerDown_ stays true across frames while the button is held,
  // each with an updated pointerY_ from mouse-move events).
  if (data.scrollPosition && pointerDown_) {
    int pcx = (int)(pointerX_ / cellPxW);
    int pcy = (int)(pointerY_ / cellPxH);
    if (pcx == barX && pcy >= viewport.y && pcy < viewport.y + viewport.h) {
      float clickFraction = viewport.h > 1 ? (float)(pcy - viewport.y) / (float)(viewport.h - 1) : 0.0f;
      scrolled = std::clamp(clickFraction * maxScroll, 0.0f, maxScroll);
      data.scrollPosition->y = -scrolled;
    }
  }

  float fraction = scrolled / maxScroll;

  int thumbHeight = std::clamp((int)std::lround((viewportH / contentH) * (float)viewport.h), 1, viewport.h);
  int trackSpan = viewport.h - thumbHeight;
  int thumbStart = viewport.y + (int)std::lround(fraction * (float)trackSpan);

  for (int y = viewport.y; y < viewport.y + viewport.h; ++y) {
    bool isThumb = y >= thumbStart && y < thumbStart + thumbHeight;
    grid_.setGlyph(barX, y, isThumb ? "█" : "│", isThumb ? ftxui::Color::RGB(140, 140, 140) : ftxui::Color::RGB(220, 220, 220));
  }
}

void TuiBackend::drawFocusRing(const Clay_BoundingBox &box, const Clay_CornerRadius &cornerRadius) {
  drawRoundedRectBorder(box, Clay_Color{60, 110, 220, 255}, cornerRadius, 1.0f);
}

void TuiBackend::drawFilledRect(const Clay_BoundingBox &box, const Clay_Color &color) { grid_.fillBackground(cellRect(box), toFtxuiColor(color)); }

void TuiBackend::drawRoundedRect(const Clay_BoundingBox &box, const Clay_Color &color, const Clay_CornerRadius &) { drawFilledRect(box, color); }

void TuiBackend::drawRoundedRectBorder(const Clay_BoundingBox &box, const Clay_Color &color, const Clay_CornerRadius &corner, float strokeWidth) {
  if (strokeWidth <= 0.0f) return;
  CellRect rect = cellRect(box);
  if (rect.w <= 0 || rect.h <= 0) return;
  ftxui::Color fg = toFtxuiColor(color);

  // A rounded-corner box border needs a top row distinct from the bottom row to mean anything;
  // collapsed to a single row (as most widgets - Entry included - are now, to stay grid-aligned)
  // the "top" and "bottom" edges are the same cells, so what actually rendered was a stray
  // floating cap (rounded-corner-then-dash-then-rounded-corner) drawn over the content's own
  // row. Use simple edge brackets there instead - a border box just doesn't mean anything with
  // no vertical extent to outline.
  if (rect.h == 1) {
    if (rect.w == 1) {
      grid_.setGlyph(rect.x, rect.y, "[", fg);
    } else {
      grid_.setGlyph(rect.x, rect.y, "▏", fg);
      grid_.setGlyph(rect.x + rect.w - 1, rect.y, "▕", fg);
    }
    return;
  }

  bool round = (corner.topLeft + corner.topRight + corner.bottomLeft + corner.bottomRight) > 0.0f;
  const char *tl = round ? "╭" : "┌"; // ╭ / ┌
  const char *tr = round ? "╮" : "┐"; // ╮ / ┐
  const char *bl = round ? "╰" : "└"; // ╰ / └
  const char *br = round ? "╯" : "┘"; // ╯ / ┘

  int x0 = rect.x, y0 = rect.y, x1 = rect.x + rect.w - 1, y1 = rect.y + rect.h - 1;
  if (x0 == x1 && y0 == y1) {
    grid_.setGlyph(x0, y0, "+", fg);
    return;
  }
  for (int x = x0 + 1; x < x1; ++x) {
    grid_.setGlyph(x, y0, "─", fg); // ─
    if (y1 != y0) grid_.setGlyph(x, y1, "─", fg);
  }
  for (int y = y0 + 1; y < y1; ++y) {
    grid_.setGlyph(x0, y, "│", fg); // │
    if (x1 != x0) grid_.setGlyph(x1, y, "│", fg);
  }
  grid_.setGlyph(x0, y0, tl, fg);
  if (x1 != x0) grid_.setGlyph(x1, y0, tr, fg);
  if (y1 != y0) grid_.setGlyph(x0, y1, bl, fg);
  if (x1 != x0 && y1 != y0) grid_.setGlyph(x1, y1, br, fg);
}

} // namespace n8v::detail
