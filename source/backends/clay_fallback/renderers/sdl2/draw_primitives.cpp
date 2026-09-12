#include "sdl2_backend_impl.hpp"
#include "text_edit_utils.hpp"

#include "backends/clay_fallback/text/line_layout.hpp"
#include "core/text_style_flags.hpp"

#include <SDL2/SDL.h>

#include <algorithm>
#include <cmath>
#include <string_view>
#include <vector>

namespace n8v::detail {

size_t Sdl2Backend::hitTestOffset(std::string_view text, FontFamily family, uint16_t fontSize, float localX, bool bold, bool italic) const {
  if (text.empty()) return 0;
  LineLayoutResult layout;
  if (!layoutLine(text, family, fontSize, bold, italic, layout)) return text.size();
  std::vector<size_t> offsets = codepointByteOffsets(text);
  size_t count = offsets.size() - 1;
  if (count == 0 || layout.caretX.size() != count) return text.size();

  float bestDist = std::abs(localX);
  size_t bestIndex = 0;
  for (size_t k = 1; k <= count; ++k) {
    float dist = std::abs(localX - layout.caretX[k - 1]);
    if (dist < bestDist) {
      bestDist = dist;
      bestIndex = k;
    }
  }
  return offsets[bestIndex];
}

float Sdl2Backend::caretPixelX(std::string_view text, FontFamily family, uint16_t fontSize, size_t byteOffset, bool bold, bool italic) const {
  if (byteOffset == 0 || text.empty()) return 0.0f;
  LineLayoutResult layout;
  if (!layoutLine(text, family, fontSize, bold, italic, layout)) return 0.0f;
  std::vector<size_t> offsets = codepointByteOffsets(text);
  for (size_t k = 0; k < offsets.size(); ++k) {
    if (offsets[k] == byteOffset) return k == 0 ? 0.0f : layout.caretX[k - 1];
  }
  return layout.width;
}

void Sdl2Backend::drawCursorCaret(const Clay_BoundingBox &textBox, std::string_view value, FontFamily family, uint16_t fontSize, size_t byteOffset, bool bold, bool italic) {
  float x = std::round(textBox.x + caretPixelX(value, family, fontSize, byteOffset, bold, italic));
  int y0 = (int)textBox.y, y1 = (int)(textBox.y + textBox.height);
  SDL_SetRenderDrawColor(renderer_, 20, 20, 20, 255);
  SDL_RenderDrawLine(renderer_, (int)x, y0, (int)x, y1);
}

void Sdl2Backend::drawSelectionHighlight(
  const Clay_BoundingBox &textBox, std::string_view value, FontFamily family, uint16_t fontSize, size_t selStart, size_t selEnd, bool bold, bool italic
) {
  float x0 = caretPixelX(value, family, fontSize, selStart, bold, italic);
  float x1 = caretPixelX(value, family, fontSize, selEnd, bold, italic);
  Clay_BoundingBox box{textBox.x + x0, textBox.y, x1 - x0, textBox.height};
  drawFilledRect(box, Clay_Color{50, 100, 220, 255});
}

void Sdl2Backend::uploadAtlas(FontGeneration &gen) {
  std::vector<uint8_t> rgba((size_t)gen.atlasWidth * (size_t)gen.atlasHeight * 4);
  for (size_t i = 0; i < gen.pixels.size(); ++i) {
    rgba[i * 4 + 0] = 255;
    rgba[i * 4 + 1] = 255;
    rgba[i * 4 + 2] = 255;
    rgba[i * 4 + 3] = gen.pixels[i];
  }

  if (gen.backendHandle) {
    SDL_DestroyTexture((SDL_Texture *)gen.backendHandle);
    gen.backendHandle = nullptr;
  }

  SDL_Texture *texture = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, gen.atlasWidth, gen.atlasHeight);
  if (texture) {
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_UpdateTexture(texture, nullptr, rgba.data(), gen.atlasWidth * 4);
  }

  gen.backendHandle = texture;
  gen.destroyBackendHandle = [](void *handle) { SDL_DestroyTexture((SDL_Texture *)handle); };
  gen.dirty = false;
}

