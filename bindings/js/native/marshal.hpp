#pragma once

#include <n8v/n8v_c.h>
#include <napi.h>

#include <string>

namespace n8v_napi {

float NumberOr(const Napi::Object &o, const char *key, float fallback);
uint16_t U16Or(const Napi::Object &o, const char *key, uint16_t fallback);
int32_t EnumOr(const Napi::Object &o, const char *key, int32_t fallback);
bool BoolOr(const Napi::Object &o, const char *key, bool fallback);

n8v_sizing ReadSizing(const Napi::Value &v);
n8v_color ReadColor(const Napi::Value &v);
n8v_padding ReadPadding(const Napi::Value &v);
n8v_corner_radius ReadCornerRadius(const Napi::Value &v);
n8v_rounding ReadRounding(const Napi::Value &v);

n8v_flex_options ReadFlexOptions(const Napi::Object &o);
n8v_panel_options ReadPanelOptions(const Napi::Object &o);
n8v_text_options ReadTextOptions(const Napi::Object &o, std::string &urlStorage);

} // namespace n8v_napi
