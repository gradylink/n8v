#include <n8v/n8v_c.h>
#include <n8v/n8v_ffi.h>
#include <napi.h>

#include <deque>
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
    buf_ = {};
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

void PointTrampoline(float x, float y, void *userdata) {
  auto *ref = static_cast<Napi::FunctionReference *>(userdata);
  ref->Call({Napi::Number::New(ref->Env(), x), Napi::Number::New(ref->Env(), y)});
}

void ZoomTrampoline(float factor, float x, float y, void *userdata) {
  auto *ref = static_cast<Napi::FunctionReference *>(userdata);
  Napi::Env env = ref->Env();
  ref->Call({Napi::Number::New(env, factor), Napi::Number::New(env, x), Napi::Number::New(env, y)});
}

bool KeyTrampoline(n8v_key key, unsigned mods, void *userdata) {
  auto *ref = static_cast<Napi::FunctionReference *>(userdata);
  Napi::Env env = ref->Env();
  return ref->Call({Napi::Number::New(env, (int)key), Napi::Number::New(env, mods)}).ToBoolean().Value();
}

Napi::Array PathsToArray(Napi::Env env, const char *const *paths, size_t count) {
  Napi::Array out = Napi::Array::New(env, count);
  for (size_t i = 0; i < count; i++) out.Set((uint32_t)i, Napi::String::New(env, paths[i]));
  return out;
}

void PasteFilesTrampoline(const char *const *paths, size_t count, void *userdata) {
  auto *ref = static_cast<Napi::FunctionReference *>(userdata);
  ref->Call({PathsToArray(ref->Env(), paths, count)});
}

bool LinkStyleTrampoline(const char *url, n8v_link_style *out, void *userdata) {
  auto *ref = static_cast<Napi::FunctionReference *>(userdata);
  Napi::Value v = ref->Call({Napi::String::New(ref->Env(), url ? url : "")});
  if (!v.IsObject()) return false;
  Napi::Object o = v.As<Napi::Object>();
  out->foreground = ReadColor(o.Get("foreground"));
  out->background = ReadColor(o.Get("background"));
  out->bold = BoolOr(o, "bold", false);
  out->clickable = BoolOr(o, "clickable", false);
  return true;
}

const char *ImageResolveTrampoline(const char *src, void *userdata) {
  static std::string buffer;
  auto *ref = static_cast<Napi::FunctionReference *>(userdata);
  Napi::Value v = ref->Call({Napi::String::New(ref->Env(), src ? src : "")});
  if (!v.IsString()) return nullptr;
  buffer = v.As<Napi::String>().Utf8Value();
  return buffer.empty() ? nullptr : buffer.c_str();
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
  Napi::Object o = info[0].As<Napi::Object>();
  n8v_flex_options f = ReadFlexOptions(o);
  std::string idStorage;
  StrOr(o, "id", f.id, idStorage);
  f.stick_to_bottom = BoolOr(o, "stickToBottom", false);
  WireCallback(o, "onHover", BoolChangeTrampoline, f.on_hover, f.on_hover_userdata);
  n8v_open_flex(f);
  return info.Env().Undefined();
}
Napi::Value CloseFlex(const Napi::CallbackInfo &info) {
  n8v_close_flex();
  return info.Env().Undefined();
}

Napi::Value OpenPanel(const Napi::CallbackInfo &info) {
  Napi::Object o = info[0].As<Napi::Object>();
  n8v_panel_options p = ReadPanelOptions(o);
  p.accent = ReadColor(o.Get("accent"));
  if (o.Get("padding").IsObject()) {
    p.has_padding = true;
    p.padding = ReadPadding(o.Get("padding"));
  }
  std::string idStorage;
  StrOr(o, "id", p.id, idStorage);
  n8v_open_panel(p);
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
  if (o.Get("collapsed").IsObject()) {
    s.collapsed = UnwrapRef<BoolRef>(o.Get("collapsed"), "BoolRef")->Pointer();
    WireCallback(o, "onCollapse", BoolChangeTrampoline, s.on_collapse, s.on_collapse_userdata);
  }
  n8v_open_sidebar(s);
  return info.Env().Undefined();
}
Napi::Value CloseSidebar(const Napi::CallbackInfo &info) {
  n8v_close_sidebar();
  return info.Env().Undefined();
}