void Sdl2Backend::drawText(const Clay_RenderCommand &command, size_t selStartByte, size_t selEndByte, SDL_Color selectedTint) {
  const Clay_TextRenderData &textData = command.renderData.text;
  auto *flags = static_cast<TextStyleFlags *>(command.userData);
  FontFamily family = flags ? flags->font : FontFamily::DejaVuSans;
  bool bold = flags && flags->bold;
  bool italic = flags && flags->italic;
  bool underline = flags && flags->underline;

  LineLayoutResult layout;
  std::string_view text(textData.stringContents.chars, (size_t)textData.stringContents.length);
  if (!layoutLine(text, family, textData.fontSize, bold, italic, layout) || !layout.generation) {
    Clay_Color block = textData.textColor;
    block.a = 160;
    drawFilledRect(command.boundingBox, block);
    return;
  }

  if (layout.generation->dirty) uploadAtlas(*layout.generation);
  auto *texture = (SDL_Texture *)layout.generation->backendHandle;
  if (!texture) return;

  SDL_Color tint{(Uint8)textData.textColor.r, (Uint8)textData.textColor.g, (Uint8)textData.textColor.b, (Uint8)textData.textColor.a};

  bool hasSelRange = selStartByte != SIZE_MAX && selEndByte > selStartByte;
  size_t selStartCp = SIZE_MAX, selEndCp = SIZE_MAX;
  if (hasSelRange) {
    std::vector<size_t> offsets = codepointByteOffsets(text);
    for (size_t k = 0; k < offsets.size(); ++k) {
      if (offsets[k] == selStartByte) selStartCp = k;
      if (offsets[k] == selEndByte) selEndCp = k;
    }
  }

  std::vector<SDL_Vertex> vertices;
  std::vector<int> indices;
  vertices.reserve(layout.quads.size() * 4);
  indices.reserve(layout.quads.size() * 6);

  float originX = std::round(command.boundingBox.x);
  float baselineY = std::round(command.boundingBox.y + layout.generation->ascent * layout.scale);

  for (const GlyphQuad &quad : layout.quads) {
    float x0 = originX + quad.x0 * layout.scale;
    float y0 = baselineY + quad.y0 * layout.scale;
    float x1 = originX + quad.x1 * layout.scale;
    float y1 = baselineY + quad.y1 * layout.scale;

    SDL_Color glyphTint = (hasSelRange && quad.codepointIndex >= selStartCp && quad.codepointIndex < selEndCp) ? selectedTint : tint;

    int base = (int)vertices.size();
    vertices.push_back({{x0, y0}, glyphTint, {quad.s0, quad.t0}});
    vertices.push_back({{x1, y0}, glyphTint, {quad.s1, quad.t0}});
    vertices.push_back({{x1, y1}, glyphTint, {quad.s1, quad.t1}});
    vertices.push_back({{x0, y1}, glyphTint, {quad.s0, quad.t1}});
    indices.insert(indices.end(), {base, base + 1, base + 2, base + 2, base + 3, base});
  }

  if (!vertices.empty()) {
    SDL_RenderGeometry(renderer_, texture, vertices.data(), (int)vertices.size(), indices.data(), (int)indices.size());
  }

  if (underline) {
    SDL_SetRenderDrawColor(renderer_, tint.r, tint.g, tint.b, tint.a);
    int underlineY = (int)std::round(command.boundingBox.y + command.boundingBox.height - 1.0f);
    SDL_RenderDrawLine(renderer_, (int)originX, underlineY, (int)std::round(originX + layout.width), underlineY);
  }
}

void Sdl2Backend::drawFocusRing(const Clay_BoundingBox &box, const Clay_CornerRadius &cornerRadius) {
  drawRoundedRectBorder(box, Clay_Color{60, 110, 220, 255}, cornerRadius, 2.0f);
}

