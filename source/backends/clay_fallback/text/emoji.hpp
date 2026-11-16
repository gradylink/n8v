#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace n8v::detail {

struct EmojiBitmap {
  int width = 0;
  int height = 0;
  float ascent = 0.0f;
  std::vector<uint8_t> rgba;
};

bool isDrawableEmoji(uint32_t codepoint, uint32_t next);

bool isEmojiModifier(uint32_t codepoint, bool afterEmoji);

float emojiAdvanceEm();

std::shared_ptr<const EmojiBitmap> emojiBitmap(uint32_t codepoint, int pixelSize);

} // namespace n8v::detail
