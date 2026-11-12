#pragma once

#include <chrono>
#include <ctime>
#include <filesystem>
#include <string>
#include <system_error>

namespace n8v::detail {

inline std::string newPastedImagePath(const std::string &extension = "png") {
  std::error_code ec;
  const std::filesystem::path dir = std::filesystem::temp_directory_path(ec) / "n8v-paste";
  std::filesystem::create_directories(dir, ec);

  const std::time_t now = std::time(nullptr);
  std::tm local{};
#ifdef _WIN32
  localtime_s(&local, &now);
#else
  localtime_r(&now, &local);
#endif
  char stamp[32];
  std::strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", &local);

  for (int n = 1;; ++n) {
    const std::string name = std::string("image-") + stamp + (n > 1 ? "-" + std::to_string(n) : "") + "." + extension;
    const std::filesystem::path path = dir / name;
    if (!std::filesystem::exists(path, ec)) return path.string();
  }
}

} // namespace n8v::detail
