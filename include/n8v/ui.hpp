#pragma once

#include <n8v/detail/callback_bridge.hpp>
#include <n8v/n8v_c.h>
#include <n8v/options.hpp>
#include <n8v/style.hpp>

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
inline ButtonStyle fromC(n8v_button_style s) { return s == N8V_BUTTON_STYLE_PRIMARY ? ButtonStyle::Primary : ButtonStyle::Secondary; }

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
inline Color fromC(n8v_color c) { return Color{c.r, c.g, c.b, c.a}; }

inline n8v_padding toC(Padding p) { return n8v_padding{p.left, p.right, p.top, p.bottom}; }
inline Padding fromC(n8v_padding p) { return Padding{p.left, p.right, p.top, p.bottom}; }

inline n8v_corner_radius toC(CornerRadius r) { return n8v_corner_radius{r.topLeft, r.topRight, r.bottomLeft, r.bottomRight}; }
inline CornerRadius fromC(n8v_corner_radius r) { return CornerRadius{r.top_left, r.top_right, r.bottom_left, r.bottom_right}; }

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
  case StyleFamily::Custom:
    return N8V_STYLE_FAMILY_CUSTOM;
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
  case N8V_STYLE_FAMILY_CUSTOM:
    return StyleFamily::Custom;
  }
  return StyleFamily::Plain;
}

inline n8v_font_family toC(FontFamily f) {
  switch (f) {
  case FontFamily::DejaVuSans:
    return N8V_FONT_FAMILY_DEJAVU_SANS;
  case FontFamily::Roboto:
    return N8V_FONT_FAMILY_ROBOTO;
  case FontFamily::Inter:
    return N8V_FONT_FAMILY_INTER;
  case FontFamily::Selawik:
    return N8V_FONT_FAMILY_SELAWIK;
  }
  return N8V_FONT_FAMILY_DEJAVU_SANS;
}

inline FontFamily fromC(n8v_font_family f) {
  switch (f) {
  case N8V_FONT_FAMILY_DEJAVU_SANS:
    return FontFamily::DejaVuSans;
  case N8V_FONT_FAMILY_ROBOTO:
    return FontFamily::Roboto;
  case N8V_FONT_FAMILY_INTER:
    return FontFamily::Inter;
  case N8V_FONT_FAMILY_SELAWIK:
    return FontFamily::Selawik;
  }
  return FontFamily::DejaVuSans;
}

inline std::string_view toView(const char *s) { return s ? std::string_view(s) : std::string_view{}; }

inline TextOptions fromC(n8v_text_options options) {
  return TextOptions{
    .bold = options.bold, .italic = options.italic, .strikethrough = options.strikethrough, .url = toView(options.url), .color = fromC(options.color)
  };
}

inline n8v_text_options toC(const TextOptions &options) {
  thread_local std::string urlStorage;
  urlStorage.assign(options.url);
  n8v_text_options c{};
  c.bold = options.bold;
  c.italic = options.italic;
  c.strikethrough = options.strikethrough;
  c.url = urlStorage.c_str();
  c.color = toC(options.color);
  return c;
}

inline n8v_button_paint toC(const ButtonPaint &p) {
  return n8v_button_paint{
    .background = toC(p.background),
    .text_color = toC(p.textColor),
    .corner_radius = toC(p.cornerRadius),
    .padding = toC(p.padding),
    .font = toC(p.font),
    .font_size = p.fontSize,
    .transition_seconds = p.transitionSeconds,
  };
}

inline ButtonPaint fromC(n8v_button_paint p) {
  return ButtonPaint{
    .background = fromC(p.background),
    .textColor = fromC(p.text_color),
    .cornerRadius = fromC(p.corner_radius),
    .padding = fromC(p.padding),
    .font = fromC(p.font),
    .fontSize = p.font_size,
    .transitionSeconds = p.transition_seconds,
  };
}

