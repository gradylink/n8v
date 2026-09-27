#include <n8v/n8v_c.h>
#include <n8v/n8v_ffi.h>
#include <napi.h>

#include <memory>
#include <string>
#include <vector>

#include "marshal.hpp"

using namespace n8v_napi;

namespace {

class BoolRef : public Napi::ObjectWrap<BoolRef> {
public:
  static Napi::Function GetClass(Napi::Env env) { return DefineClass(env, "BoolRef", {InstanceAccessor<&BoolRef::GetValue, &BoolRef::SetValue>("value")}); }

  explicit BoolRef(const Napi::CallbackInfo &info) : Napi::ObjectWrap<BoolRef>(info) {
    value_ = info.Length() > 0 && info[0].IsBoolean() ? info[0].As<Napi::Boolean>().Value() : false;
  }

  bool *Pointer() { return &value_; }

private:
  Napi::Value GetValue(const Napi::CallbackInfo &info) { return Napi::Boolean::New(info.Env(), value_); }
  void SetValue(const Napi::CallbackInfo &info, const Napi::Value &v) { value_ = v.ToBoolean().Value(); }

  bool value_ = false;
};

class IntRef : public Napi::ObjectWrap<IntRef> {
public:
  static Napi::Function GetClass(Napi::Env env) { return DefineClass(env, "IntRef", {InstanceAccessor<&IntRef::GetValue, &IntRef::SetValue>("value")}); }

  explicit IntRef(const Napi::CallbackInfo &info) : Napi::ObjectWrap<IntRef>(info) {
    value_ = info.Length() > 0 && info[0].IsNumber() ? info[0].As<Napi::Number>().Int32Value() : 0;
  }

  int *Pointer() { return &value_; }

private:
  Napi::Value GetValue(const Napi::CallbackInfo &info) { return Napi::Number::New(info.Env(), value_); }
  void SetValue(const Napi::CallbackInfo &info, const Napi::Value &v) { value_ = v.ToNumber().Int32Value(); }

  int value_ = 0;
};

class FloatRef : public Napi::ObjectWrap<FloatRef> {
public:
  static Napi::Function GetClass(Napi::Env env) { return DefineClass(env, "FloatRef", {InstanceAccessor<&FloatRef::GetValue, &FloatRef::SetValue>("value")}); }

  explicit FloatRef(const Napi::CallbackInfo &info) : Napi::ObjectWrap<FloatRef>(info) {
    value_ = info.Length() > 0 && info[0].IsNumber() ? info[0].As<Napi::Number>().FloatValue() : 0.0f;
  }

  float *Pointer() { return &value_; }

private:
  Napi::Value GetValue(const Napi::CallbackInfo &info) { return Napi::Number::New(info.Env(), value_); }
  void SetValue(const Napi::CallbackInfo &info, const Napi::Value &v) { value_ = v.ToNumber().FloatValue(); }

  float value_ = 0.0f;
};

class StringRef : public Napi::ObjectWrap<StringRef> {
public:
  static Napi::Function GetClass(Napi::Env env) { return DefineClass(env, "StringRef", {InstanceAccessor<&StringRef::GetValue, &StringRef::SetValue>("value")}); }

  explicit StringRef(const Napi::CallbackInfo &info) : Napi::ObjectWrap<StringRef>(info) {
    buf_ = {}; // data=nullptr, length=0, capacity=0 - n8v_ffi_string_buf_set/n8v itself lazily mallocs on first write.
    if (info.Length() > 0 && info[0].IsString()) {
      std::string s = info[0].As<Napi::String>().Utf8Value();
      n8v_ffi_string_buf_set(&buf_, s.data(), s.size());
    }
  }

  ~StringRef() override { n8v_string_buf_free(&buf_); }

  n8v_string_buf *Pointer() { return &buf_; }

private:
  Napi::Value GetValue(const Napi::CallbackInfo &info) { return Napi::String::New(info.Env(), buf_.data ? std::string(buf_.data, buf_.length) : std::string()); }
  void SetValue(const Napi::CallbackInfo &info, const Napi::Value &v) {
    std::string s = v.ToString().Utf8Value();
    n8v_ffi_string_buf_set(&buf_, s.data(), s.size());
  }

