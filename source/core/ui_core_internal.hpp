#pragma once

#include "core/backend.hpp"
#include <n8v/n8v_c.h>
#include <n8v/types.hpp>

#include "core/native_widget_meta.hpp"
#include "core/text_style_flags.hpp"

#include <clay.h>

#include <cstdlib>
#include <cstring>
#include <deque>
#include <string>
#include <string_view>
#include <unordered_map>

namespace n8v::detail::ui_internal {

inline n8v::Direction toDirection(n8v_direction d) { return d == N8V_DIRECTION_VERTICAL ? n8v::Direction::Vertical : n8v::Direction::Horizontal; }

inline n8v::Align toAlign(n8v_align a) {
  switch (a) {
  case N8V_ALIGN_START:
    return n8v::Align::Start;
  case N8V_ALIGN_CENTER:
    return n8v::Align::Center;
  case N8V_ALIGN_END:
    return n8v::Align::End;
  }
  return n8v::Align::Start;
}

inline n8v::ButtonStyle toButtonStyle(n8v_button_style s) { return s == N8V_BUTTON_STYLE_SECONDARY ? n8v::ButtonStyle::Secondary : n8v::ButtonStyle::Primary; }

inline n8v::SizingMode toSizingMode(n8v_sizing_mode m) {
  switch (m) {
  case N8V_SIZING_FIT:
    return n8v::SizingMode::Fit;
  case N8V_SIZING_GROW:
    return n8v::SizingMode::Grow;
  case N8V_SIZING_FIXED:
    return n8v::SizingMode::Fixed;
  case N8V_SIZING_PERCENT:
    return n8v::SizingMode::Percent;
  }
  return n8v::SizingMode::Fit;
}

inline n8v::Sizing toSizing(n8v_sizing s) { return n8v::Sizing{toSizingMode(s.mode), s.value, s.min, s.max}; }

inline n8v::Color toColor(n8v_color c) { return n8v::Color{c.r, c.g, c.b, c.a}; }

inline n8v::Padding toPadding(n8v_padding p) { return n8v::Padding{p.left, p.right, p.top, p.bottom}; }

inline n8v::CornerRadius toCornerRadius(n8v_corner_radius r) { return n8v::CornerRadius{r.top_left, r.top_right, r.bottom_left, r.bottom_right}; }

inline n8v::RoundingMode toRoundingMode(n8v_rounding_mode m) {
  switch (m) {
  case N8V_ROUNDING_STYLE_DEFAULT:
    return n8v::RoundingMode::StyleDefault;
  case N8V_ROUNDING_NONE:
    return n8v::RoundingMode::None;
  case N8V_ROUNDING_FIXED:
    return n8v::RoundingMode::Fixed;
  }
  return n8v::RoundingMode::StyleDefault;
}

inline n8v::Rounding toRounding(n8v_rounding r) { return n8v::Rounding{toRoundingMode(r.mode), toCornerRadius(r.radius)}; }

inline n8v::StyleFamily toStyleFamily(n8v_style_family f) {
  switch (f) {
  case N8V_STYLE_FAMILY_PLAIN:
    return n8v::StyleFamily::Plain;
  case N8V_STYLE_FAMILY_MATERIAL:
    return n8v::StyleFamily::Material;
  case N8V_STYLE_FAMILY_CUPERTINO:
    return n8v::StyleFamily::Cupertino;
  case N8V_STYLE_FAMILY_FLUENT:
    return n8v::StyleFamily::Fluent;
  }
  return n8v::StyleFamily::Plain;
}

inline n8v_style_family fromStyleFamily(n8v::StyleFamily f) {
  switch (f) {
  case n8v::StyleFamily::Plain:
    return N8V_STYLE_FAMILY_PLAIN;
  case n8v::StyleFamily::Material:
    return N8V_STYLE_FAMILY_MATERIAL;
  case n8v::StyleFamily::Cupertino:
    return N8V_STYLE_FAMILY_CUPERTINO;
  case n8v::StyleFamily::Fluent:
    return N8V_STYLE_FAMILY_FLUENT;
  }
  return N8V_STYLE_FAMILY_PLAIN;
}

inline std::string_view toView(const char *s) { return s ? std::string_view(s) : std::string_view{}; }

inline void ensureStringBufCapacity(n8v_string_buf &buf, size_t needed) {
  if (needed <= buf.capacity) return;
  size_t newCap = buf.capacity == 0 ? 16 : buf.capacity;
  while (newCap < needed) newCap *= 2;
  char *newData = static_cast<char *>(std::realloc(buf.data, newCap));
  if (!newData) return;
  buf.data = newData;
  buf.capacity = newCap;
}

inline void writeToStringBuf(std::string_view value, n8v_string_buf &buf) {
  ensureStringBufCapacity(buf, value.size() + 1);
  if (buf.capacity < value.size() + 1) return;
  std::memcpy(buf.data, value.data(), value.size());
  buf.data[value.size()] = '\0';
  buf.length = value.size();
}

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
void resetImageFrameState();

} // namespace n8v::detail::ui_internal
