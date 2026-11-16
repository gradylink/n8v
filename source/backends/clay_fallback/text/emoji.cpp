#include "emoji.hpp"

#include <stb_truetype.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <unordered_map>

#ifdef N8V_HAVE_BUNDLED_EMOJI
#include "bundle.hpp"
#include "bundle_n8v_emoji.h"
#endif

namespace n8v::detail {
namespace {

struct Layer {
  uint16_t glyph;
  uint16_t palette;
};

uint16_t be16(const unsigned char *p) { return (uint16_t)(p[0] << 8 | p[1]); }
uint32_t be32(const unsigned char *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }

class EmojiFont {
public:
  EmojiFont() {
#ifdef N8V_HAVE_BUNDLED_EMOJI
    static const bundle::archive archive(bundle_n8v_emoji_archive());
    const auto entry = archive.find("Twemoji.Mozilla.ttf");
    if (!entry) return;
    const std::vector<std::byte> bytes = archive.load(*entry);
    data_.assign(reinterpret_cast<const unsigned char *>(bytes.data()), reinterpret_cast<const unsigned char *>(bytes.data()) + bytes.size());
    const int offset = stbtt_GetFontOffsetForIndex(data_.data(), 0);
    if (offset < 0 || !stbtt_InitFont(&info_, data_.data(), offset)) return;
    parseTables(offset);
#endif
  }

  bool ok() const { return ok_; }

  const std::vector<Layer> *layersFor(uint32_t codepoint, int &glyphOut) const {
    glyphOut = stbtt_FindGlyphIndex(&info_, (int)codepoint);
    if (glyphOut <= 0) return nullptr;
    auto it = layers_.find((uint16_t)glyphOut);
    return it == layers_.end() ? nullptr : &it->second;
  }

  bool color(uint16_t paletteIndex, uint8_t out[4]) const {
    if (paletteIndex == 0xFFFF || (size_t)paletteIndex >= palette_.size()) {
      out[0] = out[1] = out[2] = 40;
      out[3] = 255;
      return false;
    }
    std::memcpy(out, palette_[paletteIndex].data(), 4);
    return true;
  }

  stbtt_fontinfo &info() { return info_; }
  float advanceEm() const { return advanceEm_; }

private:
  void parseTables(int offset) {
    const unsigned char *font = data_.data();
    const int tableCount = be16(font + offset + 4);
    const unsigned char *colr = nullptr, *cpal = nullptr;
    size_t colrSize = 0, cpalSize = 0;
    for (int i = 0; i < tableCount; ++i) {
      const unsigned char *record = font + offset + 12 + 16 * i;
      const uint32_t at = be32(record + 8), length = be32(record + 12);
      if (at + length > data_.size()) continue;
      if (std::memcmp(record, "COLR", 4) == 0) colr = font + at, colrSize = length;
      if (std::memcmp(record, "CPAL", 4) == 0) cpal = font + at, cpalSize = length;
    }
    if (!colr || !cpal || colrSize < 14 || cpalSize < 12) return;

    const uint16_t baseCount = be16(colr + 2);
    const uint32_t baseOffset = be32(colr + 4), layerOffset = be32(colr + 8);
    const uint16_t layerCount = be16(colr + 12);
    for (uint32_t i = 0; i < baseCount; ++i) {
      const unsigned char *base = colr + baseOffset + 6 * i;
      if ((size_t)(base - colr) + 6 > colrSize) break;
      const uint16_t glyph = be16(base), first = be16(base + 2), count = be16(base + 4);
      std::vector<Layer> stack;
      for (uint16_t l = 0; l < count && first + l < layerCount; ++l) {
        const unsigned char *layer = colr + layerOffset + 4 * (first + l);
        if ((size_t)(layer - colr) + 4 > colrSize) break;
        stack.push_back({be16(layer), be16(layer + 2)});
      }
      layers_[glyph] = std::move(stack);
    }

    const uint16_t colorCount = be16(cpal + 6);
    const uint32_t colorsOffset = be32(cpal + 8);
    for (uint16_t i = 0; i < colorCount; ++i) {
      const unsigned char *bgra = cpal + colorsOffset + 4 * i;
      if ((size_t)(bgra - cpal) + 4 > cpalSize) break;
      palette_.push_back({bgra[2], bgra[1], bgra[0], bgra[3]});
    }

    const float unitsPerEm = 1.0f / stbtt_ScaleForMappingEmToPixels(&info_, 1.0f);
    int advance = 0, lsb = 0;
    const int heart = stbtt_FindGlyphIndex(&info_, 0x1F600);
    stbtt_GetGlyphHMetrics(&info_, heart > 0 ? heart : 0, &advance, &lsb);
    advanceEm_ = advance > 0 ? (float)advance / unitsPerEm : 1.17f;
    ok_ = !layers_.empty() && !palette_.empty();
  }

