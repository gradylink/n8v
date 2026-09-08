#pragma once

#include <n8v/types.hpp>

#include <functional>
#include <string>
#include <string_view>

namespace n8v::detail {

using n8v::NativeWidgetKind;

struct NativeWidgetMeta {
  NativeWidgetKind kind;
  int ordinal;
  std::function<void()> *onClick = nullptr;                     // Button only
  std::string *url = nullptr;                                   // Link only
  bool *checked = nullptr;                                      // Checkbox only
  std::function<void(bool)> *onChange = nullptr;                // Checkbox only
  std::string *entryValue = nullptr;                             // Entry only
  std::string *placeholder = nullptr;                            // Entry only
  bool password = false;                                         // Entry only
  std::function<void(std::string_view)> *onEntryChange = nullptr; // Entry only
};

} // namespace n8v::detail
