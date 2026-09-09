#include "sdl2_backend_impl.hpp"

#include "core/native_widget_meta.hpp"

#include <SDL2/SDL.h>

#include <algorithm>
#include <cmath>

namespace n8v::detail {

void Sdl2Backend::renderCheckboxOrRadioIndicator(NativeWidgetMeta &meta, const Clay_BoundingBox &labelBox, bool isRadio) {
  float squareSize = meta.indicatorSize > 0.0f ? meta.indicatorSize : labelBox.height;
  float gap = squareSize * 0.4f;
  Clay_BoundingBox squareBox{labelBox.x - squareSize - gap, labelBox.y, squareSize, squareSize};
  Clay_CornerRadius indicatorRadius = isRadio
                                        ? Clay_CornerRadius{squareSize / 2, squareSize / 2, squareSize / 2, squareSize / 2}
                                        : Clay_CornerRadius{meta.indicatorCornerRadius, meta.indicatorCornerRadius, meta.indicatorCornerRadius, meta.indicatorCornerRadius};
  Clay_Color fill{meta.indicatorFillColor.r, meta.indicatorFillColor.g, meta.indicatorFillColor.b, meta.indicatorFillColor.a};
  if (fill.a > 0.0f) drawRoundedRect(squareBox, fill, indicatorRadius);
  if (meta.indicatorBorderWidth > 0.0f) {
    Clay_Color border{meta.indicatorBorderColor.r, meta.indicatorBorderColor.g, meta.indicatorBorderColor.b, meta.indicatorBorderColor.a};
    drawRoundedRectBorder(squareBox, border, indicatorRadius, meta.indicatorBorderWidth);
  }
  Clay_Color glyph{meta.indicatorGlyphColor.r, meta.indicatorGlyphColor.g, meta.indicatorGlyphColor.b, meta.indicatorGlyphColor.a};
  if (isRadio) {
    if (meta.indicatorGlyphScale > 0.01f) drawRadioDot(squareBox, glyph, meta.indicatorGlyphScale);
  } else if (glyph.a > 0.5f) {
    drawCheckmark(squareBox, glyph);
  }
}

void Sdl2Backend::drawCheckmark(const Clay_BoundingBox &box, const Clay_Color &color) {
  float thickness = std::max(box.width * 0.12f, 1.5f);
  float x0 = box.x + box.width * 0.15f, y0 = box.y + box.height * 0.45f;
  float xm = box.x + box.width * 0.4f, ym = box.y + box.height * 0.7f;
  float x1 = box.x + box.width * 0.85f, y1 = box.y + box.height * 0.25f;
  drawThickLine(x0, y0, xm, ym, thickness, color);
  drawThickLine(xm, ym, x1, y1, thickness, color);
  drawStrokeCap(x0, y0, thickness, color);
  drawStrokeCap(xm, ym, thickness, color);
  drawStrokeCap(x1, y1, thickness, color);
}

void Sdl2Backend::drawRadioDot(const Clay_BoundingBox &box, const Clay_Color &color, float scale) {
  float dotSize = box.width * 0.5625f * std::clamp(scale, 0.0f, 1.0f);
  Clay_BoundingBox dotBox{box.x + (box.width - dotSize) * 0.5f, box.y + (box.height - dotSize) * 0.5f, dotSize, dotSize};
  drawRoundedRect(dotBox, color, {dotSize * 0.5f, dotSize * 0.5f, dotSize * 0.5f, dotSize * 0.5f});
}

void Sdl2Backend::drawDropdownChevron(const Clay_BoundingBox &box, const Clay_Color &color, bool pointsUp) {
  SDL_Color tint{(Uint8)color.r, (Uint8)color.g, (Uint8)color.b, (Uint8)color.a};
  float cx = box.x + box.width * 0.5f;
  float halfW = box.width * 0.3f;
  float top = box.y + box.height * 0.35f, bottom = box.y + box.height * 0.65f;
  SDL_FPoint apex{cx, pointsUp ? top : bottom};
  SDL_FPoint left{cx - halfW, pointsUp ? bottom : top};
  SDL_FPoint right{cx + halfW, pointsUp ? bottom : top};
  SDL_Vertex verts[3] = {{apex, tint, {0, 0}}, {left, tint, {0, 0}}, {right, tint, {0, 0}}};
  SDL_RenderGeometry(renderer_, nullptr, verts, 3, nullptr, 0);
}

} // namespace n8v::detail
