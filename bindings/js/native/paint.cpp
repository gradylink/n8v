#include <memory>

#include "marshal.hpp"

namespace n8v_napi {
namespace {

Napi::Object WriteColor(Napi::Env env, n8v_color c) {
  Napi::Object o = Napi::Object::New(env);
  o.Set("r", c.r);
  o.Set("g", c.g);
  o.Set("b", c.b);
  o.Set("a", c.a);
  return o;
}

Napi::Object WritePadding(Napi::Env env, n8v_padding p) {
  Napi::Object o = Napi::Object::New(env);
  o.Set("left", p.left);
  o.Set("right", p.right);
  o.Set("top", p.top);
  o.Set("bottom", p.bottom);
  return o;
}

Napi::Object WriteCornerRadius(Napi::Env env, n8v_corner_radius r) {
  Napi::Object o = Napi::Object::New(env);
  o.Set("topLeft", r.top_left);
  o.Set("topRight", r.top_right);
  o.Set("bottomLeft", r.bottom_left);
  o.Set("bottomRight", r.bottom_right);
  return o;
}

Napi::Object WriteTextOptions(Napi::Env env, const n8v_text_options &t) {
  Napi::Object o = Napi::Object::New(env);
  o.Set("bold", t.bold);
  o.Set("italic", t.italic);
  o.Set("strikethrough", t.strikethrough);
  o.Set("small", t.small);
  o.Set("darken", t.darken);
  o.Set("large", t.large);
  o.Set("clickUnstyled", t.click_unstyled);
  if (t.url) o.Set("url", Napi::String::New(env, t.url));
  o.Set("color", WriteColor(env, t.color));
  return o;
}

n8v_button_paint ReadButtonPaint(const Napi::Value &v) {
  n8v_button_paint p{};
  if (!v.IsObject()) return p;
  Napi::Object o = v.As<Napi::Object>();
  p.background = ReadColor(o.Get("background"));
  p.text_color = ReadColor(o.Get("textColor"));
  p.corner_radius = ReadCornerRadius(o.Get("cornerRadius"));
  p.padding = ReadPadding(o.Get("padding"));
  p.font = (n8v_font_family)EnumOr(o, "font", 0);
  p.font_size = U16Or(o, "fontSize", 0);
  p.transition_seconds = NumberOr(o, "transitionSeconds", 0.0f);
  return p;
}

Napi::Object WriteButtonPaint(Napi::Env env, const n8v_button_paint &p) {
  Napi::Object o = Napi::Object::New(env);
  o.Set("background", WriteColor(env, p.background));
  o.Set("textColor", WriteColor(env, p.text_color));
  o.Set("cornerRadius", WriteCornerRadius(env, p.corner_radius));
  o.Set("padding", WritePadding(env, p.padding));
  o.Set("font", Napi::Number::New(env, (int)p.font));
  o.Set("fontSize", Napi::Number::New(env, p.font_size));
  o.Set("transitionSeconds", Napi::Number::New(env, p.transition_seconds));
  return o;
}

n8v_text_paint ReadTextPaint(const Napi::Value &v) {
  n8v_text_paint p{};
  if (!v.IsObject()) return p;
  Napi::Object o = v.As<Napi::Object>();
  p.color = ReadColor(o.Get("color"));
  p.font = (n8v_font_family)EnumOr(o, "font", 0);
  p.font_size = U16Or(o, "fontSize", 0);
  return p;
}

Napi::Object WriteTextPaint(Napi::Env env, const n8v_text_paint &p) {
  Napi::Object o = Napi::Object::New(env);
  o.Set("color", WriteColor(env, p.color));
  o.Set("font", Napi::Number::New(env, (int)p.font));
  o.Set("fontSize", Napi::Number::New(env, p.font_size));
  return o;
}

n8v_checkbox_paint ReadCheckboxPaint(const Napi::Value &v) {
  n8v_checkbox_paint p{};
  if (!v.IsObject()) return p;
  Napi::Object o = v.As<Napi::Object>();
  p.background = ReadColor(o.Get("background"));
  p.border_color = ReadColor(o.Get("borderColor"));
  p.border_width = NumberOr(o, "borderWidth", 0.0f);
  p.check_color = ReadColor(o.Get("checkColor"));
  p.corner_radius = ReadCornerRadius(o.Get("cornerRadius"));
  p.indicator_size = NumberOr(o, "indicatorSize", 0.0f);
  p.padding = ReadPadding(o.Get("padding"));
  p.font = (n8v_font_family)EnumOr(o, "font", 0);
  p.font_size = U16Or(o, "fontSize", 0);
  p.transition_seconds = NumberOr(o, "transitionSeconds", 0.0f);
  p.check_mark = (int)EnumOr(o, "checkMark", 0);
  return p;
}

Napi::Object WriteCheckboxPaint(Napi::Env env, const n8v_checkbox_paint &p) {
  Napi::Object o = Napi::Object::New(env);
  o.Set("background", WriteColor(env, p.background));
  o.Set("borderColor", WriteColor(env, p.border_color));
  o.Set("borderWidth", Napi::Number::New(env, p.border_width));
  o.Set("checkColor", WriteColor(env, p.check_color));
  o.Set("cornerRadius", WriteCornerRadius(env, p.corner_radius));
  o.Set("indicatorSize", Napi::Number::New(env, p.indicator_size));
  o.Set("padding", WritePadding(env, p.padding));
  o.Set("font", Napi::Number::New(env, (int)p.font));
  o.Set("fontSize", Napi::Number::New(env, p.font_size));
  o.Set("transitionSeconds", Napi::Number::New(env, p.transition_seconds));
  o.Set("checkMark", Napi::Number::New(env, p.check_mark));
  return o;
}

n8v_radio_paint ReadRadioPaint(const Napi::Value &v) {
  n8v_radio_paint p{};
  if (!v.IsObject()) return p;
  Napi::Object o = v.As<Napi::Object>();
  p.background = ReadColor(o.Get("background"));
  p.border_color = ReadColor(o.Get("borderColor"));
  p.border_width = NumberOr(o, "borderWidth", 0.0f);
  p.dot_color = ReadColor(o.Get("dotColor"));
  p.indicator_size = NumberOr(o, "indicatorSize", 0.0f);
  p.padding = ReadPadding(o.Get("padding"));
  p.font = (n8v_font_family)EnumOr(o, "font", 0);
  p.font_size = U16Or(o, "fontSize", 0);
  p.transition_seconds = NumberOr(o, "transitionSeconds", 0.0f);
  return p;
}

Napi::Object WriteRadioPaint(Napi::Env env, const n8v_radio_paint &p) {
  Napi::Object o = Napi::Object::New(env);
  o.Set("background", WriteColor(env, p.background));
  o.Set("borderColor", WriteColor(env, p.border_color));
  o.Set("borderWidth", Napi::Number::New(env, p.border_width));
  o.Set("dotColor", WriteColor(env, p.dot_color));
  o.Set("indicatorSize", Napi::Number::New(env, p.indicator_size));
  o.Set("padding", WritePadding(env, p.padding));
  o.Set("font", Napi::Number::New(env, (int)p.font));
  o.Set("fontSize", Napi::Number::New(env, p.font_size));
  o.Set("transitionSeconds", Napi::Number::New(env, p.transition_seconds));
  return o;
}

n8v_toggle_paint ReadTogglePaint(const Napi::Value &v) {
  n8v_toggle_paint p{};
  if (!v.IsObject()) return p;
  Napi::Object o = v.As<Napi::Object>();
  p.track_on_color = ReadColor(o.Get("trackOnColor"));
  p.track_off_color = ReadColor(o.Get("trackOffColor"));
  p.track_border_color = ReadColor(o.Get("trackBorderColor"));
  p.track_border_width = NumberOr(o, "trackBorderWidth", 0.0f);
  p.knob_on_color = ReadColor(o.Get("knobOnColor"));
  p.knob_off_color = ReadColor(o.Get("knobOffColor"));
  p.knob_glyph_color = ReadColor(o.Get("knobGlyphColor"));
  p.show_glyph_when_on = BoolOr(o, "showGlyphWhenOn", false);
  p.track_width = NumberOr(o, "trackWidth", 0.0f);
  p.track_height = NumberOr(o, "trackHeight", 0.0f);
  p.knob_size_off = NumberOr(o, "knobSizeOff", 0.0f);
  p.knob_size_on = NumberOr(o, "knobSizeOn", 0.0f);
  p.padding = ReadPadding(o.Get("padding"));
  p.font = (n8v_font_family)EnumOr(o, "font", 0);
  p.font_size = U16Or(o, "fontSize", 0);
  p.transition_seconds = NumberOr(o, "transitionSeconds", 0.0f);
  return p;
}

Napi::Object WriteTogglePaint(Napi::Env env, const n8v_toggle_paint &p) {
  Napi::Object o = Napi::Object::New(env);
  o.Set("trackOnColor", WriteColor(env, p.track_on_color));
  o.Set("trackOffColor", WriteColor(env, p.track_off_color));
  o.Set("trackBorderColor", WriteColor(env, p.track_border_color));
  o.Set("trackBorderWidth", Napi::Number::New(env, p.track_border_width));
  o.Set("knobOnColor", WriteColor(env, p.knob_on_color));
  o.Set("knobOffColor", WriteColor(env, p.knob_off_color));
  o.Set("knobGlyphColor", WriteColor(env, p.knob_glyph_color));
  o.Set("showGlyphWhenOn", Napi::Boolean::New(env, p.show_glyph_when_on));
  o.Set("trackWidth", Napi::Number::New(env, p.track_width));
  o.Set("trackHeight", Napi::Number::New(env, p.track_height));
  o.Set("knobSizeOff", Napi::Number::New(env, p.knob_size_off));
  o.Set("knobSizeOn", Napi::Number::New(env, p.knob_size_on));
  o.Set("padding", WritePadding(env, p.padding));
  o.Set("font", Napi::Number::New(env, (int)p.font));
  o.Set("fontSize", Napi::Number::New(env, p.font_size));
  o.Set("transitionSeconds", Napi::Number::New(env, p.transition_seconds));
  return o;
}

n8v_entry_paint ReadEntryPaint(const Napi::Value &v) {
  n8v_entry_paint p{};
  if (!v.IsObject()) return p;
  Napi::Object o = v.As<Napi::Object>();
  p.background = ReadColor(o.Get("background"));
  p.text_color = ReadColor(o.Get("textColor"));
  p.placeholder_color = ReadColor(o.Get("placeholderColor"));
  p.border_color = ReadColor(o.Get("borderColor"));
  p.border_width = NumberOr(o, "borderWidth", 0.0f);
  p.outlined = BoolOr(o, "outlined", false);
  p.label_color = ReadColor(o.Get("labelColor"));
  p.label_font_size = U16Or(o, "labelFontSize", 0);
  p.transition_seconds = NumberOr(o, "transitionSeconds", 0.0f);
  p.corner_radius = ReadCornerRadius(o.Get("cornerRadius"));
  p.padding = ReadPadding(o.Get("padding"));
  p.font = (n8v_font_family)EnumOr(o, "font", 0);
  p.font_size = U16Or(o, "fontSize", 0);
  return p;
}

Napi::Object WriteEntryPaint(Napi::Env env, const n8v_entry_paint &p) {
  Napi::Object o = Napi::Object::New(env);
  o.Set("background", WriteColor(env, p.background));
  o.Set("textColor", WriteColor(env, p.text_color));
  o.Set("placeholderColor", WriteColor(env, p.placeholder_color));
  o.Set("borderColor", WriteColor(env, p.border_color));
  o.Set("borderWidth", Napi::Number::New(env, p.border_width));
  o.Set("outlined", Napi::Boolean::New(env, p.outlined));
  o.Set("labelColor", WriteColor(env, p.label_color));
  o.Set("labelFontSize", Napi::Number::New(env, p.label_font_size));
  o.Set("transitionSeconds", Napi::Number::New(env, p.transition_seconds));
  o.Set("cornerRadius", WriteCornerRadius(env, p.corner_radius));
  o.Set("padding", WritePadding(env, p.padding));
  o.Set("font", Napi::Number::New(env, (int)p.font));
  o.Set("fontSize", Napi::Number::New(env, p.font_size));
  return o;
}

n8v_dropdown_paint ReadDropdownPaint(const Napi::Value &v) {
  n8v_dropdown_paint p{};
  if (!v.IsObject()) return p;
  Napi::Object o = v.As<Napi::Object>();
  p.background = ReadColor(o.Get("background"));
  p.text_color = ReadColor(o.Get("textColor"));
  p.placeholder_color = ReadColor(o.Get("placeholderColor"));
  p.popup_background = ReadColor(o.Get("popupBackground"));
  p.item_hover_background = ReadColor(o.Get("itemHoverBackground"));
  p.item_selected_background = ReadColor(o.Get("itemSelectedBackground"));
  p.corner_radius = ReadCornerRadius(o.Get("cornerRadius"));
  p.padding = ReadPadding(o.Get("padding"));
  p.font = (n8v_font_family)EnumOr(o, "font", 0);
  p.font_size = U16Or(o, "fontSize", 0);
  p.transition_seconds = NumberOr(o, "transitionSeconds", 0.0f);
  p.label_color = ReadColor(o.Get("labelColor"));
  p.label_font_size = U16Or(o, "labelFontSize", 0);
  p.indicator_color = ReadColor(o.Get("indicatorColor"));
  p.indicator_width = NumberOr(o, "indicatorWidth", 0.0f);
  return p;
}

Napi::Object WriteDropdownPaint(Napi::Env env, const n8v_dropdown_paint &p) {
  Napi::Object o = Napi::Object::New(env);
  o.Set("background", WriteColor(env, p.background));
  o.Set("textColor", WriteColor(env, p.text_color));
  o.Set("placeholderColor", WriteColor(env, p.placeholder_color));
  o.Set("popupBackground", WriteColor(env, p.popup_background));
  o.Set("itemHoverBackground", WriteColor(env, p.item_hover_background));
  o.Set("itemSelectedBackground", WriteColor(env, p.item_selected_background));
  o.Set("cornerRadius", WriteCornerRadius(env, p.corner_radius));
  o.Set("padding", WritePadding(env, p.padding));
  o.Set("font", Napi::Number::New(env, (int)p.font));
  o.Set("fontSize", Napi::Number::New(env, p.font_size));
  o.Set("transitionSeconds", Napi::Number::New(env, p.transition_seconds));
  o.Set("labelColor", WriteColor(env, p.label_color));
  o.Set("labelFontSize", Napi::Number::New(env, p.label_font_size));
  o.Set("indicatorColor", WriteColor(env, p.indicator_color));
  o.Set("indicatorWidth", Napi::Number::New(env, p.indicator_width));
  return o;
}

n8v_slider_paint ReadSliderPaint(const Napi::Value &v) {
  n8v_slider_paint p{};
  if (!v.IsObject()) return p;
  Napi::Object o = v.As<Napi::Object>();
  p.track_color = ReadColor(o.Get("trackColor"));
  p.fill_color = ReadColor(o.Get("fillColor"));
  p.thumb_color = ReadColor(o.Get("thumbColor"));
  p.track_height = NumberOr(o, "trackHeight", 0.0f);
  p.thumb_width = NumberOr(o, "thumbWidth", 0.0f);
  p.thumb_height = NumberOr(o, "thumbHeight", 0.0f);
  p.thumb_border_color = ReadColor(o.Get("thumbBorderColor"));
  p.thumb_border_width = NumberOr(o, "thumbBorderWidth", 0.0f);
  p.track_gap = NumberOr(o, "trackGap", 0.0f);
  return p;
}

Napi::Object WriteSliderPaint(Napi::Env env, const n8v_slider_paint &p) {
  Napi::Object o = Napi::Object::New(env);
  o.Set("trackColor", WriteColor(env, p.track_color));
  o.Set("fillColor", WriteColor(env, p.fill_color));
  o.Set("thumbColor", WriteColor(env, p.thumb_color));
  o.Set("trackHeight", Napi::Number::New(env, p.track_height));
  o.Set("thumbWidth", Napi::Number::New(env, p.thumb_width));
  o.Set("thumbHeight", Napi::Number::New(env, p.thumb_height));
  o.Set("thumbBorderColor", WriteColor(env, p.thumb_border_color));
  o.Set("thumbBorderWidth", Napi::Number::New(env, p.thumb_border_width));
  o.Set("trackGap", Napi::Number::New(env, p.track_gap));
  return o;
}

n8v_image_paint ReadImagePaint(const Napi::Value &v) {
  n8v_image_paint p{};
  if (!v.IsObject()) return p;
  Napi::Object o = v.As<Napi::Object>();
  p.corner_radius = ReadCornerRadius(o.Get("cornerRadius"));
  return p;
}

Napi::Object WriteImagePaint(Napi::Env env, const n8v_image_paint &p) {
  Napi::Object o = Napi::Object::New(env);
  o.Set("cornerRadius", WriteCornerRadius(env, p.corner_radius));
  return o;
}

n8v_icon_paint ReadIconPaint(const Napi::Value &v) {
  n8v_icon_paint p{};
  if (!v.IsObject()) return p;
  Napi::Object o = v.As<Napi::Object>();
  p.tint = ReadColor(o.Get("tint"));
  p.default_size = NumberOr(o, "defaultSize", 0.0f);
  return p;
}

Napi::Object WriteIconPaint(Napi::Env env, const n8v_icon_paint &p) {
  Napi::Object o = Napi::Object::New(env);
  o.Set("tint", WriteColor(env, p.tint));
  o.Set("defaultSize", Napi::Number::New(env, p.default_size));
  return o;
}

n8v_sidebar_paint ReadSidebarPaint(const Napi::Value &v) {
  n8v_sidebar_paint p{};
  if (!v.IsObject()) return p;
  Napi::Object o = v.As<Napi::Object>();
  p.background = ReadColor(o.Get("background"));
  p.border_color = ReadColor(o.Get("borderColor"));
  p.border_width = NumberOr(o, "borderWidth", 0.0f);
  p.corner_radius = ReadCornerRadius(o.Get("cornerRadius"));
  p.padding = ReadPadding(o.Get("padding"));
  p.row_gap = NumberOr(o, "rowGap", 0.0f);
  p.row_background = ReadColor(o.Get("rowBackground"));
  p.row_hover_background = ReadColor(o.Get("rowHoverBackground"));
  p.row_selected_background = ReadColor(o.Get("rowSelectedBackground"));
  p.row_text_color = ReadColor(o.Get("rowTextColor"));
  p.row_selected_text_color = ReadColor(o.Get("rowSelectedTextColor"));
  p.row_corner_radius = NumberOr(o, "rowCornerRadius", 0.0f);
  p.row_padding = ReadPadding(o.Get("rowPadding"));
  return p;
}

Napi::Object WriteSidebarPaint(Napi::Env env, const n8v_sidebar_paint &p) {
  Napi::Object o = Napi::Object::New(env);
  o.Set("background", WriteColor(env, p.background));
  o.Set("borderColor", WriteColor(env, p.border_color));
  o.Set("borderWidth", Napi::Number::New(env, p.border_width));
  o.Set("cornerRadius", WriteCornerRadius(env, p.corner_radius));
  o.Set("padding", WritePadding(env, p.padding));
  o.Set("rowGap", Napi::Number::New(env, p.row_gap));
  o.Set("rowBackground", WriteColor(env, p.row_background));
  o.Set("rowHoverBackground", WriteColor(env, p.row_hover_background));
  o.Set("rowSelectedBackground", WriteColor(env, p.row_selected_background));
  o.Set("rowTextColor", WriteColor(env, p.row_text_color));
  o.Set("rowSelectedTextColor", WriteColor(env, p.row_selected_text_color));
  o.Set("rowCornerRadius", Napi::Number::New(env, p.row_corner_radius));
  o.Set("rowPadding", WritePadding(env, p.row_padding));
  return o;
}

n8v_panel_paint ReadPanelPaint(const Napi::Value &v) {
  n8v_panel_paint p{};
  if (!v.IsObject()) return p;
  Napi::Object o = v.As<Napi::Object>();
  p.background = ReadColor(o.Get("background"));
  p.border_color = ReadColor(o.Get("borderColor"));
  p.border_width = NumberOr(o, "borderWidth", 0.0f);
  p.corner_radius = ReadCornerRadius(o.Get("cornerRadius"));
  p.padding = ReadPadding(o.Get("padding"));
  return p;
}

Napi::Object WritePanelPaint(Napi::Env env, const n8v_panel_paint &p) {
  Napi::Object o = Napi::Object::New(env);
  o.Set("background", WriteColor(env, p.background));
  o.Set("borderColor", WriteColor(env, p.border_color));
  o.Set("borderWidth", Napi::Number::New(env, p.border_width));
  o.Set("cornerRadius", WriteCornerRadius(env, p.corner_radius));
  o.Set("padding", WritePadding(env, p.padding));
  return o;
}

struct PaintFunctions {
  std::unique_ptr<Napi::FunctionReference> button;
  std::unique_ptr<Napi::FunctionReference> text;
  std::unique_ptr<Napi::FunctionReference> checkbox;
  std::unique_ptr<Napi::FunctionReference> radio;
  std::unique_ptr<Napi::FunctionReference> toggle;
  std::unique_ptr<Napi::FunctionReference> entry;
  std::unique_ptr<Napi::FunctionReference> dropdown;
  std::unique_ptr<Napi::FunctionReference> slider;
  std::unique_ptr<Napi::FunctionReference> image;
  std::unique_ptr<Napi::FunctionReference> icon;
  std::unique_ptr<Napi::FunctionReference> sidebar;
  std::unique_ptr<Napi::FunctionReference> panel;
  std::unique_ptr<Napi::FunctionReference> windowBackground;
};
PaintFunctions g_paint;

n8v_button_paint ButtonTrampoline(n8v_button_style style, bool hovered, bool pressed, void *) {
  Napi::FunctionReference &fn = *g_paint.button;
  Napi::Env env = fn.Env();
  Napi::Value result = fn.Call({Napi::Number::New(env, (int)style), Napi::Boolean::New(env, hovered), Napi::Boolean::New(env, pressed)});
  return ReadButtonPaint(result);
}

n8v_text_paint TextTrampoline(n8v_text_options options, void *) {
  Napi::FunctionReference &fn = *g_paint.text;
  Napi::Env env = fn.Env();
  Napi::Value result = fn.Call({WriteTextOptions(env, options)});
  return ReadTextPaint(result);
}

n8v_checkbox_paint CheckboxTrampoline(bool checked, bool hovered, bool pressed, void *) {
  Napi::FunctionReference &fn = *g_paint.checkbox;
  Napi::Env env = fn.Env();
  Napi::Value result = fn.Call({Napi::Boolean::New(env, checked), Napi::Boolean::New(env, hovered), Napi::Boolean::New(env, pressed)});
  return ReadCheckboxPaint(result);
}

n8v_radio_paint RadioTrampoline(bool selected, bool hovered, bool pressed, void *) {
  Napi::FunctionReference &fn = *g_paint.radio;
  Napi::Env env = fn.Env();
  Napi::Value result = fn.Call({Napi::Boolean::New(env, selected), Napi::Boolean::New(env, hovered), Napi::Boolean::New(env, pressed)});
  return ReadRadioPaint(result);
}

n8v_toggle_paint ToggleTrampoline(bool on, bool hovered, bool pressed, void *) {
  Napi::FunctionReference &fn = *g_paint.toggle;
  Napi::Env env = fn.Env();
  Napi::Value result = fn.Call({Napi::Boolean::New(env, on), Napi::Boolean::New(env, hovered), Napi::Boolean::New(env, pressed)});
  return ReadTogglePaint(result);
}

n8v_entry_paint EntryTrampoline(bool focused, bool hasValue, void *) {
  Napi::FunctionReference &fn = *g_paint.entry;
  Napi::Env env = fn.Env();
  Napi::Value result = fn.Call({Napi::Boolean::New(env, focused), Napi::Boolean::New(env, hasValue)});
  return ReadEntryPaint(result);
}

n8v_dropdown_paint DropdownTrampoline(bool open, bool hasSelection, bool hovered, bool pressed, void *) {
  Napi::FunctionReference &fn = *g_paint.dropdown;
  Napi::Env env = fn.Env();
  Napi::Value result = fn.Call({Napi::Boolean::New(env, open), Napi::Boolean::New(env, hasSelection), Napi::Boolean::New(env, hovered), Napi::Boolean::New(env, pressed)});
  return ReadDropdownPaint(result);
}

n8v_slider_paint SliderTrampoline(bool hovered, bool pressed, void *) {
  Napi::FunctionReference &fn = *g_paint.slider;
  Napi::Env env = fn.Env();
  Napi::Value result = fn.Call({Napi::Boolean::New(env, hovered), Napi::Boolean::New(env, pressed)});
  return ReadSliderPaint(result);
}

n8v_image_paint ImageTrampoline(void *) {
  Napi::FunctionReference &fn = *g_paint.image;
  Napi::Value result = fn.Call({});
  return ReadImagePaint(result);
}

n8v_icon_paint IconTrampoline(void *) {
  Napi::FunctionReference &fn = *g_paint.icon;
  Napi::Value result = fn.Call({});
  return ReadIconPaint(result);
}

n8v_sidebar_paint SidebarTrampoline(void *) {
  Napi::FunctionReference &fn = *g_paint.sidebar;
  Napi::Value result = fn.Call({});
  return ReadSidebarPaint(result);
}

n8v_panel_paint PanelTrampoline(n8v_panel_role role, bool hovered, bool pressed, void *) {
  Napi::FunctionReference &fn = *g_paint.panel;
  Napi::Env env = fn.Env();
  Napi::Value result = fn.Call({Napi::Number::New(env, (int)role), Napi::Boolean::New(env, hovered), Napi::Boolean::New(env, pressed)});
  return ReadPanelPaint(result);
}

n8v_color WindowBackgroundTrampoline(void *) {
  Napi::FunctionReference &fn = *g_paint.windowBackground;
  Napi::Value result = fn.Call({});
  return ReadColor(result);
}

Napi::Value SetCustomPaint(const Napi::CallbackInfo &info) {
  Napi::Object o = info[0].As<Napi::Object>();
  n8v_custom_paint_vtable vt{};
  g_paint = PaintFunctions{};
  if (o.Get("button").IsFunction()) {
    g_paint.button = std::make_unique<Napi::FunctionReference>(Napi::Persistent(o.Get("button").As<Napi::Function>()));
    vt.button = ButtonTrampoline;
  }
  if (o.Get("text").IsFunction()) {
    g_paint.text = std::make_unique<Napi::FunctionReference>(Napi::Persistent(o.Get("text").As<Napi::Function>()));
    vt.text = TextTrampoline;
  }
  if (o.Get("checkbox").IsFunction()) {
    g_paint.checkbox = std::make_unique<Napi::FunctionReference>(Napi::Persistent(o.Get("checkbox").As<Napi::Function>()));
    vt.checkbox = CheckboxTrampoline;
  }
  if (o.Get("radio").IsFunction()) {
    g_paint.radio = std::make_unique<Napi::FunctionReference>(Napi::Persistent(o.Get("radio").As<Napi::Function>()));
    vt.radio = RadioTrampoline;
  }
  if (o.Get("toggle").IsFunction()) {
    g_paint.toggle = std::make_unique<Napi::FunctionReference>(Napi::Persistent(o.Get("toggle").As<Napi::Function>()));
    vt.toggle = ToggleTrampoline;
  }
  if (o.Get("entry").IsFunction()) {
    g_paint.entry = std::make_unique<Napi::FunctionReference>(Napi::Persistent(o.Get("entry").As<Napi::Function>()));
    vt.entry = EntryTrampoline;
  }
  if (o.Get("dropdown").IsFunction()) {
    g_paint.dropdown = std::make_unique<Napi::FunctionReference>(Napi::Persistent(o.Get("dropdown").As<Napi::Function>()));
    vt.dropdown = DropdownTrampoline;
  }
  if (o.Get("slider").IsFunction()) {
    g_paint.slider = std::make_unique<Napi::FunctionReference>(Napi::Persistent(o.Get("slider").As<Napi::Function>()));
    vt.slider = SliderTrampoline;
  }
  if (o.Get("image").IsFunction()) {
    g_paint.image = std::make_unique<Napi::FunctionReference>(Napi::Persistent(o.Get("image").As<Napi::Function>()));
    vt.image = ImageTrampoline;
  }
  if (o.Get("icon").IsFunction()) {
    g_paint.icon = std::make_unique<Napi::FunctionReference>(Napi::Persistent(o.Get("icon").As<Napi::Function>()));
    vt.icon = IconTrampoline;
  }
  if (o.Get("sidebar").IsFunction()) {
    g_paint.sidebar = std::make_unique<Napi::FunctionReference>(Napi::Persistent(o.Get("sidebar").As<Napi::Function>()));
    vt.sidebar = SidebarTrampoline;
  }
  if (o.Get("panel").IsFunction()) {
    g_paint.panel = std::make_unique<Napi::FunctionReference>(Napi::Persistent(o.Get("panel").As<Napi::Function>()));
    vt.panel = PanelTrampoline;
  }
  if (o.Get("windowBackground").IsFunction()) {
    g_paint.windowBackground = std::make_unique<Napi::FunctionReference>(Napi::Persistent(o.Get("windowBackground").As<Napi::Function>()));
    vt.window_background = WindowBackgroundTrampoline;
  }
  n8v_set_custom_paint(&vt, nullptr);
  return info.Env().Undefined();
}

n8v_text_options ReadTextOptionsForQuery(const Napi::Value &v) {
  static std::string urlStorage;
  if (!v.IsObject()) return n8v_text_options{};
  return ReadTextOptions(v.As<Napi::Object>(), urlStorage);
}

Napi::Value StyleButtonPaint(const Napi::CallbackInfo &info) {
  return WriteButtonPaint(
    info.Env(),
    n8v_style_button_paint(
      (n8v_style_family)info[0].As<Napi::Number>().Int32Value(),
      (n8v_button_style)info[1].As<Napi::Number>().Int32Value(),
      info[2].ToBoolean().Value(),
      info[3].ToBoolean().Value()
    )
  );
}

Napi::Value StyleTextPaint(const Napi::CallbackInfo &info) {
  return WriteTextPaint(info.Env(), n8v_style_text_paint((n8v_style_family)info[0].As<Napi::Number>().Int32Value(), ReadTextOptionsForQuery(info[1])));
}

Napi::Value StyleCheckboxPaint(const Napi::CallbackInfo &info) {
  return WriteCheckboxPaint(
    info.Env(),
    n8v_style_checkbox_paint((n8v_style_family)info[0].As<Napi::Number>().Int32Value(), info[1].ToBoolean().Value(), info[2].ToBoolean().Value(), info[3].ToBoolean().Value())
  );
}

Napi::Value StyleRadioPaint(const Napi::CallbackInfo &info) {
  return WriteRadioPaint(
    info.Env(),
    n8v_style_radio_paint((n8v_style_family)info[0].As<Napi::Number>().Int32Value(), info[1].ToBoolean().Value(), info[2].ToBoolean().Value(), info[3].ToBoolean().Value())
  );
}

Napi::Value StyleTogglePaint(const Napi::CallbackInfo &info) {
  return WriteTogglePaint(
    info.Env(),
    n8v_style_toggle_paint((n8v_style_family)info[0].As<Napi::Number>().Int32Value(), info[1].ToBoolean().Value(), info[2].ToBoolean().Value(), info[3].ToBoolean().Value())
  );
}

Napi::Value StyleEntryPaint(const Napi::CallbackInfo &info) {
  return WriteEntryPaint(
    info.Env(),
    n8v_style_entry_paint((n8v_style_family)info[0].As<Napi::Number>().Int32Value(), info[1].ToBoolean().Value(), info[2].ToBoolean().Value())
  );
}

Napi::Value StyleDropdownPaint(const Napi::CallbackInfo &info) {
  return WriteDropdownPaint(
    info.Env(),
    n8v_style_dropdown_paint(
      (n8v_style_family)info[0].As<Napi::Number>().Int32Value(),
      info[1].ToBoolean().Value(),
      info[2].ToBoolean().Value(),
      info[3].ToBoolean().Value(),
      info[4].ToBoolean().Value()
    )
  );
}

Napi::Value StyleSliderPaint(const Napi::CallbackInfo &info) {
  return WriteSliderPaint(
    info.Env(),
    n8v_style_slider_paint((n8v_style_family)info[0].As<Napi::Number>().Int32Value(), info[1].ToBoolean().Value(), info[2].ToBoolean().Value())
  );
}

Napi::Value StyleImagePaint(const Napi::CallbackInfo &info) {
  return WriteImagePaint(info.Env(), n8v_style_image_paint((n8v_style_family)info[0].As<Napi::Number>().Int32Value()));
}

Napi::Value StyleIconPaint(const Napi::CallbackInfo &info) {
  return WriteIconPaint(info.Env(), n8v_style_icon_paint((n8v_style_family)info[0].As<Napi::Number>().Int32Value()));
}

Napi::Value StyleSidebarPaint(const Napi::CallbackInfo &info) {
  return WriteSidebarPaint(info.Env(), n8v_style_sidebar_paint((n8v_style_family)info[0].As<Napi::Number>().Int32Value()));
}

Napi::Value StylePanelPaint(const Napi::CallbackInfo &info) {
  return WritePanelPaint(
    info.Env(),
    n8v_style_panel_paint(
      (n8v_style_family)info[0].As<Napi::Number>().Int32Value(),
      (n8v_panel_role)info[1].As<Napi::Number>().Int32Value(),
      info[2].ToBoolean().Value(),
      info[3].ToBoolean().Value()
    )
  );
}

} // namespace

void InitPaint(Napi::Env env, Napi::Object exports) {
  exports.Set("setCustomPaint", Napi::Function::New(env, SetCustomPaint));
  exports.Set("styleButtonPaint", Napi::Function::New(env, StyleButtonPaint));
  exports.Set("styleTextPaint", Napi::Function::New(env, StyleTextPaint));
  exports.Set("styleCheckboxPaint", Napi::Function::New(env, StyleCheckboxPaint));
  exports.Set("styleRadioPaint", Napi::Function::New(env, StyleRadioPaint));
  exports.Set("styleTogglePaint", Napi::Function::New(env, StyleTogglePaint));
  exports.Set("styleEntryPaint", Napi::Function::New(env, StyleEntryPaint));
  exports.Set("styleDropdownPaint", Napi::Function::New(env, StyleDropdownPaint));
  exports.Set("styleSliderPaint", Napi::Function::New(env, StyleSliderPaint));
  exports.Set("styleImagePaint", Napi::Function::New(env, StyleImagePaint));
  exports.Set("styleIconPaint", Napi::Function::New(env, StyleIconPaint));
  exports.Set("styleSidebarPaint", Napi::Function::New(env, StyleSidebarPaint));
  exports.Set("stylePanelPaint", Napi::Function::New(env, StylePanelPaint));
}

} // namespace n8v_napi
