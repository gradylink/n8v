#pragma once

#include <n8v/detail/callback_bridge.hpp>
#include <n8v/n8v_c.h>
#include <n8v/options.hpp>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace n8v::detail {

inline n8v_direction toC(Direction d) { return d == Direction::Horizontal ? N8V_DIRECTION_HORIZONTAL : N8V_DIRECTION_VERTICAL; }

inline n8v_align toC(Align a) {
  switch (a) {
  case Align::Start:
    return N8V_ALIGN_START;
  case Align::Center:
    return N8V_ALIGN_CENTER;
  case Align::End:
    return N8V_ALIGN_END;
  }
  return N8V_ALIGN_START;
}

inline n8v_button_style toC(ButtonStyle s) { return s == ButtonStyle::Primary ? N8V_BUTTON_STYLE_PRIMARY : N8V_BUTTON_STYLE_SECONDARY; }

inline n8v_icon_variant toC(IconVariant v) { return v == IconVariant::Outline ? N8V_ICON_VARIANT_OUTLINE : N8V_ICON_VARIANT_FILLED; }

inline n8v_icon_position toC(IconPosition p) { return p == IconPosition::Leading ? N8V_ICON_POSITION_LEADING : N8V_ICON_POSITION_TRAILING; }

inline n8v_sizing_mode toC(SizingMode m) {
  switch (m) {
  case SizingMode::Fit:
    return N8V_SIZING_FIT;
  case SizingMode::Grow:
    return N8V_SIZING_GROW;
  case SizingMode::Fixed:
    return N8V_SIZING_FIXED;
  case SizingMode::Percent:
    return N8V_SIZING_PERCENT;
  }
  return N8V_SIZING_FIT;
}

inline n8v_sizing toC(Sizing s) { return n8v_sizing{toC(s.mode), s.value, s.min, s.max}; }

inline n8v_image_source_kind toC(ImageSource s) {
  switch (s) {
  case ImageSource::Path:
    return N8V_IMAGE_SOURCE_PATH;
  case ImageSource::Bundle:
    return N8V_IMAGE_SOURCE_BUNDLE;
  case ImageSource::Encoded:
    return N8V_IMAGE_SOURCE_ENCODED;
  case ImageSource::Rgba:
    return N8V_IMAGE_SOURCE_RGBA;
  }
  return N8V_IMAGE_SOURCE_PATH;
}

inline n8v_color toC(Color c) { return n8v_color{c.r, c.g, c.b, c.a}; }

inline n8v_padding toC(Padding p) { return n8v_padding{p.left, p.right, p.top, p.bottom}; }

inline n8v_corner_radius toC(CornerRadius r) { return n8v_corner_radius{r.topLeft, r.topRight, r.bottomLeft, r.bottomRight}; }

inline n8v_rounding_mode toC(RoundingMode m) {
  switch (m) {
  case RoundingMode::StyleDefault:
    return N8V_ROUNDING_STYLE_DEFAULT;
  case RoundingMode::None:
    return N8V_ROUNDING_NONE;
  case RoundingMode::Fixed:
    return N8V_ROUNDING_FIXED;
  }
  return N8V_ROUNDING_STYLE_DEFAULT;
}

inline n8v_rounding toC(Rounding r) { return n8v_rounding{toC(r.mode), toC(r.radius)}; }

inline n8v_style_family toC(StyleFamily f) {
  switch (f) {
  case StyleFamily::Plain:
    return N8V_STYLE_FAMILY_PLAIN;
  case StyleFamily::Material:
    return N8V_STYLE_FAMILY_MATERIAL;
  case StyleFamily::Cupertino:
    return N8V_STYLE_FAMILY_CUPERTINO;
  case StyleFamily::Fluent:
    return N8V_STYLE_FAMILY_FLUENT;
  }
  return N8V_STYLE_FAMILY_PLAIN;
}

inline StyleFamily fromC(n8v_style_family f) {
  switch (f) {
  case N8V_STYLE_FAMILY_PLAIN:
    return StyleFamily::Plain;
  case N8V_STYLE_FAMILY_MATERIAL:
    return StyleFamily::Material;
  case N8V_STYLE_FAMILY_CUPERTINO:
    return StyleFamily::Cupertino;
  case N8V_STYLE_FAMILY_FLUENT:
    return StyleFamily::Fluent;
  }
  return StyleFamily::Plain;
}

inline void beginFrame() {
  n8v_begin_frame();
  callback_bridge::clearCallbackStorage();
}

