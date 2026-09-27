#include "marshal.hpp"

namespace n8v_napi {

float NumberOr(const Napi::Object &o, const char *key, float fallback) {
  Napi::Value v = o.Get(key);
  return v.IsNumber() ? v.As<Napi::Number>().FloatValue() : fallback;
}

uint16_t U16Or(const Napi::Object &o, const char *key, uint16_t fallback) {
  Napi::Value v = o.Get(key);
  return v.IsNumber() ? (uint16_t)v.As<Napi::Number>().Uint32Value() : fallback;
}

int32_t EnumOr(const Napi::Object &o, const char *key, int32_t fallback) {
  Napi::Value v = o.Get(key);
  return v.IsNumber() ? v.As<Napi::Number>().Int32Value() : fallback;
}

bool BoolOr(const Napi::Object &o, const char *key, bool fallback) {
  Napi::Value v = o.Get(key);
  return v.IsBoolean() ? v.As<Napi::Boolean>().Value() : fallback;
}

n8v_sizing ReadSizing(const Napi::Value &v) {
  n8v_sizing s{};
  if (!v.IsObject()) return n8v_sizing_fit(0, 0);
  Napi::Object o = v.As<Napi::Object>();
  s.mode = (n8v_sizing_mode)EnumOr(o, "mode", N8V_SIZING_FIT);
  s.value = NumberOr(o, "value", 0.0f);
  s.min = NumberOr(o, "min", 0.0f);
  s.max = NumberOr(o, "max", 0.0f);
  return s;
}

n8v_color ReadColor(const Napi::Value &v) {
  n8v_color c{0, 0, 0, 0};
  if (!v.IsObject()) return c;
  Napi::Object o = v.As<Napi::Object>();
  c.r = NumberOr(o, "r", 0.0f);
  c.g = NumberOr(o, "g", 0.0f);
  c.b = NumberOr(o, "b", 0.0f);
  c.a = NumberOr(o, "a", 0.0f);
  return c;
}

n8v_padding ReadPadding(const Napi::Value &v) {
  n8v_padding p{0, 0, 0, 0};
  if (!v.IsObject()) return p;
  Napi::Object o = v.As<Napi::Object>();
  p.left = U16Or(o, "left", 0);
  p.right = U16Or(o, "right", 0);
  p.top = U16Or(o, "top", 0);
  p.bottom = U16Or(o, "bottom", 0);
  return p;
}

n8v_corner_radius ReadCornerRadius(const Napi::Value &v) {
  n8v_corner_radius r{0, 0, 0, 0};
  if (!v.IsObject()) return r;
  Napi::Object o = v.As<Napi::Object>();
  r.top_left = NumberOr(o, "topLeft", 0.0f);
  r.top_right = NumberOr(o, "topRight", 0.0f);
  r.bottom_left = NumberOr(o, "bottomLeft", 0.0f);
  r.bottom_right = NumberOr(o, "bottomRight", 0.0f);
  return r;
}

n8v_rounding ReadRounding(const Napi::Value &v) {
  if (!v.IsObject()) return n8v_rounding_style_default();
  Napi::Object o = v.As<Napi::Object>();
  n8v_rounding r{};
  r.mode = (n8v_rounding_mode)EnumOr(o, "mode", N8V_ROUNDING_STYLE_DEFAULT);
  r.radius = ReadCornerRadius(o.Get("radius"));
  return r;
}

n8v_flex_options ReadFlexOptions(const Napi::Object &o) {
  n8v_flex_options f{};
  f.direction = (n8v_direction)EnumOr(o, "direction", N8V_DIRECTION_HORIZONTAL);
  f.gap = U16Or(o, "gap", 0);
  f.padding = ReadPadding(o.Get("padding"));
  f.h_align = (n8v_align)EnumOr(o, "hAlign", N8V_ALIGN_START);
  f.v_align = (n8v_align)EnumOr(o, "vAlign", N8V_ALIGN_START);
  f.width = ReadSizing(o.Get("width"));
  f.height = ReadSizing(o.Get("height"));
  f.clip_horizontal = BoolOr(o, "clipHorizontal", false);
  f.clip_vertical = BoolOr(o, "clipVertical", false);
  return f;
}

n8v_panel_options ReadPanelOptions(const Napi::Object &o) {
  n8v_panel_options p{};
  p.role = (n8v_panel_role)EnumOr(o, "role", N8V_PANEL_ROLE_CARD);
  p.direction = (n8v_direction)EnumOr(o, "direction", N8V_DIRECTION_VERTICAL);
  p.gap = U16Or(o, "gap", 0);
  p.h_align = (n8v_align)EnumOr(o, "hAlign", N8V_ALIGN_START);
  p.v_align = (n8v_align)EnumOr(o, "vAlign", N8V_ALIGN_START);
  p.width = ReadSizing(o.Get("width"));
  p.height = ReadSizing(o.Get("height"));
  p.clip_horizontal = BoolOr(o, "clipHorizontal", false);
  p.clip_vertical = BoolOr(o, "clipVertical", false);
  return p;
}

n8v_text_options ReadTextOptions(const Napi::Object &o, std::string &urlStorage) {
  n8v_text_options t{};
  t.bold = BoolOr(o, "bold", false);
  t.italic = BoolOr(o, "italic", false);
  t.strikethrough = BoolOr(o, "strikethrough", false);
  Napi::Value url = o.Get("url");
  if (url.IsString()) {
    urlStorage = url.As<Napi::String>().Utf8Value();
    t.url = urlStorage.c_str();
  } else {
    t.url = nullptr;
  }
  t.color = ReadColor(o.Get("color"));
  return t;
}

} // namespace n8v_napi