  n8v_string_buf buf_{};
};

template <typename Ref> Ref *UnwrapRef(const Napi::Value &v, const char *what) {
  if (!v.IsObject()) throw Napi::TypeError::New(v.Env(), std::string("n8v: expected a ") + what);
  return Ref::Unwrap(v.As<Napi::Object>());
}

std::vector<std::unique_ptr<Napi::FunctionReference>> g_frameCallbacks;

void *RegisterCallback(Napi::Env env, Napi::Function fn) {
  auto ref = std::make_unique<Napi::FunctionReference>(Napi::Persistent(fn));
  void *userdata = ref.get();
  g_frameCallbacks.push_back(std::move(ref));
  return userdata;
}

void ClearFrameCallbacks() { g_frameCallbacks.clear(); }

void ClickTrampoline(void *userdata) { static_cast<Napi::FunctionReference *>(userdata)->Call({}); }

void BoolChangeTrampoline(bool value, void *userdata) {
  auto *ref = static_cast<Napi::FunctionReference *>(userdata);
  ref->Call({Napi::Boolean::New(ref->Env(), value)});
}

void IntChangeTrampoline(int value, void *userdata) {
  auto *ref = static_cast<Napi::FunctionReference *>(userdata);
  ref->Call({Napi::Number::New(ref->Env(), value)});
}

void FloatChangeTrampoline(float value, void *userdata) {
  auto *ref = static_cast<Napi::FunctionReference *>(userdata);
  ref->Call({Napi::Number::New(ref->Env(), value)});
}

void TextChangeTrampoline(const char *text, size_t length, void *userdata) {
  auto *ref = static_cast<Napi::FunctionReference *>(userdata);
  ref->Call({Napi::String::New(ref->Env(), std::string(text, length))});
}

template <typename Fn> void WireCallback(const Napi::Object &o, const char *key, Fn trampoline, Fn &outFn, void *&outUserdata) {
  Napi::Value v = o.Get(key);
  if (!v.IsFunction()) return;
  outFn = trampoline;
  outUserdata = RegisterCallback(o.Env(), v.As<Napi::Function>());
}

void StrOr(const Napi::Object &o, const char *key, const char *&outPtr, std::string &storage) {
  Napi::Value v = o.Get(key);
  if (v.IsString()) {
    storage = v.As<Napi::String>().Utf8Value();
    outPtr = storage.c_str();
  } else {
    outPtr = nullptr;
  }
}

Napi::Value Initialize(const Napi::CallbackInfo &info) {
  int width = info[0].As<Napi::Number>().Int32Value();
  int height = info[1].As<Napi::Number>().Int32Value();
  std::string title = info[2].As<Napi::String>().Utf8Value();
  return Napi::Boolean::New(info.Env(), n8v_initialize(width, height, title.c_str()));
}

Napi::Value PumpEvents(const Napi::CallbackInfo &info) { return Napi::Boolean::New(info.Env(), n8v_pump_events()); }

Napi::Value Shutdown(const Napi::CallbackInfo &info) {
  n8v_shutdown();
  return info.Env().Undefined();
}

Napi::Value SetStyleFamily(const Napi::CallbackInfo &info) {
  n8v_set_style_family((n8v_style_family)info[0].As<Napi::Number>().Int32Value());
  return info.Env().Undefined();
}

Napi::Value ActiveStyleFamily(const Napi::CallbackInfo &info) { return Napi::Number::New(info.Env(), (int)n8v_active_style_family()); }

Napi::Value BeginFrame(const Napi::CallbackInfo &info) {
  ClearFrameCallbacks();
  n8v_begin_frame();
  return info.Env().Undefined();
}

Napi::Value EndFrame(const Napi::CallbackInfo &info) {
  n8v_end_frame();
  return info.Env().Undefined();
}

Napi::Value OpenFlex(const Napi::CallbackInfo &info) {
  n8v_open_flex(ReadFlexOptions(info[0].As<Napi::Object>()));
  return info.Env().Undefined();
}
Napi::Value CloseFlex(const Napi::CallbackInfo &info) {
  n8v_close_flex();
  return info.Env().Undefined();
}

Napi::Value OpenPanel(const Napi::CallbackInfo &info) {
  n8v_open_panel(ReadPanelOptions(info[0].As<Napi::Object>()));
  return info.Env().Undefined();
}
Napi::Value ClosePanel(const Napi::CallbackInfo &info) {
  n8v_close_panel();
  return info.Env().Undefined();
}

Napi::Value OpenSidebar(const Napi::CallbackInfo &info) {
  Napi::Object o = info[0].As<Napi::Object>();
  n8v_sidebar_options s{};
  std::string titleStorage;
  StrOr(o, "title", s.title, titleStorage);
  s.selected = UnwrapRef<IntRef>(o.Get("selected"), "IntRef")->Pointer();
  WireCallback(o, "onChange", IntChangeTrampoline, s.on_change, s.on_change_userdata);
  s.width = ReadSizing(o.Get("width"));
  s.min_width = NumberOr(o, "minWidth", 0.0f);
  s.max_width = NumberOr(o, "maxWidth", 0.0f);
  s.compact = BoolOr(o, "compact", false);
  n8v_open_sidebar(s);
  return info.Env().Undefined();
}
Napi::Value CloseSidebar(const Napi::CallbackInfo &info) {
  n8v_close_sidebar();
  return info.Env().Undefined();
}

Napi::Value OpenPage(const Napi::CallbackInfo &info) {
  Napi::Object o = info[0].As<Napi::Object>();
  n8v_page_options p{};
  std::string nameStorage;
  StrOr(o, "name", p.name, nameStorage);
  std::string iconStorage;
  StrOr(o, "icon", p.icon, iconStorage);
  std::string imageStorage;
  StrOr(o, "image", p.image, imageStorage);
  return Napi::Boolean::New(info.Env(), n8v_open_page(p));
}
Napi::Value ClosePage(const Napi::CallbackInfo &info) {
  n8v_close_page();
  return info.Env().Undefined();
}

Napi::Value Text(const Napi::CallbackInfo &info) {
  Napi::Object o = info[0].As<Napi::Object>();
  std::string label = info[1].As<Napi::String>().Utf8Value();
  std::string urlStorage;
  n8v_text_options t = ReadTextOptions(o, urlStorage);
  _n8v_set_text_opts(t);
  _n8v_text_commit(label.c_str());
  return info.Env().Undefined();
}

Napi::Value Button(const Napi::CallbackInfo &info) {
  Napi::Object o = info[0].As<Napi::Object>();
  std::string label = info[1].As<Napi::String>().Utf8Value();
  n8v_button_options b{};
  b.style = (n8v_button_style)EnumOr(o, "style", N8V_BUTTON_STYLE_PRIMARY);
  std::string iconStorage;
  StrOr(o, "icon", b.icon, iconStorage);
  b.icon_variant = (n8v_icon_variant)EnumOr(o, "iconVariant", N8V_ICON_VARIANT_OUTLINE);
  b.icon_position = (n8v_icon_position)EnumOr(o, "iconPosition", N8V_ICON_POSITION_LEADING);
  WireCallback(o, "onClick", ClickTrampoline, b.on_click, b.on_click_userdata);
  _n8v_set_button_opts(b);
  _n8v_button_commit(label.c_str());
  return info.Env().Undefined();
}

Napi::Value Checkbox(const Napi::CallbackInfo &info) {
  Napi::Object o = info[0].As<Napi::Object>();
  std::string label = info[1].As<Napi::String>().Utf8Value();
  n8v_checkbox_options c{};
  c.checked = UnwrapRef<BoolRef>(o.Get("checked"), "BoolRef")->Pointer();
  WireCallback(o, "onChange", BoolChangeTrampoline, c.on_change, c.on_change_userdata);
  _n8v_set_checkbox_opts(c);
  _n8v_checkbox_commit(label.c_str());
  return info.Env().Undefined();
}

Napi::Value Toggle(const Napi::CallbackInfo &info) {
  Napi::Object o = info[0].As<Napi::Object>();
  std::string label = info[1].As<Napi::String>().Utf8Value();
  n8v_toggle_options t{};
  t.checked = UnwrapRef<BoolRef>(o.Get("checked"), "BoolRef")->Pointer();
  WireCallback(o, "onChange", BoolChangeTrampoline, t.on_change, t.on_change_userdata);
  _n8v_set_toggle_opts(t);
  _n8v_toggle_commit(label.c_str());
  return info.Env().Undefined();
}

Napi::Value Radio(const Napi::CallbackInfo &info) {
  Napi::Object o = info[0].As<Napi::Object>();
  std::string label = info[1].As<Napi::String>().Utf8Value();
  n8v_radio_options r{};
  r.selected = UnwrapRef<IntRef>(o.Get("selected"), "IntRef")->Pointer();
  r.value = (int)EnumOr(o, "value", 0);
  WireCallback(o, "onChange", IntChangeTrampoline, r.on_change, r.on_change_userdata);
  _n8v_set_radio_opts(r);
  _n8v_radio_commit(label.c_str());
  return info.Env().Undefined();
}

Napi::Value Entry(const Napi::CallbackInfo &info) {
  Napi::Object o = info[0].As<Napi::Object>();
  n8v_entry_options e{};
  e.value = UnwrapRef<StringRef>(o.Get("value"), "StringRef")->Pointer();
  std::string placeholderStorage;
  StrOr(o, "placeholder", e.placeholder, placeholderStorage);
  e.password = BoolOr(o, "password", false);
  WireCallback(o, "onChange", TextChangeTrampoline, e.on_change, e.on_change_userdata);
  WireCallback(o, "onSubmit", ClickTrampoline, e.on_submit, e.on_submit_userdata);
  n8v_entry(e);
  return info.Env().Undefined();
}

Napi::Value Dropdown(const Napi::CallbackInfo &info) {
  Napi::Object o = info[0].As<Napi::Object>();
  Napi::Array itemsArr = o.Get("items").As<Napi::Array>();
  uint32_t n = itemsArr.Length();
  std::vector<std::string> itemStrings;
  std::vector<const char *> itemPtrs;
  itemStrings.reserve(n);
  itemPtrs.reserve(n);
  for (uint32_t i = 0; i < n; i++) itemStrings.push_back(itemsArr.Get(i).As<Napi::String>().Utf8Value());
  for (auto &s : itemStrings) itemPtrs.push_back(s.c_str());

  n8v_dropdown_options d{};
  d.items = itemPtrs.data();
  d.item_count = itemPtrs.size();
  d.selected = UnwrapRef<IntRef>(o.Get("selected"), "IntRef")->Pointer();
  std::string placeholderStorage;
  StrOr(o, "placeholder", d.placeholder, placeholderStorage);
  WireCallback(o, "onChange", IntChangeTrampoline, d.on_change, d.on_change_userdata);
  n8v_dropdown(d);
  return info.Env().Undefined();
}

Napi::Value Slider(const Napi::CallbackInfo &info) {
  Napi::Object o = info[0].As<Napi::Object>();
  n8v_slider_options s{};
  s.value = UnwrapRef<FloatRef>(o.Get("value"), "FloatRef")->Pointer();
  s.min = NumberOr(o, "min", 0.0f);
  s.max = NumberOr(o, "max", 1.0f);
  WireCallback(o, "onChange", FloatChangeTrampoline, s.on_change, s.on_change_userdata);
  n8v_slider(s);
  return info.Env().Undefined();
}

Napi::Value Image(const Napi::CallbackInfo &info) {
  Napi::Object o = info[0].As<Napi::Object>();
  n8v_image_options img{};
  img.source_kind = (n8v_image_source_kind)EnumOr(o, "sourceKind", N8V_IMAGE_SOURCE_PATH);
  std::string pathStorage;
  StrOr(o, "path", img.path, pathStorage);

  Napi::Value encoded = o.Get("encodedData");
  if (encoded.IsTypedArray()) {
    Napi::Uint8Array arr = encoded.As<Napi::TypedArray>().As<Napi::Uint8Array>();
    img.encoded_data = arr.Data();
    img.encoded_size = arr.ByteLength();
  }
  Napi::Value pixels = o.Get("pixels");
  if (pixels.IsTypedArray()) {
    img.pixels = pixels.As<Napi::TypedArray>().As<Napi::Uint8Array>().Data();
  }
  img.pixel_width = (int)EnumOr(o, "pixelWidth", 0);
  img.pixel_height = (int)EnumOr(o, "pixelHeight", 0);
  img.width = ReadSizing(o.Get("width"));
  img.height = ReadSizing(o.Get("height"));
  img.rounding = ReadRounding(o.Get("rounding"));
  n8v_image(img);
  return info.Env().Undefined();
}

Napi::Value Icon(const Napi::CallbackInfo &info) {
  Napi::Object o = info[0].As<Napi::Object>();
  n8v_icon_options ic{};
  std::string nameStorage;
  StrOr(o, "name", ic.name, nameStorage);
  ic.variant = (n8v_icon_variant)EnumOr(o, "variant", N8V_ICON_VARIANT_OUTLINE);
  ic.width = ReadSizing(o.Get("width"));
  ic.height = ReadSizing(o.Get("height"));
  ic.tint = ReadColor(o.Get("tint"));
  n8v_icon(ic);
  return info.Env().Undefined();
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  exports.Set("BoolRef", BoolRef::GetClass(env));
  exports.Set("IntRef", IntRef::GetClass(env));
  exports.Set("FloatRef", FloatRef::GetClass(env));
  exports.Set("StringRef", StringRef::GetClass(env));

  exports.Set("initialize", Napi::Function::New(env, Initialize));
  exports.Set("pumpEvents", Napi::Function::New(env, PumpEvents));
  exports.Set("shutdown", Napi::Function::New(env, Shutdown));
  exports.Set("setStyleFamily", Napi::Function::New(env, SetStyleFamily));
  exports.Set("activeStyleFamily", Napi::Function::New(env, ActiveStyleFamily));
  exports.Set("beginFrame", Napi::Function::New(env, BeginFrame));
  exports.Set("endFrame", Napi::Function::New(env, EndFrame));

  exports.Set("openFlex", Napi::Function::New(env, OpenFlex));
  exports.Set("closeFlex", Napi::Function::New(env, CloseFlex));
  exports.Set("openPanel", Napi::Function::New(env, OpenPanel));
  exports.Set("closePanel", Napi::Function::New(env, ClosePanel));
  exports.Set("openSidebar", Napi::Function::New(env, OpenSidebar));
  exports.Set("closeSidebar", Napi::Function::New(env, CloseSidebar));
  exports.Set("openPage", Napi::Function::New(env, OpenPage));
  exports.Set("closePage", Napi::Function::New(env, ClosePage));

  exports.Set("text", Napi::Function::New(env, Text));
  exports.Set("button", Napi::Function::New(env, Button));
  exports.Set("checkbox", Napi::Function::New(env, Checkbox));
  exports.Set("toggle", Napi::Function::New(env, Toggle));
  exports.Set("radio", Napi::Function::New(env, Radio));
  exports.Set("entry", Napi::Function::New(env, Entry));
  exports.Set("dropdown", Napi::Function::New(env, Dropdown));
  exports.Set("slider", Napi::Function::New(env, Slider));
  exports.Set("image", Napi::Function::New(env, Image));
  exports.Set("icon", Napi::Function::New(env, Icon));

  return exports;
}

} // namespace

NODE_API_MODULE(n8v, Init)