inline void endFrame() { n8v_end_frame(); }

inline void openFlex(const FlexOptions &options) {
  n8v_flex_options c_opts{};
  c_opts.direction = toC(options.direction);
  c_opts.gap = options.gap;
  c_opts.padding = toC(options.padding);
  c_opts.h_align = toC(options.hAlign);
  c_opts.v_align = toC(options.vAlign);
  c_opts.width = toC(options.width);
  c_opts.height = toC(options.height);
  c_opts.clip_horizontal = options.clipHorizontal;
  c_opts.clip_vertical = options.clipVertical;
  n8v_open_flex(c_opts);
}

inline void closeFlex() { n8v_close_flex(); }

struct LeafBuilder {
  bool isButton;
  ButtonOptions buttonOptions;
  TextOptions textOptions;

  void operator()(std::string_view label) && {
    std::string labelStorage(label);
    if (isButton) {
      std::string iconStorage(buttonOptions.icon);
      n8v_button_options c_opts{};
      c_opts.style = toC(buttonOptions.style);
      if (buttonOptions.onClick) callback_bridge::bridge(callback_bridge::clickClosures, std::move(buttonOptions.onClick), c_opts.on_click, c_opts.on_click_userdata);
      c_opts.icon = iconStorage.empty() ? nullptr : iconStorage.c_str();
      c_opts.icon_variant = toC(buttonOptions.iconVariant);
      c_opts.icon_position = toC(buttonOptions.iconPosition);
      _n8v_set_button_opts(c_opts);
      _n8v_button_commit(labelStorage.c_str());
    } else {
      std::string urlStorage(textOptions.url);
      n8v_text_options c_opts{};
      c_opts.bold = textOptions.bold;
      c_opts.italic = textOptions.italic;
      c_opts.url = urlStorage.c_str();
      c_opts.color = toC(textOptions.color);
      _n8v_set_text_opts(c_opts);
      _n8v_text_commit(labelStorage.c_str());
    }
  }
};

struct CheckboxBuilder {
  CheckboxOptions options;

  void operator()(std::string_view label) && {
    std::string labelStorage(label);
    n8v_checkbox_options c_opts{};
    c_opts.checked = options.checked;
    if (options.onChange) callback_bridge::bridge(callback_bridge::boolChangeClosures, std::move(options.onChange), c_opts.on_change, c_opts.on_change_userdata);
    _n8v_set_checkbox_opts(c_opts);
    _n8v_checkbox_commit(labelStorage.c_str());
  }
};

struct RadioBuilder {
  RadioOptions options;

  void operator()(std::string_view label) && {
    std::string labelStorage(label);
    n8v_radio_options c_opts{};
    c_opts.selected = options.selected;
    c_opts.value = options.value;
    if (options.onChange) callback_bridge::bridge(callback_bridge::intChangeClosures, std::move(options.onChange), c_opts.on_change, c_opts.on_change_userdata);
    _n8v_set_radio_opts(c_opts);
    _n8v_radio_commit(labelStorage.c_str());
  }
};

struct EntryShadow {
  n8v_string_buf buf{};
  std::string lastSyncedValue;
};

inline std::unordered_map<std::string *, EntryShadow> entryBufShadows;

inline void syncStringBuf(n8v_string_buf &buf, std::string_view value) {
  std::string_view current(buf.data ? buf.data : "", buf.length);
  if (current == value) return;
  size_t needed = value.size() + 1;
  if (buf.capacity < needed) {
    size_t newCap = buf.capacity == 0 ? 16 : buf.capacity;
    while (newCap < needed) newCap *= 2;
    char *newData = static_cast<char *>(std::realloc(buf.data, newCap));
    if (!newData) return;
    buf.data = newData;
    buf.capacity = newCap;
  }
  std::memcpy(buf.data, value.data(), value.size());
  buf.data[value.size()] = '\0';
  buf.length = value.size();
}

inline void entry(EntryOptions options) {
  std::string placeholderStorage(options.placeholder);
  n8v_entry_options c_opts{};
  c_opts.placeholder = placeholderStorage.c_str();
  c_opts.password = options.password;

  n8v_string_buf *buf = nullptr;
  EntryShadow *shadow = nullptr;
  if (options.value) {
    EntryShadow &s = entryBufShadows[options.value];
    if (*options.value != s.lastSyncedValue) syncStringBuf(s.buf, *options.value);
    buf = &s.buf;
    shadow = &s;
  }
  c_opts.value = buf;

  if (options.onChange) callback_bridge::bridgeTextChange(std::move(options.onChange), c_opts.on_change, c_opts.on_change_userdata);

  n8v_entry(c_opts);

  if (buf && options.value) {
    options.value->assign(buf->data ? buf->data : "", buf->length);
    shadow->lastSyncedValue = *options.value;
  }
}