inline n8v_text_paint toC(const TextPaint &p) { return n8v_text_paint{.color = toC(p.color), .font = toC(p.font), .font_size = p.fontSize}; }
inline TextPaint fromC(n8v_text_paint p) { return TextPaint{.color = fromC(p.color), .font = fromC(p.font), .fontSize = p.font_size}; }

inline n8v_checkbox_paint toC(const CheckboxPaint &p) {
  return n8v_checkbox_paint{
    .background = toC(p.background),
    .border_color = toC(p.borderColor),
    .border_width = p.borderWidth,
    .check_color = toC(p.checkColor),
    .corner_radius = toC(p.cornerRadius),
    .indicator_size = p.indicatorSize,
    .padding = toC(p.padding),
    .font = toC(p.font),
    .font_size = p.fontSize,
    .transition_seconds = p.transitionSeconds,
  };
}

inline CheckboxPaint fromC(n8v_checkbox_paint p) {
  return CheckboxPaint{
    .background = fromC(p.background),
    .borderColor = fromC(p.border_color),
    .borderWidth = p.border_width,
    .checkColor = fromC(p.check_color),
    .cornerRadius = fromC(p.corner_radius),
    .indicatorSize = p.indicator_size,
    .padding = fromC(p.padding),
    .font = fromC(p.font),
    .fontSize = p.font_size,
    .transitionSeconds = p.transition_seconds,
  };
}

inline n8v_radio_paint toC(const RadioPaint &p) {
  return n8v_radio_paint{
    .background = toC(p.background),
    .border_color = toC(p.borderColor),
    .border_width = p.borderWidth,
    .dot_color = toC(p.dotColor),
    .indicator_size = p.indicatorSize,
    .padding = toC(p.padding),
    .font = toC(p.font),
    .font_size = p.fontSize,
    .transition_seconds = p.transitionSeconds,
  };
}

inline RadioPaint fromC(n8v_radio_paint p) {
  return RadioPaint{
    .background = fromC(p.background),
    .borderColor = fromC(p.border_color),
    .borderWidth = p.border_width,
    .dotColor = fromC(p.dot_color),
    .indicatorSize = p.indicator_size,
    .padding = fromC(p.padding),
    .font = fromC(p.font),
    .fontSize = p.font_size,
    .transitionSeconds = p.transition_seconds,
  };
}

inline n8v_toggle_paint toC(const TogglePaint &p) {
  return n8v_toggle_paint{
    .track_on_color = toC(p.trackOnColor),
    .track_off_color = toC(p.trackOffColor),
    .track_border_color = toC(p.trackBorderColor),
    .track_border_width = p.trackBorderWidth,
    .knob_on_color = toC(p.knobOnColor),
    .knob_off_color = toC(p.knobOffColor),
    .knob_glyph_color = toC(p.knobGlyphColor),
    .show_glyph_when_on = p.showGlyphWhenOn,
    .track_width = p.trackWidth,
    .track_height = p.trackHeight,
    .knob_size_off = p.knobSizeOff,
    .knob_size_on = p.knobSizeOn,
    .padding = toC(p.padding),
    .font = toC(p.font),
    .font_size = p.fontSize,
    .transition_seconds = p.transitionSeconds,
  };
}

inline TogglePaint fromC(n8v_toggle_paint p) {
  return TogglePaint{
    .trackOnColor = fromC(p.track_on_color),
    .trackOffColor = fromC(p.track_off_color),
    .trackBorderColor = fromC(p.track_border_color),
    .trackBorderWidth = p.track_border_width,
    .knobOnColor = fromC(p.knob_on_color),
    .knobOffColor = fromC(p.knob_off_color),
    .knobGlyphColor = fromC(p.knob_glyph_color),
    .showGlyphWhenOn = p.show_glyph_when_on,
    .trackWidth = p.track_width,
    .trackHeight = p.track_height,
    .knobSizeOff = p.knob_size_off,
    .knobSizeOn = p.knob_size_on,
    .padding = fromC(p.padding),
    .font = fromC(p.font),
    .fontSize = p.font_size,
    .transitionSeconds = p.transition_seconds,
  };
}

