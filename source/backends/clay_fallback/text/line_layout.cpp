#include "line_layout.hpp"

#include "bundled_font.hpp"
#include "utf8.hpp"

namespace n8v::detail {
namespace {

std::shared_ptr<FontAtlas> ensureAtlas(bool bold, bool italic) {
  std::shared_ptr<FontAtlas> atlas = FontManager::acquire(bundledFontKey(bold, italic));
  if (!atlas->isValid()) {
    std::vector<unsigned char> bytes = bundledFontBytes(bold, italic);
    if (bytes.empty() || !atlas->loadFromMemory(std::move(bytes))) return nullptr;
  }
  return atlas;
}

} // namespace

bool layoutLine(std::string_view utf8Text, uint16_t pixelSize, bool bold, bool italic, LineLayoutResult &out) {
  std::shared_ptr<FontAtlas> atlas = ensureAtlas(bold, italic);
  if (!atlas || !atlas->isValid()) return false;

  std::vector<uint32_t> codepoints = utf8::decode(utf8Text);

  float requestedPixelSize = pixelSize > 0 ? (float)pixelSize : 16.0f;
  int bucket = atlas->pickBucket(requestedPixelSize, 0);
  FontGeneration &gen = atlas->ensureGeneration(bucket, codepoints);
  float scale = requestedPixelSize / (float)gen.pixelSize;

  out.atlas = atlas;
  out.generation = &gen;
  out.scale = scale;
  out.quads.clear();
  out.quads.reserve(codepoints.size());

  float penX = 0, penY = 0;
  for (uint32_t cp : codepoints) {
    GlyphQuad q;
    atlas->getGlyphQuad(gen, cp, penX, penY, q);
    if (q.valid) out.quads.push_back(q);
  }

  out.width = penX * scale;
  out.height = (gen.ascent - gen.descent + gen.lineGap) * scale;
  return true;
}

Clay_Dimensions measureLine(std::string_view utf8Text, uint16_t pixelSize, bool bold, bool italic) {
  LineLayoutResult result;
  if (!layoutLine(utf8Text, pixelSize, bold, italic, result)) {
    float size = pixelSize > 0 ? (float)pixelSize : 16.0f;
    return {(float)utf8Text.size() * size * 0.6f, size * 1.2f};
  }
  return {result.width, result.height};
}

} // namespace n8v::detail
