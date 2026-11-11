#pragma once

#include <string>

namespace n8v::detail {

inline std::string pageLabelWithBadges(const std::string &name, bool notify, const char *badge) {
  std::string label = notify ? "\u2022 " + name : name;
  return badge && *badge ? label + "  (" + badge + ")" : label;
}

} // namespace n8v::detail