inline n8v_entry_paint toC(const EntryPaint &p) {
  return n8v_entry_paint{
    .background = toC(p.background),
    .text_color = toC(p.textColor),
    .placeholder_color = toC(p.placeholderColor),
    .border_color = toC(p.borderColor),
    .border_width = p.borderWidth,
    .outlined = p.outlined,
    .label_color = toC(p.labelColor),
    .label_font_size = p.labelFontSize,
    .transition_seconds = p.transitionSeconds,
    .corner_radius = toC(p.cornerRadius),
    .padding = toC(p.padding),
    .font = toC(p.font),
    .font_size = p.fontSize,
  };
}

inline EntryPaint fromC(n8v_entry_paint p) {
  return EntryPaint{
    .background = fromC(p.background),
    .textColor = fromC(p.text_color),
    .placeholderColor = fromC(p.placeholder_color),
    .borderColor = fromC(p.border_color),
    .borderWidth = p.border_width,
    .outlined = p.outlined,
    .labelColor = fromC(p.label_color),
    .labelFontSize = p.label_font_size,
    .transitionSeconds = p.transition_seconds,
    .cornerRadius = fromC(p.corner_radius),
    .padding = fromC(p.padding),
    .font = fromC(p.font),
    .fontSize = p.font_size,
  };
}

inline n8v_dropdown_paint toC(const DropdownPaint &p) {
  return n8v_dropdown_paint{
    .background = toC(p.background),
    .text_color = toC(p.textColor),
    .placeholder_color = toC(p.placeholderColor),
    .popup_background = toC(p.popupBackground),
    .item_hover_background = toC(p.itemHoverBackground),
    .item_selected_background = toC(p.itemSelectedBackground),
    .corner_radius = toC(p.cornerRadius),
    .padding = toC(p.padding),
    .font = toC(p.font),
    .font_size = p.fontSize,
    .transition_seconds = p.transitionSeconds,
    .label_color = toC(p.labelColor),
    .label_font_size = p.labelFontSize,
    .indicator_color = toC(p.indicatorColor),
    .indicator_width = p.indicatorWidth,
  };
}

inline DropdownPaint fromC(n8v_dropdown_paint p) {
  return DropdownPaint{
    .background = fromC(p.background),
    .textColor = fromC(p.text_color),
    .placeholderColor = fromC(p.placeholder_color),
    .popupBackground = fromC(p.popup_background),
    .itemHoverBackground = fromC(p.item_hover_background),
    .itemSelectedBackground = fromC(p.item_selected_background),
    .cornerRadius = fromC(p.corner_radius),
    .padding = fromC(p.padding),
    .font = fromC(p.font),
    .fontSize = p.font_size,
    .transitionSeconds = p.transition_seconds,
    .labelColor = fromC(p.label_color),
    .labelFontSize = p.label_font_size,
    .indicatorColor = fromC(p.indicator_color),
    .indicatorWidth = p.indicator_width,
  };
}

inline n8v_slider_paint toC(const SliderPaint &p) {
  return n8v_slider_paint{
    .track_color = toC(p.trackColor),
    .fill_color = toC(p.fillColor),
    .thumb_color = toC(p.thumbColor),
    .track_height = p.trackHeight,
    .thumb_width = p.thumbWidth,
    .thumb_height = p.thumbHeight,
    .thumb_border_color = toC(p.thumbBorderColor),
    .thumb_border_width = p.thumbBorderWidth,
    .track_gap = p.trackGap,
  };
}

