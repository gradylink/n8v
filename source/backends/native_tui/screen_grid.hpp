#pragma once

#include <ftxui/screen/color.hpp>
#include <ftxui/screen/screen.hpp>

#include <string>
#include <vector>

namespace n8v::detail {

struct CellRect {
  int x = 0, y = 0, w = 0, h = 0;
};

class ScreenGrid {
public:
  void resize(int cols, int rows);
  int cols() const { return cols_; }
  int rows() const { return rows_; }

  void pushClip(CellRect rect);
  void popClip();

  bool clip(CellRect &rect) const;
  bool visible(int x, int y) const;

  void fillBackground(CellRect rect, ftxui::Color bg);
  void setGlyph(int x, int y, const std::string &glyph, ftxui::Color fg, bool bold = false, bool underline = false, bool inverted = false, bool strikethrough = false);
  void markWideContinuation(int x, int y);
  void invertCell(int x, int y);

  std::string render() const;

private:
  int cols_ = 0, rows_ = 0;
  ftxui::Screen screen_ = ftxui::Screen::Create(ftxui::Dimension::Fixed(0), ftxui::Dimension::Fixed(0));
  std::vector<CellRect> clipStack_;
};

} // namespace n8v::detail