void Sdl2Backend::drawThickLine(float x0, float y0, float x1, float y1, float thickness, const Clay_Color &color) {
  float dx = x1 - x0, dy = y1 - y0;
  float len = std::sqrt(dx * dx + dy * dy);
  if (len < 0.0001f) return;
  float nx = -dy / len * thickness * 0.5f, ny = dx / len * thickness * 0.5f;
  SDL_Color tint{(Uint8)color.r, (Uint8)color.g, (Uint8)color.b, (Uint8)color.a};
  SDL_Vertex vertices[4] = {
    {{x0 + nx, y0 + ny}, tint, {0, 0}},
    {{x1 + nx, y1 + ny}, tint, {0, 0}},
    {{x1 - nx, y1 - ny}, tint, {0, 0}},
    {{x0 - nx, y0 - ny}, tint, {0, 0}},
  };
  int indices[6] = {0, 1, 2, 0, 2, 3};
  SDL_RenderGeometry(renderer_, nullptr, vertices, 4, indices, 6);
}

void Sdl2Backend::drawStrokeCap(float cx, float cy, float thickness, const Clay_Color &color) {
  float r = thickness * 0.5f;
  drawRoundedRect({cx - r, cy - r, thickness, thickness}, color, {r, r, r, r});
}

void Sdl2Backend::drawFilledRect(const Clay_BoundingBox &box, const Clay_Color &color) {
  SDL_SetRenderDrawColor(renderer_, (Uint8)color.r, (Uint8)color.g, (Uint8)color.b, (Uint8)color.a);
  SDL_Rect rect{(int)box.x, (int)box.y, (int)box.width, (int)box.height};
  SDL_RenderFillRect(renderer_, &rect);
}

void Sdl2Backend::appendArc(std::vector<SDL_Vertex> &vertices, const SDL_Color &tint, float cx, float cy, float radius, float startAngle, float endAngle) {
  int segments = std::clamp((int)(radius * 0.6f) + 2, 3, 20);
  for (int i = 0; i <= segments; ++i) {
    float angle = startAngle + (endAngle - startAngle) * ((float)i / (float)segments);
    vertices.push_back({{cx + std::cos(angle) * radius, cy + std::sin(angle) * radius}, tint, {0, 0}});
  }
}

void Sdl2Backend::appendArcN(std::vector<SDL_FPoint> &points, float cx, float cy, float radius, float startAngle, float endAngle, int segments) {
  for (int i = 0; i <= segments; ++i) {
    float angle = startAngle + (endAngle - startAngle) * ((float)i / (float)segments);
    points.push_back({cx + std::cos(angle) * radius, cy + std::sin(angle) * radius});
  }
}