inline SliderPaint fromC(n8v_slider_paint p) {
  return SliderPaint{
    .trackColor = fromC(p.track_color),
    .fillColor = fromC(p.fill_color),
    .thumbColor = fromC(p.thumb_color),
    .trackHeight = p.track_height,
    .thumbWidth = p.thumb_width,
    .thumbHeight = p.thumb_height,
    .thumbBorderColor = fromC(p.thumb_border_color),
    .thumbBorderWidth = p.thumb_border_width,
    .trackGap = p.track_gap,
  };
}

inline n8v_image_paint toC(const ImagePaint &p) { return n8v_image_paint{.corner_radius = toC(p.cornerRadius)}; }
inline ImagePaint fromC(n8v_image_paint p) { return ImagePaint{.cornerRadius = fromC(p.corner_radius)}; }

inline n8v_icon_paint toC(const IconPaint &p) { return n8v_icon_paint{.tint = toC(p.tint), .default_size = p.defaultSize}; }
inline IconPaint fromC(n8v_icon_paint p) { return IconPaint{.tint = fromC(p.tint), .defaultSize = p.default_size}; }

inline n8v_sidebar_paint toC(const SidebarPaint &p) {
  return n8v_sidebar_paint{
    .background = toC(p.background),
    .border_color = toC(p.borderColor),
    .border_width = p.borderWidth,
    .corner_radius = toC(p.cornerRadius),
    .padding = toC(p.padding),
    .row_gap = p.rowGap,
  };
}

inline SidebarPaint fromC(n8v_sidebar_paint p) {
  return SidebarPaint{
    .background = fromC(p.background),
    .borderColor = fromC(p.border_color),
    .borderWidth = p.border_width,
    .cornerRadius = fromC(p.corner_radius),
    .padding = fromC(p.padding),
    .rowGap = p.row_gap,
  };
}

class BuiltinPaint final : public Paint {
public:
  explicit BuiltinPaint(n8v_style_family family) : family_(family) {}

  ButtonPaint button(ButtonStyle style, bool hovered, bool pressed) const override { return fromC(n8v_style_button_paint(family_, toC(style), hovered, pressed)); }
  TextPaint text(const TextOptions &options) const override { return fromC(n8v_style_text_paint(family_, toC(options))); }
  CheckboxPaint checkbox(bool checked, bool hovered, bool pressed) const override { return fromC(n8v_style_checkbox_paint(family_, checked, hovered, pressed)); }
  RadioPaint radio(bool selected, bool hovered, bool pressed) const override { return fromC(n8v_style_radio_paint(family_, selected, hovered, pressed)); }
  TogglePaint toggle(bool on, bool hovered, bool pressed) const override { return fromC(n8v_style_toggle_paint(family_, on, hovered, pressed)); }
  EntryPaint entry(bool focused, bool hasValue) const override { return fromC(n8v_style_entry_paint(family_, focused, hasValue)); }
  DropdownPaint dropdown(bool open, bool hasSelection, bool hovered, bool pressed) const override {
    return fromC(n8v_style_dropdown_paint(family_, open, hasSelection, hovered, pressed));
  }
  SliderPaint slider(bool hovered, bool pressed) const override { return fromC(n8v_style_slider_paint(family_, hovered, pressed)); }
  ImagePaint image() const override { return fromC(n8v_style_image_paint(family_)); }
  IconPaint icon() const override { return fromC(n8v_style_icon_paint(family_)); }
  SidebarPaint sidebar() const override { return fromC(n8v_style_sidebar_paint(family_)); }

private:
  n8v_style_family family_;
};

