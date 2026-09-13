#pragma once

#include <n8v/types.hpp>

#include <string>
#include <string_view>

namespace n8v::detail {

bool isKnownIconName(std::string_view name);
std::string iconAssetPath(n8v::StyleFamily family, std::string_view name);
const char *resolveFreedesktopIconName(std::string_view name);

} // namespace n8v::detail
