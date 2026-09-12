#include "sdl2_backend_impl.hpp"

#include "core/image_loader.hpp"
#include "core/native_widget_meta.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace n8v::detail {

void Sdl2Backend::drawImage(NativeWidgetMeta &meta, const Clay_BoundingBox &box, const Clay_CornerRadius &corner) {
  if (!meta.image) return;

  SDL_Texture *&texture = imageTextures_[meta.image];
  if (!texture) {
    texture = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, meta.image->width, meta.image->height);
    if (!texture) return;
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_UpdateTexture(texture, nullptr, meta.image->rgba, meta.image->width * 4);
  }

  float limit = std::min(box.width, box.height) * 0.5f;
  float tl = std::clamp(corner.topLeft, 0.0f, limit);
  float tr = std::clamp(corner.topRight, 0.0f, limit);
  float br = std::clamp(corner.bottomRight, 0.0f, limit);
  float bl = std::clamp(corner.bottomLeft, 0.0f, limit);

  if (tl <= 0.5f && tr <= 0.5f && br <= 0.5f && bl <= 0.5f) {
    SDL_Rect dest{(int)box.x, (int)box.y, (int)box.width, (int)box.height};
    SDL_RenderCopy(renderer_, texture, nullptr, &dest);
    return;
  }

  const float pi = 3.14159265358979323846f;
  const SDL_Color white{255, 255, 255, 255};
  auto toUV = [&](float x, float y) { return SDL_FPoint{(x - box.x) / box.width, (y - box.y) / box.height}; };

  std::vector<SDL_Vertex> vertices;
  float ccx = box.x + box.width * 0.5f, ccy = box.y + box.height * 0.5f;
  vertices.push_back({{ccx, ccy}, white, toUV(ccx, ccy)});

  auto appendArcUV = [&](float cx, float cy, float radius, float startAngle, float endAngle) {
    int segments = std::clamp((int)(radius * 0.6f) + 2, 3, 20);
    for (int i = 0; i <= segments; ++i) {
      float angle = startAngle + (endAngle - startAngle) * ((float)i / (float)segments);
      float px = cx + std::cos(angle) * radius;
      float py = cy + std::sin(angle) * radius;
      vertices.push_back({{px, py}, white, toUV(px, py)});
    }
  };

  appendArcUV(box.x + tl, box.y + tl, tl, pi, pi * 1.5f);
  appendArcUV(box.x + box.width - tr, box.y + tr, tr, pi * 1.5f, pi * 2.0f);
  appendArcUV(box.x + box.width - br, box.y + box.height - br, br, 0.0f, pi * 0.5f);
  appendArcUV(box.x + bl, box.y + box.height - bl, bl, pi * 0.5f, pi);

  int perimeter = (int)vertices.size() - 1;
  std::vector<int> indices;
  indices.reserve((size_t)perimeter * 3);
  for (int i = 0; i < perimeter; ++i) {
    indices.push_back(0);
    indices.push_back(1 + i);
    indices.push_back(1 + (i + 1) % perimeter);
  }

  SDL_RenderGeometry(renderer_, texture, vertices.data(), (int)vertices.size(), indices.data(), (int)indices.size());
}

} // namespace n8v::detail
