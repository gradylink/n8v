#pragma once

#include "font_atlas.hpp"

#include <n8v/types.hpp>

#include <clay.h>

#include <memory>
#include <string_view>
#include <vector>

namespace n8v::detail {

struct LineLayoutResult {
  std::shared_ptr<FontAtlas> atlas;
  FontGeneration *generation = nullptr;
  std::vector<GlyphQuad> quads;
  std::vector<float> caretX;
  float scale = 1.0f;
  float width = 0, height = 0;
};

bool layoutLine(std::string_view utf8Text, FontFamily family, uint16_t pixelSize, bool bold, bool italic, LineLayoutResult &out);

Clay_Dimensions measureLine(std::string_view utf8Text, FontFamily family, uint16_t pixelSize, bool bold, bool italic);

} // namespace n8v::detail
