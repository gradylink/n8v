#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace n8v::detail {

bool looksLikeWebp(const uint8_t *bytes, size_t size);

bool decodeWebp(const uint8_t *bytes, size_t size, std::vector<uint8_t> &rgba, int &width, int &height);

} // namespace n8v::detail
