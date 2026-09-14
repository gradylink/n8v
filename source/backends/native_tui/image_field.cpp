#include "color.hpp"
#include "kitty_graphics.hpp"
#include "nerd_font_icons.hpp"
#include "tui_backend_impl.hpp"

#include "core/image_loader.hpp"
#include "core/native_widget_meta.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace n8v::detail {

namespace {

constexpr float physicalCellAspect = 2.0f;

CellRect containFit(CellRect bounds, int imgW, int imgH) {
  if (imgW <= 0 || imgH <= 0 || bounds.w <= 0 || bounds.h <= 0) return bounds;
  int idealRows = std::max(1, (int)std::lround((float)bounds.w * ((float)imgH / (float)imgW) / physicalCellAspect));
  if (idealRows <= bounds.h) return {bounds.x, bounds.y, bounds.w, idealRows};
  int cols = std::max(1, (int)std::lround((float)bounds.h * physicalCellAspect * ((float)imgW / (float)imgH)));
  return {bounds.x, bounds.y, cols, bounds.h};
}

const DecodedImage *whiteBackedRounded(const DecodedImage *alphaBaked) {
  static std::unordered_map<const DecodedImage *, DecodedImage> cache;
  auto it = cache.find(alphaBaked);
  if (it != cache.end()) return &it->second;

  DecodedImage out;
  size_t n = (size_t)alphaBaked->width * (size_t)alphaBaked->height * 4;
  out.owned.resize(n);
  for (size_t i = 0; i < n; i += 4) {
    uint32_t a = alphaBaked->rgba[i + 3];
    out.owned[i + 0] = (uint8_t)(((uint32_t)alphaBaked->rgba[i + 0] * a + 255u * (255u - a)) / 255u);
    out.owned[i + 1] = (uint8_t)(((uint32_t)alphaBaked->rgba[i + 1] * a + 255u * (255u - a)) / 255u);
    out.owned[i + 2] = (uint8_t)(((uint32_t)alphaBaked->rgba[i + 2] * a + 255u * (255u - a)) / 255u);
    out.owned[i + 3] = 255;
  }
  out.rgba = out.owned.data();
  out.width = alphaBaked->width;
  out.height = alphaBaked->height;
  return &(cache[alphaBaked] = std::move(out));
}

ftxui::Color averageBlock(const DecodedImage &img, int x0, int x1, int y0, int y1) {
  x0 = std::clamp(x0, 0, img.width);
  x1 = std::clamp(x1, x0 + 1, img.width);
  y0 = std::clamp(y0, 0, img.height);
  y1 = std::clamp(y1, y0 + 1, img.height);
  uint64_t r = 0, g = 0, b = 0, count = 0;
  for (int y = y0; y < y1; ++y) {
    for (int x = x0; x < x1; ++x) {
      const uint8_t *p = img.rgba + ((size_t)y * (size_t)img.width + (size_t)x) * 4;
      uint32_t a = p[3];
      r += (uint32_t)p[0] * a + 255u * (255u - a);
      g += (uint32_t)p[1] * a + 255u * (255u - a);
      b += (uint32_t)p[2] * a + 255u * (255u - a);
      ++count;
    }
  }
  if (count == 0) return ftxui::Color::Default;
  return ftxui::Color::RGB((uint8_t)(r / (count * 255)), (uint8_t)(g / (count * 255)), (uint8_t)(b / (count * 255)));
}

} // namespace

void TuiBackend::drawImage(NativeWidgetMeta &meta, const Clay_BoundingBox &box, const Clay_CornerRadius &corner) {
  if (!meta.image) return;
  CellRect rect = cellRect(box);
  if (!grid_.clip(rect) || rect.w <= 0 || rect.h <= 0) return;
  const DecodedImage *imgPtr = meta.image;
  if (imgPtr->width <= 0 || imgPtr->height <= 0) return;

  if (corner.topLeft > 0.0f || corner.topRight > 0.0f || corner.bottomLeft > 0.0f || corner.bottomRight > 0.0f) {
    float scale = (float)imgPtr->width / std::max(box.width, 1.0f);
    const DecodedImage *alphaBaked =
      getOrBakeRoundedImage(imgPtr, imgPtr->width, imgPtr->height, corner.topLeft * scale, corner.topRight * scale, corner.bottomLeft * scale, corner.bottomRight * scale);
    imgPtr = whiteBackedRounded(alphaBaked);
  }
  const DecodedImage &img = *imgPtr;
  rect = containFit(rect, img.width, img.height);

  if (kittySupported_) {
    KittyImageState &state = kittyImages_[imgPtr];

    if (openDropdownPopupOverlapsRows(rect.y, rect.h)) {
      if (state.visible) {
        pendingImageEscapes_ += kittyDelete(state.id);
        state.visible = false;
      }
      return;
    }

    grid_.fillBackground(rect, ftxui::Color::Default);

    pendingImageEscapes_ += kittyMoveCursor(rect.x, rect.y);
    if (!state.transmitted) {
      state.id = nextKittyImageId_++;
      pendingImageEscapes_ += kittyTransmitAndPlace(img, state.id, rect.w, rect.h);
      state.transmitted = true;
    } else {
      pendingImageEscapes_ += kittyPut(state.id, rect.w, rect.h);
    }
    state.visible = true;
    return;
  }

  for (int cy = 0; cy < rect.h; ++cy) {
    int y0 = img.height * (2 * cy) / (2 * rect.h);
    int yMid = img.height * (2 * cy + 1) / (2 * rect.h);
    int y1 = img.height * (2 * cy + 2) / (2 * rect.h);
    for (int cx = 0; cx < rect.w; ++cx) {
      int x0 = img.width * cx / rect.w;
      int x1 = img.width * (cx + 1) / rect.w;
      ftxui::Color top = averageBlock(img, x0, x1, y0, yMid);
      ftxui::Color bottom = averageBlock(img, x0, x1, yMid, y1);
      grid_.fillBackground({rect.x + cx, rect.y + cy, 1, 1}, bottom);
      grid_.setGlyph(rect.x + cx, rect.y + cy, "▀", top);
    }
  }
}

bool TuiBackend::drawIconGlyph(NativeWidgetMeta &meta, const Clay_BoundingBox &box) {
  if (!meta.iconName) return false;
  std::string glyph = nerdFontGlyph(meta.iconName);
  if (glyph.empty()) return false;

  CellRect rect = cellRect(box);
  if (!grid_.clip(rect) || rect.w <= 0 || rect.h <= 0) return true;
  int cx = rect.x + rect.w / 2;
  int cy = rect.y + rect.h / 2;
  Clay_Color tint{meta.iconTint.r, meta.iconTint.g, meta.iconTint.b, meta.iconTint.a};
  ftxui::Color fg = tint.a > 0.0f ? toFtxuiColor(tint) : ftxui::Color::RGB(0, 0, 0);
  grid_.setGlyph(cx, cy, glyph, fg, true);
  return true;
}

} // namespace n8v::detail
