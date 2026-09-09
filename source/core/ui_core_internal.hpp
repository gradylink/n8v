#pragma once

#include <n8v/backend.hpp>
#include <n8v/types.hpp>

#include "core/native_widget_meta.hpp"
#include "core/text_style_flags.hpp"

#include <clay.h>

#include <deque>
#include <string>
#include <string_view>
#include <unordered_map>

namespace n8v::detail::ui_internal {

inline std::deque<std::string> textStorage;
inline std::deque<n8v::detail::TextStyleFlags> textStyleStorage;
inline std::deque<n8v::detail::NativeWidgetMeta> widgetMetaStorage;

inline int widgetOrdinal = 0;
inline n8v::CursorKind pendingCursor = n8v::CursorKind::Default;
inline float currentDelta = 0.0f;

inline int animKey(int ordinal, int slot) { return ordinal * 64 + slot; }

inline float easeValue(int key, float target, float duration) {
  struct ValueAnimation {
    float start = 0.0f;
    float current = 0.0f;
    float target = 0.0f;
    float elapsed = 0.0f;
  };
  static std::unordered_map<int, ValueAnimation> animations;

  ValueAnimation &anim = animations[key];
  if (anim.target != target) {
    anim.start = anim.current;
    anim.target = target;
    anim.elapsed = 0.0f;
  }
  if (duration <= 0.0f) {
    anim.current = target;
    return target;
  }
  anim.elapsed += currentDelta;
  float ratio = anim.elapsed / duration;
  if (ratio > 1.0f) ratio = 1.0f;
  float inverse = 1.0f - ratio;
  float lerp = 1.0f - inverse * inverse * inverse;
  anim.current = anim.start + (anim.target - anim.start) * lerp;
  return anim.current;
}

inline n8v::Color easeColor(int ordinal, int slotBase, n8v::Color target, float duration) {
  return {
    easeValue(animKey(ordinal, slotBase + 0), target.r, duration),
    easeValue(animKey(ordinal, slotBase + 1), target.g, duration),
    easeValue(animKey(ordinal, slotBase + 2), target.b, duration),
    easeValue(animKey(ordinal, slotBase + 3), target.a, duration),
  };
}

inline Clay_String internString(std::string_view text) {
  textStorage.emplace_back(text);
  const std::string &stored = textStorage.back();
  return Clay_String{false, (int32_t)stored.size(), stored.data()};
}

void resetLeafFrameState();
void resetCheckboxFrameState();
void resetRadioFrameState();
void resetEntryFrameState();
void resetDropdownFrameState();
void resetSliderFrameState();

} // namespace n8v::detail::ui_internal
