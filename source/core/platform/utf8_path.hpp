#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace n8v::detail {

inline std::filesystem::path pathFromUtf8(std::string_view text) { return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t *>(text.data()), text.size())); }

inline std::string pathToUtf8(const std::filesystem::path &path) {
  const std::u8string text = path.u8string();
  return std::string(reinterpret_cast<const char *>(text.data()), text.size());
}

} // namespace n8v::detail
