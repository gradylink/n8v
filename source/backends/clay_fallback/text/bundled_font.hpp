#pragma once

#include <n8v/types.hpp>

#include <string>
#include <vector>

namespace n8v::detail {

std::string bundledFontKey(FontFamily family, bool bold, bool italic);
std::vector<unsigned char> bundledFontBytes(FontFamily family, bool bold, bool italic);

} // namespace n8v::detail
