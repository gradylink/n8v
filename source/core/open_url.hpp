#pragma once

#include <functional>
#include <string_view>

namespace n8v::detail {

void openUrl(std::string_view url);

void setUrlHandler(std::function<bool(std::string_view)> handler);

} // namespace n8v::detail
