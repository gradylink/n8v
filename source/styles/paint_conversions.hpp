#pragma once

#include "core/style.hpp"

#include <n8v/n8v_c.h>

#include <string>

namespace n8v::detail {

inline n8v_color toC(Color c) { return n8v_color{c.r, c.g, c.b, c.a}; }
inline Color fromC(n8v_color c) { return Color{c.r, c.g, c.b, c.a}; }

inline n8v_padding toC(Padding p) { return n8v_padding{p.left, p.right, p.top, p.bottom}; }
inline Padding fromC(n8v_padding p) { return Padding{p.left, p.right, p.top, p.bottom}; }

inline n8v_corner_radius toC(CornerRadius r) { return n8v_corner_radius{r.topLeft, r.topRight, r.bottomLeft, r.bottomRight}; }
inline CornerRadius fromC(n8v_corner_radius r) { return CornerRadius{r.top_left, r.top_right, r.bottom_left, r.bottom_right}; }

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

inline n8v_button_style toC(ButtonStyle s) {
  switch (s) {
  case ButtonStyle::Secondary:
    return N8V_BUTTON_STYLE_SECONDARY;
  case ButtonStyle::Ghost:
    return N8V_BUTTON_STYLE_GHOST;
  case ButtonStyle::Primary:
    return N8V_BUTTON_STYLE_PRIMARY;
  }
  return N8V_BUTTON_STYLE_PRIMARY;
}
inline ButtonStyle fromC(n8v_button_style s) {
  switch (s) {
  case N8V_BUTTON_STYLE_SECONDARY:
    return ButtonStyle::Secondary;
  case N8V_BUTTON_STYLE_GHOST:
    return ButtonStyle::Ghost;
  case N8V_BUTTON_STYLE_PRIMARY:
    return ButtonStyle::Primary;
  }
  return ButtonStyle::Primary;
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

inline TextOptions fromC(n8v_text_options options) {
  return TextOptions{
    .bold = options.bold,
    .italic = options.italic,
    .strikethrough = options.strikethrough,
    .url = options.url ? std::string_view(options.url) : std::string_view{},
    .color = fromC(options.color),
  };
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

inline TextPaint fromC(n8v_text_paint p) {
  return TextPaint{
    .color = fromC(p.color),
    .font = fromC(p.font),
    .fontSize = p.font_size,
  };
}

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

} // namespace n8v::detail
