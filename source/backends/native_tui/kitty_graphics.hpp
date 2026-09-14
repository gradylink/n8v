#pragma once

#include <cstdint>
#include <string>

namespace n8v::detail {

struct DecodedImage;

std::string kittyMoveCursor(int col, int row);

std::string kittyTransmitAndPlace(const DecodedImage &image, uint32_t imageId, int cols, int rows);

std::string kittyPut(uint32_t imageId, int cols, int rows);

std::string kittyDelete(uint32_t imageId);

} // namespace n8v::detail