Napi::Value SidebarSection(const Napi::CallbackInfo &info) {
  std::string title = info[0].As<Napi::String>().Utf8Value();
  n8v_sidebar_section_options s{};
  s.title = title.c_str();
  n8v_sidebar_section(s);
  return info.Env().Undefined();
}

Napi::Value SidebarDivider(const Napi::CallbackInfo &info) {
  n8v_sidebar_divider();
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
  p.image_rounding = ReadRounding(o.Get("imageRounding"));
  p.notify = BoolOr(o, "notify", false);
  std::string badgeStorage;
  StrOr(o, "badge", p.badge, badgeStorage);
  p.badge_color = (n8v_badge_color)EnumOr(o, "badgeColor", N8V_BADGE_COLOR_DEFAULT);
  p.image_size = NumberOr(o, "imageSize", 0.0f);
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
  WireCallback(o, "onClick", ClickTrampoline, t.on_click, t.on_click_userdata);
  _n8v_set_text_opts(t);
  _n8v_text_commit(label.c_str());
  return info.Env().Undefined();
}

Napi::Value Button(const Napi::CallbackInfo &info) {
  Napi::Object o = info[0].As<Napi::Object>();
  std::string label = info[1].As<Napi::String>().Utf8Value();
  n8v_button_options b{};
  b.style = (n8v_button_style)EnumOr(o, "style", N8V_BUTTON_STYLE_SECONDARY);
  std::string iconStorage;
  StrOr(o, "icon", b.icon, iconStorage);
  b.icon_variant = (n8v_icon_variant)EnumOr(o, "iconVariant", N8V_ICON_VARIANT_OUTLINE);
  b.icon_position = (n8v_icon_position)EnumOr(o, "iconPosition", N8V_ICON_POSITION_LEADING);
  WireCallback(o, "onClick", ClickTrampoline, b.on_click, b.on_click_userdata);
  if (o.Get("checked").IsObject()) {
    b.checked = UnwrapRef<BoolRef>(o.Get("checked"), "BoolRef")->Pointer();
    WireCallback(o, "onChange", BoolChangeTrampoline, b.on_change, b.on_change_userdata);
  }
  std::string idStorage;
  StrOr(o, "id", b.id, idStorage);
  std::string buttonImageStorage;
  StrOr(o, "image", b.image, buttonImageStorage);
  b.image_height = (int)EnumOr(o, "imageHeight", 0);
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
  std::string idStorage;
  StrOr(o, "id", c.id, idStorage);
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
  std::string idStorage;
  StrOr(o, "id", t.id, idStorage);
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
  std::string idStorage;
  StrOr(o, "id", r.id, idStorage);
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
  e.multiline = BoolOr(o, "multiline", false);
  e.submit_mode = (n8v_submit_mode)EnumOr(o, "submitMode", N8V_SUBMIT_ON_ENTER);
  e.min_lines = U16Or(o, "minLines", 0);
  e.max_lines = U16Or(o, "maxLines", 0);
  e.match_entry_style = BoolOr(o, "matchEntryStyle", false);
  WireCallback(o, "onChange", TextChangeTrampoline, e.on_change, e.on_change_userdata);
  WireCallback(o, "onSubmit", ClickTrampoline, e.on_submit, e.on_submit_userdata);
  WireCallback(o, "onKey", KeyTrampoline, e.on_key, e.on_key_userdata);
  WireCallback(o, "onPasteFiles", PasteFilesTrampoline, e.on_paste_files, e.on_paste_files_userdata);
  std::string idStorage;
  StrOr(o, "id", e.id, idStorage);
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
  std::string idStorage;
  StrOr(o, "id", d.id, idStorage);
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
  std::string idStorage;
  StrOr(o, "id", s.id, idStorage);
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
  WireCallback(o, "onClick", ClickTrampoline, img.on_click, img.on_click_userdata);
  WireCallback(o, "onClickAt", PointTrampoline, img.on_click_at, img.on_click_at_userdata);
  WireCallback(o, "onZoom", ZoomTrampoline, img.on_zoom, img.on_zoom_userdata);
  WireCallback(o, "onDrag", PointTrampoline, img.on_drag, img.on_drag_userdata);
  n8v_image(img);
  return info.Env().Undefined();
}