// Trampolines bridging the C-linkage vtable n8v_set_custom_paint expects back to a user's
// n8v::Paint subclass, passed through as `userdata`. Kept at internal linkage (no two TUs share
// these symbols) since ui.hpp is a header included into every translation unit that uses it.
extern "C" inline n8v_button_paint _n8v_custom_paint_button_trampoline(n8v_button_style style, bool hovered, bool pressed, void *userdata) {
  return toC(static_cast<const Paint *>(userdata)->button(fromC(style), hovered, pressed));
}
extern "C" inline n8v_text_paint _n8v_custom_paint_text_trampoline(n8v_text_options options, void *userdata) {
  return toC(static_cast<const Paint *>(userdata)->text(fromC(options)));
}
extern "C" inline n8v_checkbox_paint _n8v_custom_paint_checkbox_trampoline(bool checked, bool hovered, bool pressed, void *userdata) {
  return toC(static_cast<const Paint *>(userdata)->checkbox(checked, hovered, pressed));
}
extern "C" inline n8v_radio_paint _n8v_custom_paint_radio_trampoline(bool selected, bool hovered, bool pressed, void *userdata) {
  return toC(static_cast<const Paint *>(userdata)->radio(selected, hovered, pressed));
}
extern "C" inline n8v_toggle_paint _n8v_custom_paint_toggle_trampoline(bool on, bool hovered, bool pressed, void *userdata) {
  return toC(static_cast<const Paint *>(userdata)->toggle(on, hovered, pressed));
}
extern "C" inline n8v_entry_paint _n8v_custom_paint_entry_trampoline(bool focused, bool hasValue, void *userdata) {
  return toC(static_cast<const Paint *>(userdata)->entry(focused, hasValue));
}
extern "C" inline n8v_dropdown_paint _n8v_custom_paint_dropdown_trampoline(bool open, bool hasSelection, bool hovered, bool pressed, void *userdata) {
  return toC(static_cast<const Paint *>(userdata)->dropdown(open, hasSelection, hovered, pressed));
}
extern "C" inline n8v_slider_paint _n8v_custom_paint_slider_trampoline(bool hovered, bool pressed, void *userdata) {
  return toC(static_cast<const Paint *>(userdata)->slider(hovered, pressed));
}
extern "C" inline n8v_image_paint _n8v_custom_paint_image_trampoline(void *userdata) { return toC(static_cast<const Paint *>(userdata)->image()); }
extern "C" inline n8v_icon_paint _n8v_custom_paint_icon_trampoline(void *userdata) { return toC(static_cast<const Paint *>(userdata)->icon()); }
extern "C" inline n8v_sidebar_paint _n8v_custom_paint_sidebar_trampoline(void *userdata) { return toC(static_cast<const Paint *>(userdata)->sidebar()); }

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

inline void openSidebar(const SidebarOptions &options) {
  std::string titleStorage(options.title);
  n8v_sidebar_options c_opts{};
  c_opts.title = titleStorage.c_str();
  c_opts.selected = options.selected;
  if (options.onChange) callback_bridge::bridge(callback_bridge::intChangeClosures, options.onChange, c_opts.on_change, c_opts.on_change_userdata);
  c_opts.width = toC(options.width);
  c_opts.min_width = options.minWidth;
  c_opts.max_width = options.maxWidth;
  n8v_open_sidebar(c_opts);
}

inline void openSidebar(std::string_view title) { openSidebar(SidebarOptions{.title = title}); }

inline void closeSidebar() { n8v_close_sidebar(); }

inline bool openPage(const PageOptions &options) {
  std::string nameStorage(options.name);
  std::string iconStorage(options.icon);
  std::string imageStorage(options.image);
  n8v_page_options c_opts{};
  c_opts.name = nameStorage.c_str();
  c_opts.icon = iconStorage.empty() ? nullptr : iconStorage.c_str();
  c_opts.image = imageStorage.empty() ? nullptr : imageStorage.c_str();
  return n8v_open_page(c_opts);
}

inline bool openPage(std::string_view name) { return openPage(PageOptions{.name = name}); }

inline void closePage() { n8v_close_page(); }

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
      c_opts.strikethrough = textOptions.strikethrough;
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

struct ToggleBuilder {
  ToggleOptions options;

