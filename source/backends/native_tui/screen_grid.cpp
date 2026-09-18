#include "screen_grid.hpp"

#include <algorithm>

namespace n8v::detail {

void ScreenGrid::resize(int cols, int rows) {
  cols = std::max(cols, 1);
  rows = std::max(rows, 1);
  if (cols == cols_ && rows == rows_) return;
  cols_ = cols;
  rows_ = rows;
  screen_ = ftxui::Screen::Create(ftxui::Dimension::Fixed(cols_), ftxui::Dimension::Fixed(rows_));
  clipStack_.clear();
}

void ScreenGrid::pushClip(CellRect rect) {
  clip(rect);
  clipStack_.push_back(rect);
}

void ScreenGrid::popClip() {
  if (!clipStack_.empty()) clipStack_.pop_back();
}

bool ScreenGrid::clip(CellRect &rect) const {
  if (!clipStack_.empty()) {
    const CellRect &c = clipStack_.back();
    int x0 = std::max(rect.x, c.x), y0 = std::max(rect.y, c.y);
    int x1 = std::min(rect.x + rect.w, c.x + c.w), y1 = std::min(rect.y + rect.h, c.y + c.h);
    rect = {x0, y0, std::max(x1 - x0, 0), std::max(y1 - y0, 0)};
  }
  int x0 = std::max(rect.x, 0), y0 = std::max(rect.y, 0);
  int x1 = std::min(rect.x + rect.w, cols_), y1 = std::min(rect.y + rect.h, rows_);
  rect = {x0, y0, std::max(x1 - x0, 0), std::max(y1 - y0, 0)};
  return rect.w > 0 && rect.h > 0;
}

bool ScreenGrid::visible(int x, int y) const {
  if (x < 0 || y < 0 || x >= cols_ || y >= rows_) return false;
  if (clipStack_.empty()) return true;
  const CellRect &c = clipStack_.back();
  return x >= c.x && x < c.x + c.w && y >= c.y && y < c.y + c.h;
}

void ScreenGrid::fillBackground(CellRect rect, ftxui::Color bg) {
  if (!clip(rect)) return;
  for (int y = rect.y; y < rect.y + rect.h; ++y) {
    for (int x = rect.x; x < rect.x + rect.w; ++x) {
      ftxui::Cell &pixel = screen_.PixelAt(x, y);
      pixel.character = " ";
      pixel.background_color = bg;
    }
  }
}

void ScreenGrid::setGlyph(int x, int y, const std::string &glyph, ftxui::Color fg, bool bold, bool underline, bool inverted, bool strikethrough) {
  if (!visible(x, y)) return;
  ftxui::Cell &pixel = screen_.PixelAt(x, y);
  pixel.character = glyph;
  pixel.foreground_color = fg;
  pixel.bold = bold;
  pixel.underlined = underline;
  pixel.inverted = inverted;
  pixel.strikethrough = strikethrough;
}

void ScreenGrid::markWideContinuation(int x, int y) {
  if (!visible(x, y)) return;
  ftxui::Cell &pixel = screen_.PixelAt(x, y);
  pixel.character = "";
}

void ScreenGrid::invertCell(int x, int y) {
  if (!visible(x, y)) return;
  ftxui::Cell &pixel = screen_.PixelAt(x, y);
  pixel.inverted = !pixel.inverted;
}

std::string ScreenGrid::render() const { return screen_.ToString(); }

} // namespace n8v::detail
