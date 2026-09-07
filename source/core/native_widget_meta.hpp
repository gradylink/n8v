#pragma once

#include <n8v/types.hpp>

#include <functional>
#include <string>

namespace n8v::detail {

using n8v::NativeWidgetKind;

struct NativeWidgetMeta {
  NativeWidgetKind kind;
  int ordinal;
  std::function<void()> *onClick = nullptr; // Button only
  std::string *url = nullptr;               // Link only
};

} // namespace n8v::detail
