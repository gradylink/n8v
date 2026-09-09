#pragma once

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
  std::function<void()> *onClick = nullptr;                     // Button only
  std::string *url = nullptr;                                   // Link only
  bool *checked = nullptr;                                      // Checkbox only
  std::function<void(bool)> *onChange = nullptr;                // Checkbox only
  std::string *entryValue = nullptr;                             // Entry only
  std::string *placeholder = nullptr;                            // Entry only
  bool password = false;                                         // Entry only
  std::function<void(std::string_view)> *onEntryChange = nullptr; // Entry only
  bool entryHasCustomBorder = false;                              // Entry only - skip the generic focus ring; the style's own border already changes color/width on focus
  int *radioSelected = nullptr;                                  // Radio only
  int radioValue = 0;                                            // Radio only
  std::function<void(int)> *onRadioChange = nullptr;             // Radio only
  std::vector<std::string> *dropdownItems = nullptr;             // Dropdown only
  int *dropdownSelected = nullptr;                               // Dropdown only
  std::function<void(int)> *onDropdownChange = nullptr;          // Dropdown only
  float *sliderValue = nullptr;                                  // Slider only
  float sliderMin = 0.0f;                                        // Slider only
  float sliderMax = 1.0f;                                        // Slider only
  std::function<void(float)> *onSliderChange = nullptr;          // Slider only
  n8v::Color indicatorFillColor{};                               // Checkbox/Radio only
  n8v::Color indicatorBorderColor{};                              // Checkbox/Radio only
  float indicatorBorderWidth = 0.0f;                              // Checkbox/Radio only
  n8v::Color indicatorGlyphColor{};                               // Checkbox (check) / Radio (dot) only
  float indicatorCornerRadius = 0.0f;                             // Checkbox only
  float indicatorSize = 0.0f;                                     // Checkbox/Radio only
  float indicatorGlyphScale = 1.0f;                               // Radio only - eases 0->1 so the inner dot grows in rather than popping to full size
  n8v::Color chevronColor{};                                      // DropdownChevron only
  bool chevronPointsUp = false;                                   // DropdownChevron only
};

} // namespace n8v::detail
