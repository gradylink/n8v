#pragma once

#include <string>
#include <vector>

namespace n8v::detail {

std::string bundledFontKey(bool bold, bool italic);
std::vector<unsigned char> bundledFontBytes(bool bold, bool italic);

} // namespace n8v::detail
