#include "line_layout.hpp"

#include "bundled_font.hpp"
#include "emoji.hpp"
#include "utf8.hpp"

#include <algorithm>
#include <cmath>

namespace n8v::detail {
namespace {

std::shared_ptr<FontAtlas> ensureAtlas(FontFamily family, bool bold, bool italic) {
  std::shared_ptr<FontAtlas> atlas = FontManager::acquire(bundledFontKey(family, bold, italic));
  if (!atlas->isValid()) {
    std::vector<unsigned char> bytes = bundledFontBytes(family, bold, italic);
    if (bytes.empty() || !atlas->loadFromMemory(std::move(bytes))) return nullptr;
  }
  return atlas;
}

} // namespace

bool layoutLine(std::string_view utf8Text, FontFamily family, uint16_t pixelSize, bool bold, bool italic, LineLayoutResult &out) {
  std::shared_ptr<FontAtlas> atlas = ensureAtlas(family, bold, italic);
  if (!atlas || !atlas->isValid()) return false;

  std::vector<uint32_t> codepoints = utf8::decode(utf8Text);

  enum class Kind : uint8_t { Glyph, Emoji, Skipped };
  std::vector<Kind> kinds(codepoints.size(), Kind::Glyph);
  std::vector<uint32_t> atlasCodepoints;
  atlasCodepoints.reserve(codepoints.size());
  bool afterEmoji = false;
  for (size_t i = 0; i < codepoints.size(); ++i) {
    const uint32_t next = i + 1 < codepoints.size() ? codepoints[i + 1] : 0;
    if (isEmojiModifier(codepoints[i], afterEmoji)) {
      kinds[i] = Kind::Skipped;
    } else if (isDrawableEmoji(codepoints[i], next)) {
      kinds[i] = Kind::Emoji;
      afterEmoji = true;
    } else {
      afterEmoji = false;
      atlasCodepoints.push_back(codepoints[i]);
    }
  }

  float requestedPixelSize = pixelSize > 0 ? (float)pixelSize : 16.0f;
  int bucket = std::clamp((int)std::lround(requestedPixelSize), 6, 256);
  FontGeneration &gen = atlas->ensureGeneration(bucket, atlasCodepoints);
  float scale = requestedPixelSize / (float)gen.pixelSize;

  out.atlas = atlas;
  out.generation = &gen;
  out.scale = scale;
  out.quads.clear();
  out.emoji.clear();
  out.quads.reserve(codepoints.size());
  out.caretX.clear();
  out.caretX.reserve(codepoints.size());

  float penX = 0, penY = 0;
  for (size_t idx = 0; idx < codepoints.size(); ++idx) {
    if (kinds[idx] == Kind::Skipped) {
      out.caretX.push_back(penX * scale);
      continue;
    }
    if (kinds[idx] == Kind::Emoji) {
      out.emoji.push_back({codepoints[idx], penX, idx});
      penX += emojiAdvanceEm() * (float)gen.pixelSize;
      out.caretX.push_back(penX * scale);
      continue;
    }
    GlyphQuad q;
    atlas->getGlyphQuad(gen, codepoints[idx], penX, penY, q);
    if (q.valid) {
      q.codepointIndex = idx;
      out.quads.push_back(q);
    }
    out.caretX.push_back(penX * scale);
  }

  out.width = penX * scale;
  out.height = (gen.ascent - gen.descent + gen.lineGap) * scale;
  return true;
}

Clay_Dimensions measureLine(std::string_view utf8Text, FontFamily family, uint16_t pixelSize, bool bold, bool italic) {
  LineLayoutResult result;
  if (!layoutLine(utf8Text, family, pixelSize, bold, italic, result)) {
    float size = pixelSize > 0 ? (float)pixelSize : 16.0f;
    return {(float)utf8Text.size() * size * 0.6f, size * 1.2f};
  }
  return {result.width, result.height};
}

} // namespace n8v::detail