Napi::Value Video(const Napi::CallbackInfo &info) {
  Napi::Object o = info[0].As<Napi::Object>();
  n8v_video_options v{};
  std::string sourceStorage;
  StrOr(o, "source", v.source, sourceStorage);
  v.width = ReadSizing(o.Get("width"));
  v.height = ReadSizing(o.Get("height"));
  v.rounding = ReadRounding(o.Get("rounding"));
  v.autoplay = BoolOr(o, "autoplay", false);
  v.loop = BoolOr(o, "loop", false);
  v.muted = BoolOr(o, "muted", false);
  v.controls = BoolOr(o, "controls", false);
  std::string idStorage;
  StrOr(o, "id", v.id, idStorage);
  WireCallback(o, "onFullscreen", BoolChangeTrampoline, v.on_fullscreen, v.on_fullscreen_userdata);
  v.is_fullscreen = BoolOr(o, "isFullscreen", false);
  n8v_video(v);
  return info.Env().Undefined();
}

Napi::Value Audio(const Napi::CallbackInfo &info) {
  Napi::Object o = info[0].As<Napi::Object>();
  n8v_audio_options a{};
  std::string sourceStorage;
  StrOr(o, "source", a.source, sourceStorage);
  a.width = ReadSizing(o.Get("width"));
  a.autoplay = BoolOr(o, "autoplay", false);
  a.loop = BoolOr(o, "loop", false);
  a.muted = BoolOr(o, "muted", false);
  std::string idStorage;
  StrOr(o, "id", a.id, idStorage);
  n8v_audio(a);
  return info.Env().Undefined();
}