inline void dropdown(DropdownOptions options) {
  std::string placeholderStorage(options.placeholder);
  std::vector<std::string> itemStorage(options.items.begin(), options.items.end());
  std::vector<const char *> itemPtrs;
  itemPtrs.reserve(itemStorage.size());
  for (const std::string &item : itemStorage) itemPtrs.push_back(item.c_str());

  n8v_dropdown_options c_opts{};
  c_opts.items = itemPtrs.data();
  c_opts.item_count = itemPtrs.size();
  c_opts.selected = options.selected;
  c_opts.placeholder = placeholderStorage.c_str();
  if (options.onChange) callback_bridge::bridge(callback_bridge::intChangeClosures, std::move(options.onChange), c_opts.on_change, c_opts.on_change_userdata);

  n8v_dropdown(c_opts);
}

inline void slider(SliderOptions options) {
  n8v_slider_options c_opts{};
  c_opts.value = options.value;
  c_opts.min = options.min;
  c_opts.max = options.max;
  if (options.onChange) callback_bridge::bridge(callback_bridge::floatChangeClosures, std::move(options.onChange), c_opts.on_change, c_opts.on_change_userdata);

  n8v_slider(c_opts);
}

inline void image(ImageOptions options) {
  n8v_image_options c_opts{};
  c_opts.source_kind = toC(options.source);
  c_opts.path = options.path.empty() ? nullptr : options.path.c_str();
  c_opts.encoded_data = options.encodedData;
  c_opts.encoded_size = options.encodedSize;
  c_opts.pixels = options.pixels;
  c_opts.pixel_width = options.pixelWidth;
  c_opts.pixel_height = options.pixelHeight;
  c_opts.width = toC(options.width);
  c_opts.height = toC(options.height);
  c_opts.rounding = toC(options.rounding);

  n8v_image(c_opts);
}

inline void icon(IconOptions options) {
  std::string nameStorage(options.name);
  n8v_icon_options c_opts{};
  c_opts.name = nameStorage.empty() ? nullptr : nameStorage.c_str();
  c_opts.variant = toC(options.variant);
  c_opts.width = toC(options.width);
  c_opts.height = toC(options.height);
  c_opts.tint = toC(options.tint);

  n8v_icon(c_opts);
}

} // namespace n8v::detail

namespace n8v {

inline detail::LeafBuilder button(ButtonOptions options) { return detail::LeafBuilder{true, std::move(options), {}}; }

inline detail::LeafBuilder text(TextOptions options) { return detail::LeafBuilder{false, {}, std::move(options)}; }

inline detail::CheckboxBuilder checkbox(CheckboxOptions options) { return detail::CheckboxBuilder{std::move(options)}; }

inline detail::RadioBuilder radio(RadioOptions options) { return detail::RadioBuilder{std::move(options)}; }

inline void entry(EntryOptions options) { detail::entry(std::move(options)); }

inline void dropdown(DropdownOptions options) { detail::dropdown(std::move(options)); }

inline void slider(SliderOptions options) { detail::slider(std::move(options)); }

inline void image(ImageOptions options) { detail::image(std::move(options)); }

inline void icon(IconOptions options) { detail::icon(std::move(options)); }

inline bool initialize(int width, int height, std::string_view title) {
  std::string titleStorage(title);
  return n8v_initialize(width, height, titleStorage.c_str());
}

inline bool pumpEvents() { return n8v_pump_events(); }

inline void shutdown() { n8v_shutdown(); }

inline void setStyleFamily(StyleFamily family) { n8v_set_style_family(detail::toC(family)); }

inline StyleFamily activeStyleFamily() { return detail::fromC(n8v_active_style_family()); }

} // namespace n8v

#define UI() for (uint8_t n8v_uiLatch = (n8v::detail::beginFrame(), 0); n8v_uiLatch < 1; n8v_uiLatch = 1, n8v::detail::endFrame())

#define flex(...) for (uint8_t n8v_flexLatch = (n8v::detail::openFlex(__VA_ARGS__), 0); n8v_flexLatch < 1; n8v_flexLatch = 1, n8v::detail::closeFlex())
