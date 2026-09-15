#include "sdl2_backend_impl.hpp"

#include "core/icon_loader.hpp"
#include "core/native_widget_meta.hpp"

#include <SDL2/SDL.h>

#include <algorithm>
#include <cmath>

namespace n8v::detail {

void Sdl2Backend::drawIconGlyph(const Clay_BoundingBox &box, const char *iconName, const Clay_Color &color) {
  uint16_t pixelSize = (uint16_t)std::max(1.0f, std::round(std::min(box.width, box.height)));
  const DecodedImage *decoded = getOrDecodeIcon(iconName, pixelSize, n8v::Color{255, 255, 255, 255});
  if (!decoded) return;

  SDL_Texture *&texture = imageTextures_[decoded];
  if (!texture) {
    texture = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, decoded->width, decoded->height);
    if (!texture) return;
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_UpdateTexture(texture, nullptr, decoded->rgba, decoded->width * 4);
  }

  SDL_SetTextureColorMod(texture, (Uint8)color.r, (Uint8)color.g, (Uint8)color.b);
  SDL_SetTextureAlphaMod(texture, (Uint8)color.a);
  float w = (float)decoded->width, h = (float)decoded->height;
  SDL_Rect dest{(int)(box.x + (box.width - w) * 0.5f), (int)(box.y + (box.height - h) * 0.5f), (int)w, (int)h};
  SDL_RenderCopy(renderer_, texture, nullptr, &dest);
  SDL_SetTextureColorMod(texture, 255, 255, 255);
  SDL_SetTextureAlphaMod(texture, 255);
}

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
    drawIconGlyph(squareBox, "check", glyph);
  }
}

void Sdl2Backend::drawRadioDot(const Clay_BoundingBox &box, const Clay_Color &color, float scale) {
  float dotSize = box.width * 0.5625f * std::clamp(scale, 0.0f, 1.0f);
  Clay_BoundingBox dotBox{box.x + (box.width - dotSize) * 0.5f, box.y + (box.height - dotSize) * 0.5f, dotSize, dotSize};
  drawRoundedRect(dotBox, color, {dotSize * 0.5f, dotSize * 0.5f, dotSize * 0.5f, dotSize * 0.5f});
}

void Sdl2Backend::renderSwitchIndicator(NativeWidgetMeta &meta, const Clay_BoundingBox &labelBox) {
  float trackW = meta.switchTrackWidth, trackH = meta.switchTrackHeight;
  float gap = trackH * 0.5f;
  Clay_BoundingBox trackBox{labelBox.x - trackW - gap, labelBox.y + (labelBox.height - trackH) * 0.5f, trackW, trackH};
  Clay_CornerRadius pillRadius{trackH / 2, trackH / 2, trackH / 2, trackH / 2};

  Clay_Color trackColor{meta.switchTrackColor.r, meta.switchTrackColor.g, meta.switchTrackColor.b, meta.switchTrackColor.a};
  if (trackColor.a > 0.0f) drawRoundedRect(trackBox, trackColor, pillRadius);
  if (meta.switchTrackBorderWidth > 0.0f) {
    Clay_Color borderColor{meta.switchTrackBorderColor.r, meta.switchTrackBorderColor.g, meta.switchTrackBorderColor.b, meta.switchTrackBorderColor.a};
    drawRoundedRectBorder(trackBox, borderColor, pillRadius, meta.switchTrackBorderWidth);
  }

  float knobInset = std::max((trackH - meta.switchKnobSize) * 0.5f, 0.0f);
  float leftX = meta.switchKnobSize * 0.5f + knobInset;
  float rightX = trackW - meta.switchKnobSize * 0.5f - knobInset;
  float knobCx = trackBox.x + leftX + (rightX - leftX) * std::clamp(meta.switchKnobPosition, 0.0f, 1.0f);
  float knobCy = trackBox.y + trackH * 0.5f;
  Clay_BoundingBox knobBox{knobCx - meta.switchKnobSize * 0.5f, knobCy - meta.switchKnobSize * 0.5f, meta.switchKnobSize, meta.switchKnobSize};
  Clay_Color knobColor{meta.switchKnobColor.r, meta.switchKnobColor.g, meta.switchKnobColor.b, meta.switchKnobColor.a};
  drawRoundedRect(knobBox, knobColor, {meta.switchKnobSize / 2, meta.switchKnobSize / 2, meta.switchKnobSize / 2, meta.switchKnobSize / 2});

  if (meta.switchGlyphScale > 0.01f) {
    Clay_Color glyphColor{meta.switchKnobGlyphColor.r, meta.switchKnobGlyphColor.g, meta.switchKnobGlyphColor.b, meta.switchKnobGlyphColor.a * meta.switchGlyphScale};
    drawIconGlyph(knobBox, "check", glyphColor);
  }
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