void Sdl2Backend::drawRoundedRectBorder(const Clay_BoundingBox &box, const Clay_Color &color, const Clay_CornerRadius &corner, float strokeWidth) {
  if (strokeWidth <= 0.0f || box.width <= 0.0f || box.height <= 0.0f) return;
  float limit = std::min(box.width, box.height) * 0.5f;
  float sw = std::min(strokeWidth, limit);
  float tl = std::clamp(corner.topLeft, 0.0f, limit);
  float tr = std::clamp(corner.topRight, 0.0f, limit);
  float br = std::clamp(corner.bottomRight, 0.0f, limit);
  float bl = std::clamp(corner.bottomLeft, 0.0f, limit);
  float itl = std::max(tl - sw, 0.0f), itr = std::max(tr - sw, 0.0f);
  float ibr = std::max(br - sw, 0.0f), ibl = std::max(bl - sw, 0.0f);

  const float pi = 3.14159265358979323846f;
  int segTl = std::clamp((int)(tl * 0.6f) + 2, 3, 20);
  int segTr = std::clamp((int)(tr * 0.6f) + 2, 3, 20);
  int segBr = std::clamp((int)(br * 0.6f) + 2, 3, 20);
  int segBl = std::clamp((int)(bl * 0.6f) + 2, 3, 20);

  std::vector<SDL_FPoint> outer, inner;
  appendArcN(outer, box.x + tl, box.y + tl, tl, pi, pi * 1.5f, segTl);
  appendArcN(outer, box.x + box.width - tr, box.y + tr, tr, pi * 1.5f, pi * 2.0f, segTr);
  appendArcN(outer, box.x + box.width - br, box.y + box.height - br, br, 0.0f, pi * 0.5f, segBr);
  appendArcN(outer, box.x + bl, box.y + box.height - bl, bl, pi * 0.5f, pi, segBl);

  appendArcN(inner, box.x + sw + itl, box.y + sw + itl, itl, pi, pi * 1.5f, segTl);
  appendArcN(inner, box.x + box.width - sw - itr, box.y + sw + itr, itr, pi * 1.5f, pi * 2.0f, segTr);
  appendArcN(inner, box.x + box.width - sw - ibr, box.y + box.height - sw - ibr, ibr, 0.0f, pi * 0.5f, segBr);
  appendArcN(inner, box.x + sw + ibl, box.y + box.height - sw - ibl, ibl, pi * 0.5f, pi, segBl);

  size_t n = outer.size();
  if (n != inner.size() || n < 2) return;

  SDL_Color tint{(Uint8)color.r, (Uint8)color.g, (Uint8)color.b, (Uint8)color.a};
  std::vector<SDL_Vertex> vertices;
  vertices.reserve(n * 2);
  for (size_t i = 0; i < n; ++i) vertices.push_back({outer[i], tint, {0, 0}});
  for (size_t i = 0; i < n; ++i) vertices.push_back({inner[i], tint, {0, 0}});

  std::vector<int> indices;
  indices.reserve(n * 6);
  for (size_t i = 0; i < n; ++i) {
    size_t j = (i + 1) % n;
    int o0 = (int)i, o1 = (int)j, i0 = (int)(n + i), i1 = (int)(n + j);
    indices.push_back(o0);
    indices.push_back(o1);
    indices.push_back(i0);
    indices.push_back(i0);
    indices.push_back(o1);
    indices.push_back(i1);
  }

  SDL_RenderGeometry(renderer_, nullptr, vertices.data(), (int)vertices.size(), indices.data(), (int)indices.size());
}

void Sdl2Backend::drawRoundedRect(const Clay_BoundingBox &box, const Clay_Color &color, const Clay_CornerRadius &corner) {
  float limit = std::min(box.width, box.height) * 0.5f;
  float tl = std::clamp(corner.topLeft, 0.0f, limit);
  float tr = std::clamp(corner.topRight, 0.0f, limit);
  float br = std::clamp(corner.bottomRight, 0.0f, limit);
  float bl = std::clamp(corner.bottomLeft, 0.0f, limit);

  if (tl <= 0.5f && tr <= 0.5f && br <= 0.5f && bl <= 0.5f) {
    drawFilledRect(box, color);
    return;
  }

  const float pi = 3.14159265358979323846f;
  SDL_Color tint{(Uint8)color.r, (Uint8)color.g, (Uint8)color.b, (Uint8)color.a};

  std::vector<SDL_Vertex> vertices;
  vertices.push_back({{box.x + box.width * 0.5f, box.y + box.height * 0.5f}, tint, {0, 0}});

  appendArc(vertices, tint, box.x + tl, box.y + tl, tl, pi, pi * 1.5f);
  appendArc(vertices, tint, box.x + box.width - tr, box.y + tr, tr, pi * 1.5f, pi * 2.0f);
  appendArc(vertices, tint, box.x + box.width - br, box.y + box.height - br, br, 0.0f, pi * 0.5f);
  appendArc(vertices, tint, box.x + bl, box.y + box.height - bl, bl, pi * 0.5f, pi);

  int perimeter = (int)vertices.size() - 1;
  std::vector<int> indices;
  indices.reserve((size_t)perimeter * 3);
  for (int i = 0; i < perimeter; ++i) {
    indices.push_back(0);
    indices.push_back(1 + i);
    indices.push_back(1 + (i + 1) % perimeter);
  }

  SDL_RenderGeometry(renderer_, nullptr, vertices.data(), (int)vertices.size(), indices.data(), (int)indices.size());
}

} // namespace n8v::detail