  std::vector<unsigned char> data_;
  stbtt_fontinfo info_{};
  std::unordered_map<uint16_t, std::vector<Layer>> layers_;
  std::vector<std::array<uint8_t, 4>> palette_;
  float advanceEm_ = 1.17f;
  bool ok_ = false;
};

EmojiFont &font() {
  static EmojiFont instance;
  return instance;
}

} // namespace

bool isDrawableEmoji(uint32_t cp, uint32_t next) {
  if (!font().ok()) return false;
  bool candidate = false;
  if (cp >= 0x1F000 && cp <= 0x1FAFF) {
    candidate = !(cp >= 0x1F1E6 && cp <= 0x1F1FF);
  } else if (next == 0xFE0F) {
    candidate = cp == 0xA9 || cp == 0xAE || cp == 0x203C || cp == 0x2049 || cp == 0x2122 || cp == 0x2139 || (cp >= 0x2190 && cp <= 0x21FF) || (cp >= 0x2300 && cp <= 0x23FF) ||
                (cp >= 0x24C2 && cp <= 0x24C2) || (cp >= 0x25A0 && cp <= 0x25FF) || (cp >= 0x2600 && cp <= 0x27BF) || (cp >= 0x2900 && cp <= 0x297F) ||
                (cp >= 0x2B00 && cp <= 0x2BFF) || cp == 0x3030 || cp == 0x303D || cp == 0x3297 || cp == 0x3299;
  } else {
    static const uint32_t defaults[] = {0x231A, 0x231B, 0x23E9, 0x23EA, 0x23EB, 0x23EC, 0x23F0, 0x23F3, 0x25FD, 0x25FE, 0x2614, 0x2615, 0x267F, 0x2693, 0x26A1, 0x26AA,
                                        0x26AB, 0x26BD, 0x26BE, 0x26C4, 0x26C5, 0x26CE, 0x26D4, 0x26EA, 0x26F2, 0x26F3, 0x26F5, 0x26FA, 0x26FD, 0x2705, 0x270A, 0x270B,
                                        0x2728, 0x274C, 0x274E, 0x2753, 0x2754, 0x2755, 0x2757, 0x2795, 0x2796, 0x2797, 0x27B0, 0x27BF, 0x2B1B, 0x2B1C, 0x2B50, 0x2B55};
    candidate = (cp >= 0x2648 && cp <= 0x2653) || std::find(std::begin(defaults), std::end(defaults), cp) != std::end(defaults);
  }
  if (!candidate) return false;
  int glyph = 0;
  return font().layersFor(cp, glyph) != nullptr;
}

bool isEmojiModifier(uint32_t cp, bool afterEmoji) { return cp == 0xFE0F || cp == 0xFE0E || cp == 0x200D || (afterEmoji && cp >= 0x1F3FB && cp <= 0x1F3FF); }

float emojiAdvanceEm() { return font().advanceEm(); }

std::shared_ptr<const EmojiBitmap> emojiBitmap(uint32_t codepoint, int pixelSize) {
  static std::mutex mutex;
  static std::map<std::pair<uint32_t, int>, std::shared_ptr<const EmojiBitmap>> cache;
  std::lock_guard<std::mutex> lock(mutex);
  const auto key = std::make_pair(codepoint, pixelSize);
  if (auto it = cache.find(key); it != cache.end()) return it->second;

  std::shared_ptr<const EmojiBitmap> result;
  int glyph = 0;
  if (const std::vector<Layer> *layers = font().layersFor(codepoint, glyph)) {
    stbtt_fontinfo &info = font().info();
    const float scale = stbtt_ScaleForMappingEmToPixels(&info, (float)pixelSize);
    int ascent = 0, descent = 0, gap = 0;
    stbtt_GetFontVMetrics(&info, &ascent, &descent, &gap);
    auto bitmap = std::make_shared<EmojiBitmap>();
    bitmap->ascent = (float)ascent * scale;
    bitmap->width = std::max(1, (int)std::ceil(font().advanceEm() * (float)pixelSize));
    bitmap->height = std::max(1, (int)std::ceil((float)(ascent - descent) * scale));
    bitmap->rgba.assign((size_t)bitmap->width * bitmap->height * 4, 0);

    for (const Layer &layer : *layers) {
      uint8_t color[4];
      font().color(layer.palette, color);
      int w = 0, h = 0, xoff = 0, yoff = 0;
      unsigned char *mask = stbtt_GetGlyphBitmap(&info, scale, scale, layer.glyph, &w, &h, &xoff, &yoff);
      if (!mask) continue;
      for (int y = 0; y < h; ++y) {
        const int dy = (int)std::lround(bitmap->ascent) + yoff + y;
        if (dy < 0 || dy >= bitmap->height) continue;
        for (int x = 0; x < w; ++x) {
          const int dx = xoff + x;
          if (dx < 0 || dx >= bitmap->width) continue;
          const float srcA = (mask[y * w + x] / 255.0f) * (color[3] / 255.0f);
          if (srcA <= 0.0f) continue;
          uint8_t *dst = &bitmap->rgba[((size_t)dy * bitmap->width + dx) * 4];
          const float dstA = dst[3] / 255.0f;
          const float outA = srcA + dstA * (1.0f - srcA);
          for (int c = 0; c < 3; ++c) dst[c] = (uint8_t)std::lround((color[c] * srcA + dst[c] * dstA * (1.0f - srcA)) / outA);
          dst[3] = (uint8_t)std::lround(outA * 255.0f);
        }
      }
      stbtt_FreeBitmap(mask, nullptr);
    }
    result = bitmap;
  }
  cache.emplace(key, result);
  return result;
}

} // namespace n8v::detail
