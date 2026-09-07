#pragma once

#include <n8v/types.hpp>

namespace n8v::detail {

struct TextStyleFlags {
  FontFamily font = FontFamily::DejaVuSans;
  bool bold = false;
  bool italic = false;
  bool underline = false;
};

} // namespace n8v::detail