  void operator()(std::string_view label) && {
    std::string labelStorage(label);
    n8v_toggle_options c_opts{};
    c_opts.checked = options.checked;
    if (options.onChange) callback_bridge::bridge(callback_bridge::boolChangeClosures, std::move(options.onChange), c_opts.on_change, c_opts.on_change_userdata);
    _n8v_set_toggle_opts(c_opts);
    _n8v_toggle_commit(labelStorage.c_str());
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

/** Shorthand for text({})(label), for the common case of plain text with no styling options. */
inline void text(std::string_view label) { text(TextOptions{})(label); }

inline detail::CheckboxBuilder checkbox(CheckboxOptions options) { return detail::CheckboxBuilder{std::move(options)}; }

inline detail::ToggleBuilder toggle(ToggleOptions options) { return detail::ToggleBuilder{std::move(options)}; }

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

/**
 * The bundled styles' own Paint values, queryable directly regardless of which style is
 * currently active. A custom style typically holds a reference to one of these (usually
 * plainPaint()) and delegates to it for any widget it doesn't want to reimplement itself.
 */
inline const Paint &plainPaint() {
  static const detail::BuiltinPaint instance(N8V_STYLE_FAMILY_PLAIN);
  return instance;
}
inline const Paint &materialPaint() {
  static const detail::BuiltinPaint instance(N8V_STYLE_FAMILY_MATERIAL);
  return instance;
}
inline const Paint &cupertinoPaint() {
  static const detail::BuiltinPaint instance(N8V_STYLE_FAMILY_CUPERTINO);
  return instance;
}
inline const Paint &fluentPaint() {
  static const detail::BuiltinPaint instance(N8V_STYLE_FAMILY_FLUENT);
  return instance;
}

/**
 * Installs a custom Paint and switches to StyleFamily::Custom. Only affects the sdl2/html
 * backends - other backends draw with a native toolkit or, in the terminal's case, always force
 * Plain regardless of the active style.
 *
 * `paint` must stay alive for as long as the custom style is active (until shutdown, or until a
 * different style/custom Paint replaces it) - n8v only keeps a pointer to it.
 */
inline void setCustomPaint(Paint &paint) {
  n8v_custom_paint_vtable vtable{};
  vtable.button = detail::_n8v_custom_paint_button_trampoline;
  vtable.text = detail::_n8v_custom_paint_text_trampoline;
  vtable.checkbox = detail::_n8v_custom_paint_checkbox_trampoline;
  vtable.radio = detail::_n8v_custom_paint_radio_trampoline;
  vtable.toggle = detail::_n8v_custom_paint_toggle_trampoline;
  vtable.entry = detail::_n8v_custom_paint_entry_trampoline;
  vtable.dropdown = detail::_n8v_custom_paint_dropdown_trampoline;
  vtable.slider = detail::_n8v_custom_paint_slider_trampoline;
  vtable.image = detail::_n8v_custom_paint_image_trampoline;
  vtable.icon = detail::_n8v_custom_paint_icon_trampoline;
  vtable.sidebar = detail::_n8v_custom_paint_sidebar_trampoline;
  n8v_set_custom_paint(&vtable, &paint);
}

} // namespace n8v

#define UI() for (uint8_t n8v_uiLatch = (n8v::detail::beginFrame(), 0); n8v_uiLatch < 1; n8v_uiLatch = 1, n8v::detail::endFrame())

#define flex(...) for (uint8_t n8v_flexLatch = (n8v::detail::openFlex(__VA_ARGS__), 0); n8v_flexLatch < 1; n8v_flexLatch = 1, n8v::detail::closeFlex())

#define sidebar(...) for (uint8_t n8v_sidebarLatch = (n8v::detail::openSidebar(__VA_ARGS__), 0); n8v_sidebarLatch < 1; n8v_sidebarLatch = 1, n8v::detail::closeSidebar())

#define page(...)                                                                                                                                                             \
  for (bool n8v_pageOpened = n8v::detail::openPage(__VA_ARGS__), n8v_pageLatch = false; n8v_pageOpened && !n8v_pageLatch; n8v_pageLatch = true, n8v::detail::closePage())
