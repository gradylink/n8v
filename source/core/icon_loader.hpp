#pragma once

#include "core/image_loader.hpp"
#include <n8v/types.hpp>

#include <string_view>

namespace n8v::detail {

const DecodedImage *getOrDecodeIcon(std::string_view name, uint16_t pixelSize, n8v::Color tint);
const DecodedImage *getOrDecodeIconFromFile(const std::string &filePath, uint16_t pixelSize, n8v::Color tint);

} // namespace n8v::detail
