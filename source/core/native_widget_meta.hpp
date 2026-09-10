#pragma once

#include <n8v/n8v_c.h>
#include <n8v/types.hpp>

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace n8v::detail {

using n8v::NativeWidgetKind;

struct NativeWidgetMeta {
  NativeWidgetKind kind;
  int ordinal;
  n8v_click_fn onClick = nullptr;                    // Button only
  void *onClickUserdata = nullptr;                   // Button only
  std::string *url = nullptr;                        // Link only
  bool *checked = nullptr;                           // Checkbox only
  n8v_bool_change_fn onChange = nullptr;             // Checkbox only
  void *onChangeUserdata = nullptr;                  // Checkbox only
  std::string *entryValue = nullptr;                 // Entry only
  n8v_string_buf *entryBuf = nullptr;                // Entry only - the caller's buffer entryValue shadows; see EntryEditState
  std::string *placeholder = nullptr;                // Entry only
  bool password = false;                             // Entry only
  n8v_text_change_fn onEntryChange = nullptr;        // Entry only
  void *onEntryChangeUserdata = nullptr;             // Entry only
  bool entryHasCustomBorder = false;                 // Entry only - skip the generic focus ring; the style's own border already changes color/width on focus
  int *radioSelected = nullptr;                      // Radio only
  int radioValue = 0;                                // Radio only
  n8v_int_change_fn onRadioChange = nullptr;         // Radio only
  void *onRadioChangeUserdata = nullptr;             // Radio only
  std::vector<std::string> *dropdownItems = nullptr; // Dropdown only
  int *dropdownSelected = nullptr;                   // Dropdown only
  n8v_int_change_fn onDropdownChange = nullptr;      // Dropdown only
  void *onDropdownChangeUserdata = nullptr;          // Dropdown only
  float *sliderValue = nullptr;                      // Slider only
  float sliderMin = 0.0f;                            // Slider only
  float sliderMax = 1.0f;                            // Slider only
  n8v_float_change_fn onSliderChange = nullptr;      // Slider only
  void *onSliderChangeUserdata = nullptr;            // Slider only
  n8v::Color indicatorFillColor{};                   // Checkbox/Radio only
  n8v::Color indicatorBorderColor{};                 // Checkbox/Radio only
  float indicatorBorderWidth = 0.0f;                 // Checkbox/Radio only
  n8v::Color indicatorGlyphColor{};                  // Checkbox (check) / Radio (dot) only
  float indicatorCornerRadius = 0.0f;                // Checkbox only
  float indicatorSize = 0.0f;                        // Checkbox/Radio only
  float indicatorGlyphScale = 1.0f;                  // Radio only - eases 0->1 so the inner dot grows in rather than popping to full size
  n8v::Color chevronColor{};                         // DropdownChevron only
  bool chevronPointsUp = false;                      // DropdownChevron only
};

inline std::function<void()> toStdFunction(n8v_click_fn fn, void *userdata) {
  return fn ? std::function<void()>([fn, userdata] { fn(userdata); }) : std::function<void()>{};
}

inline std::function<void(bool)> toStdFunction(n8v_bool_change_fn fn, void *userdata) {
  return fn ? std::function<void(bool)>([fn, userdata](bool value) { fn(value, userdata); }) : std::function<void(bool)>{};
}

inline std::function<void(int)> toStdFunction(n8v_int_change_fn fn, void *userdata) {
  return fn ? std::function<void(int)>([fn, userdata](int value) { fn(value, userdata); }) : std::function<void(int)>{};
}

inline std::function<void(float)> toStdFunction(n8v_float_change_fn fn, void *userdata) {
  return fn ? std::function<void(float)>([fn, userdata](float value) { fn(value, userdata); }) : std::function<void(float)>{};
}

inline std::function<void(std::string_view)> toStdFunction(n8v_text_change_fn fn, void *userdata) {
  return fn ? std::function<void(std::string_view)>([fn, userdata](std::string_view value) { fn(value.data(), value.size(), userdata); })
            : std::function<void(std::string_view)>{};
}

} // namespace n8v::detail