Napi::Value Divider(const Napi::CallbackInfo &info) {
  Napi::Object o = info[0].As<Napi::Object>();
  n8v_divider_options d{};
  d.color = ReadColor(o.Get("color"));
  std::string idStorage;
  StrOr(o, "id", d.id, idStorage);
  n8v_divider(d);
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

Napi::Value Document(const Napi::CallbackInfo &info) {
  Napi::Object o = info[0].As<Napi::Object>();
  n8v_document_options d{};
  std::string markdownStorage;
  StrOr(o, "markdown", d.markdown, markdownStorage);
  d.width = ReadSizing(o.Get("width"));
  d.height = ReadSizing(o.Get("height"));
  std::string idStorage;
  StrOr(o, "id", d.id, idStorage);
  std::string imageStateStorage;
  StrOr(o, "imageState", d.image_state, imageStateStorage);
  WireCallback(o, "linkStyle", LinkStyleTrampoline, d.link_style, d.link_style_userdata);
  WireCallback(o, "imageResolver", ImageResolveTrampoline, d.image_resolver, d.image_resolver_userdata);
  n8v_document(d);
  return info.Env().Undefined();
}

Napi::Value OpenFloating(const Napi::CallbackInfo &info) {
  Napi::Object o = info[0].As<Napi::Object>();
  n8v_floating_options f{};
  f.width = ReadSizing(o.Get("width"));
  f.height = ReadSizing(o.Get("height"));
  f.element_attach_point = (n8v_attach_point)EnumOr(o, "elementAttachPoint", N8V_ATTACH_POINT_RIGHT_TOP);
  f.parent_attach_point = (n8v_attach_point)EnumOr(o, "parentAttachPoint", N8V_ATTACH_POINT_RIGHT_TOP);
  f.offset_x = NumberOr(o, "offsetX", 0.0f);
  f.offset_y = NumberOr(o, "offsetY", 0.0f);
  f.opaque = BoolOr(o, "opaque", false);
  f.keep_on_screen = BoolOr(o, "keepOnScreen", false);
  f.screen_margin = NumberOr(o, "screenMargin", 0.0f);
  WireCallback(o, "onDismiss", ClickTrampoline, f.on_dismiss, f.on_dismiss_userdata);
  std::string idStorage;
  StrOr(o, "id", f.id, idStorage);
  n8v_open_floating(f);
  return info.Env().Undefined();
}
Napi::Value CloseFloating(const Napi::CallbackInfo &info) {
  n8v_close_floating();
  return info.Env().Undefined();
}

Napi::Value OpenPopover(const Napi::CallbackInfo &info) {
  Napi::Object o = info[0].As<Napi::Object>();
  n8v_popover_options p{};
  p.side = (n8v_popover_side)EnumOr(o, "side", N8V_POPOVER_SIDE_BELOW);
  p.align_end = BoolOr(o, "alignEnd", false);
  p.width = ReadSizing(o.Get("width"));
  p.height = ReadSizing(o.Get("height"));
  p.gap = NumberOr(o, "gap", 0.0f);
  p.screen_margin = NumberOr(o, "screenMargin", 0.0f);
  WireCallback(o, "onDismiss", ClickTrampoline, p.on_dismiss, p.on_dismiss_userdata);
  std::string idStorage;
  StrOr(o, "id", p.id, idStorage);
  n8v_open_popover(p);
  return info.Env().Undefined();
}
Napi::Value ClosePopover(const Napi::CallbackInfo &info) {
  n8v_close_popover();
  return info.Env().Undefined();
}

Napi::Value IsHovered(const Napi::CallbackInfo &info) {
  std::string id = info[0].As<Napi::String>().Utf8Value();
  return Napi::Boolean::New(info.Env(), n8v_is_hovered(id.c_str()));
}

template <void (*Fn)(const char *)> Napi::Value ScrollSimple(const Napi::CallbackInfo &info) {
  std::string id = info[0].As<Napi::String>().Utf8Value();
  Fn(id.c_str());
  return info.Env().Undefined();
}

Napi::Value ScrollToOffset(const Napi::CallbackInfo &info) {
  std::string id = info[0].As<Napi::String>().Utf8Value();
  n8v_scroll_to_offset(id.c_str(), info[1].As<Napi::Number>().FloatValue(), info[2].As<Napi::Number>().FloatValue());
  return info.Env().Undefined();
}

Napi::Value ScrollToElement(const Napi::CallbackInfo &info) {
  std::string container = info[0].As<Napi::String>().Utf8Value();
  std::string element = info[1].As<Napi::String>().Utf8Value();
  n8v_scroll_to_element(container.c_str(), element.c_str(), info[2].As<Napi::Number>().FloatValue(), info[3].As<Napi::Number>().FloatValue());
  return info.Env().Undefined();
}

Napi::Value Focus(const Napi::CallbackInfo &info) {
  std::string id = info[0].As<Napi::String>().Utf8Value();
  n8v_focus(id.c_str());
  return info.Env().Undefined();
}

Napi::Value ScrollState(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  std::string id = info[0].As<Napi::String>().Utf8Value();
  float x = 0, y = 0, w = 0, h = 0;
  if (!n8v_scroll_state(id.c_str(), &x, &y, &w, &h)) return env.Null();
  Napi::Object out = Napi::Object::New(env);
  out.Set("offsetX", x);
  out.Set("offsetY", y);
  out.Set("viewWidth", w);
  out.Set("viewHeight", h);
  return out;
}

Napi::Value OpenUrl(const Napi::CallbackInfo &info) {
  std::string url = info[0].As<Napi::String>().Utf8Value();
  n8v_open_url(url.c_str());
  return info.Env().Undefined();
}

std::unique_ptr<Napi::FunctionReference> g_urlHandler;

bool UrlHandlerTrampoline(const char *url, void *userdata) {
  auto *ref = static_cast<Napi::FunctionReference *>(userdata);
  return ref->Call({Napi::String::New(ref->Env(), url ? url : "")}).ToBoolean().Value();
}

Napi::Value SetUrlHandler(const Napi::CallbackInfo &info) {
  if (!info[0].IsFunction()) {
    n8v_set_url_handler(nullptr, nullptr);
    g_urlHandler.reset();
    return info.Env().Undefined();
  }
  auto next = std::make_unique<Napi::FunctionReference>(Napi::Persistent(info[0].As<Napi::Function>()));
  n8v_set_url_handler(UrlHandlerTrampoline, next.get());
  g_urlHandler = std::move(next);
  return info.Env().Undefined();
}

struct VisibleChildren {
  Napi::Env env;
  Napi::Array out;
};

void VisibleChildTrampoline(const char *id, float top, float height, void *userdata) {
  auto *v = static_cast<VisibleChildren *>(userdata);
  Napi::Object child = Napi::Object::New(v->env);
  child.Set("id", Napi::String::New(v->env, id));
  child.Set("top", top);
  child.Set("height", height);
  v->out.Set(v->out.Length(), child);
}

Napi::Value VisibleChildrenOf(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  std::string id = info[0].As<Napi::String>().Utf8Value();
  VisibleChildren visible{env, Napi::Array::New(env)};
  if (!n8v_visible_children(id.c_str(), info[1].As<Napi::Number>().FloatValue(), VisibleChildTrampoline, &visible)) return env.Null();
  return visible.out;
}

Napi::Value Wake(const Napi::CallbackInfo &info) {
  n8v_wake();
  return info.Env().Undefined();
}

Napi::Value RaiseWindow(const Napi::CallbackInfo &info) {
  n8v_raise_window();
  return info.Env().Undefined();
}

Napi::Value IsDarkTheme(const Napi::CallbackInfo &info) { return Napi::Boolean::New(info.Env(), n8v_is_dark_theme()); }

Napi::Value WindowSize(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  float w = 0, h = 0;
  if (!n8v_window_size(&w, &h)) return env.Null();
  Napi::Object out = Napi::Object::New(env);
  out.Set("width", w);
  out.Set("height", h);
  return out;
}

void FileDialogTrampoline(const char *const *paths, size_t count, void *userdata) {
  std::unique_ptr<Napi::FunctionReference> ref(static_cast<Napi::FunctionReference *>(userdata));
  Napi::Env env = ref->Env();
  Napi::HandleScope scope(env);
  ref->Call({PathsToArray(env, paths, count)});
}

Napi::Value ShowFileDialog(const Napi::CallbackInfo &info) {
  Napi::Object o = info[0].As<Napi::Object>();
  n8v_file_dialog_options d{};
  d.kind = (n8v_file_dialog_kind)EnumOr(o, "kind", N8V_FILE_DIALOG_OPEN_FILE);
  std::string titleStorage, dirStorage, nameStorage;
  StrOr(o, "title", d.title, titleStorage);
  StrOr(o, "initialDir", d.initial_dir, dirStorage);
  StrOr(o, "defaultName", d.default_name, nameStorage);
  d.multiple = BoolOr(o, "multiple", false);

  std::deque<std::string> strings;
  std::deque<std::vector<const char *>> patternLists;
  std::vector<n8v_file_filter> filters;
  Napi::Value filtersValue = o.Get("filters");
  if (filtersValue.IsArray()) {
    Napi::Array arr = filtersValue.As<Napi::Array>();
    uint32_t n = arr.Length();
    filters.reserve(n);
    for (uint32_t i = 0; i < n; i++) {
      Napi::Object f = arr.Get(i).As<Napi::Object>();
      strings.push_back(f.Get("name").IsString() ? f.Get("name").As<Napi::String>().Utf8Value() : std::string());
      const char *name = strings.back().c_str();
      patternLists.emplace_back();
      Napi::Value patterns = f.Get("patterns");
      if (patterns.IsArray()) {
        Napi::Array p = patterns.As<Napi::Array>();
        for (uint32_t j = 0; j < p.Length(); j++) {
          strings.push_back(p.Get(j).ToString().Utf8Value());
          patternLists.back().push_back(strings.back().c_str());
        }
      }
      filters.push_back({name, patternLists.back().data(), patternLists.back().size()});
    }
    d.filters = filters.data();
    d.filter_count = filters.size();
  }

  Napi::Value cb = info[1];
  if (cb.IsFunction()) {
    d.on_result = FileDialogTrampoline;
    d.on_result_userdata = new Napi::FunctionReference(Napi::Persistent(cb.As<Napi::Function>()));
  }
  n8v_show_file_dialog(d);
  return info.Env().Undefined();
}

void MessageDialogTrampoline(int button, void *userdata) {
  std::unique_ptr<Napi::FunctionReference> ref(static_cast<Napi::FunctionReference *>(userdata));
  Napi::Env env = ref->Env();
  Napi::HandleScope scope(env);
  ref->Call({Napi::Number::New(env, button)});
}

Napi::Value ShowMessageDialog(const Napi::CallbackInfo &info) {
  Napi::Object o = info[0].As<Napi::Object>();
  n8v_message_dialog_options d{};
  d.kind = (n8v_message_dialog_kind)EnumOr(o, "kind", N8V_MESSAGE_DIALOG_INFO);
  std::string titleStorage, messageStorage;
  StrOr(o, "title", d.title, titleStorage);
  StrOr(o, "message", d.message, messageStorage);

  std::deque<std::string> labels;
  std::vector<n8v_message_button> buttons;
  Napi::Value buttonsValue = o.Get("buttons");
  if (buttonsValue.IsArray()) {
    Napi::Array arr = buttonsValue.As<Napi::Array>();
    for (uint32_t i = 0; i < arr.Length(); i++) {
      Napi::Value b = arr.Get(i);
      if (b.IsString()) {
        labels.push_back(b.As<Napi::String>().Utf8Value());
        buttons.push_back({labels.back().c_str(), false});
      } else if (b.IsObject()) {
        Napi::Object bo = b.As<Napi::Object>();
        labels.push_back(bo.Get("label").IsString() ? bo.Get("label").As<Napi::String>().Utf8Value() : std::string());
        buttons.push_back({labels.back().c_str(), BoolOr(bo, "destructive", false)});
      }
    }
    d.buttons = buttons.data();
    d.button_count = buttons.size();
  }

  Napi::Value cb = info[1];
  if (cb.IsFunction()) {
    d.on_result = MessageDialogTrampoline;
    d.on_result_userdata = new Napi::FunctionReference(Napi::Persistent(cb.As<Napi::Function>()));
  }
  n8v_show_message_dialog(d);
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
  exports.Set("sidebarSection", Napi::Function::New(env, SidebarSection));
  exports.Set("sidebarDivider", Napi::Function::New(env, SidebarDivider));
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
  exports.Set("video", Napi::Function::New(env, Video));
  exports.Set("audio", Napi::Function::New(env, Audio));
  exports.Set("divider", Napi::Function::New(env, Divider));
  exports.Set("icon", Napi::Function::New(env, Icon));
  exports.Set("document", Napi::Function::New(env, Document));

  exports.Set("showFileDialog", Napi::Function::New(env, ShowFileDialog));
  exports.Set("showMessageDialog", Napi::Function::New(env, ShowMessageDialog));

  exports.Set("openFloating", Napi::Function::New(env, OpenFloating));
  exports.Set("closeFloating", Napi::Function::New(env, CloseFloating));
  exports.Set("openPopover", Napi::Function::New(env, OpenPopover));
  exports.Set("closePopover", Napi::Function::New(env, ClosePopover));

  exports.Set("focus", Napi::Function::New(env, Focus));
  exports.Set("scrollState", Napi::Function::New(env, ScrollState));
  exports.Set("openUrl", Napi::Function::New(env, OpenUrl));
  exports.Set("setUrlHandler", Napi::Function::New(env, SetUrlHandler));
  exports.Set("visibleChildren", Napi::Function::New(env, VisibleChildrenOf));
  exports.Set("wake", Napi::Function::New(env, Wake));
  exports.Set("raiseWindow", Napi::Function::New(env, RaiseWindow));
  exports.Set("isDarkTheme", Napi::Function::New(env, IsDarkTheme));
  exports.Set("windowSize", Napi::Function::New(env, WindowSize));

  exports.Set("isHovered", Napi::Function::New(env, IsHovered));
  exports.Set("scrollToBottom", Napi::Function::New(env, ScrollSimple<n8v_scroll_to_bottom>));
  exports.Set("scrollToTop", Napi::Function::New(env, ScrollSimple<n8v_scroll_to_top>));
  exports.Set("scrollToLeft", Napi::Function::New(env, ScrollSimple<n8v_scroll_to_left>));
  exports.Set("scrollToRight", Napi::Function::New(env, ScrollSimple<n8v_scroll_to_right>));
  exports.Set("scrollToOffset", Napi::Function::New(env, ScrollToOffset));
  exports.Set("scrollToElement", Napi::Function::New(env, ScrollToElement));

  InitPaint(env, exports);

  return exports;
}

} // namespace

NODE_API_MODULE(n8v, Init)
