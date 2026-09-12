#include "html_backend_impl.hpp"

#include <emscripten.h>
#include <emscripten/html5.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>

namespace n8v::detail {
namespace {

void applyStyleFromQueryString() {
  emscripten::val params = emscripten::val::global("URLSearchParams").new_(emscripten::val::global("location")["search"]);
  emscripten::val styleParam = params.call<emscripten::val>("get", std::string("style"));
  if (styleParam.isNull() || styleParam.isUndefined()) return;

  std::string value = styleParam.as<std::string>();
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return std::tolower(c); });

  if (value == "plain") n8v_set_style_family(N8V_STYLE_FAMILY_PLAIN);
  else if (value == "material") n8v_set_style_family(N8V_STYLE_FAMILY_MATERIAL);
  else if (value == "cupertino") n8v_set_style_family(N8V_STYLE_FAMILY_CUPERTINO);
  else if (value == "fluent") n8v_set_style_family(N8V_STYLE_FAMILY_FLUENT);
}

} // namespace

bool HtmlBackend::initialize(int width, int height, std::string_view title) {
  n8vHtmlSetInstance(this);
  doc_ = emscripten::val::global("document");
  emscripten::val win = emscripten::val::global("window");

  applyStyleFromQueryString();

  doc_.set("title", std::string(title));

  emscripten::val style = doc_.call<emscripten::val>("createElement", std::string("style"));
  style.set(
    "textContent",
    std::string(
      "html,body{margin:0;padding:0;width:100%;height:100%;overflow:hidden;background:#ffffff;}"
      "#n8v-root{position:relative;width:100%;height:100%;overflow:hidden;}"
      "#n8v-root div,#n8v-root a,#n8v-root svg{position:absolute;left:0;top:0;box-sizing:border-box;user-select:none;-webkit-user-select:none;}"
      "#n8v-root input{position:absolute;left:0;top:0;box-sizing:border-box;margin:0;border:none;outline:none;background:transparent;}"
      "#n8v-root .n8v-text{white-space:pre;pointer-events:auto;user-select:text;-webkit-user-select:text;}"
      "#n8v-root .n8v-text-unselectable{user-select:none;-webkit-user-select:none;}"
    )
  );
  doc_["head"].call<void>("appendChild", style);

  root_ = doc_.call<emscripten::val>("createElement", std::string("div"));
  root_.call<void>("setAttribute", std::string("id"), std::string("n8v-root"));
  doc_["body"].call<void>("appendChild", root_);

  emscripten::val canvas = doc_.call<emscripten::val>("createElement", std::string("canvas"));
  measureCtx_ = canvas.call<emscripten::val>("getContext", std::string("2d"));

  injectFontFaces();
  n8vHtmlInstallListeners();

  width_ = win["innerWidth"].as<int>();
  height_ = win["innerHeight"].as<int>();
  if (width_ <= 0) width_ = width;
  if (height_ <= 0) height_ = height;

  initialized_ = true;
  return true;
}

bool HtmlBackend::pumpEvents() {
  constexpr double kTargetFrameMs = 1000.0 / 60.0;
  double now = emscripten_get_now();
  double elapsed = lastFrameTime_ > 0.0 ? now - lastFrameTime_ : kTargetFrameMs;
  double remaining = kTargetFrameMs - elapsed;
  emscripten_sleep(remaining > 0.0 ? (unsigned int)remaining : 0);
  lastFrameTime_ = emscripten_get_now();

  emscripten::val win = emscripten::val::global("window");
  width_ = win["innerWidth"].as<int>();
  height_ = win["innerHeight"].as<int>();
  return true;
}

Clay_Dimensions HtmlBackend::windowSize() const { return {(float)width_, (float)height_}; }

bool HtmlBackend::isEntryFocused(int ordinal) const {
  auto it = entryElements_.find(ordinal);
  if (it == entryElements_.end()) return false;
  emscripten::val active = doc_["activeElement"];
  return active.strictlyEquals(it->second);
}

void HtmlBackend::beginFrame() { Clay_SetPointerState({pointerX_, pointerY_}, pointerDown_); }

void HtmlBackend::setCursor(CursorKind cursor) {
  if (cursor == currentCursorKind_) return;
  currentCursorKind_ = cursor;
  const char *css = "default";
  if (cursor == CursorKind::Pointer) css = "pointer";
  else if (cursor == CursorKind::Text) css = "text";
  doc_["body"]["style"].set("cursor", std::string(css));
}

void HtmlBackend::shutdown() {
  if (!initialized_) return;
  initialized_ = false;
  for (auto &entry : entryElements_) entry.second.call<void>("remove");
  for (auto &entry : elementCache_) entry.second.call<void>("remove");
  entryElements_.clear();
  elementCache_.clear();
  if (!root_.isUndefined() && !root_.isNull()) root_.call<void>("remove");
  n8vHtmlSetInstance(nullptr);
}

void HtmlBackend::onPointerMove(float x, float y) {
  pointerX_ = x;
  pointerY_ = y;
}

void HtmlBackend::onPointerDown(bool down) { pointerDown_ = down; }

std::unique_ptr<Backend> makeHtmlBackend() { return std::make_unique<HtmlBackend>(); }

} // namespace n8v::detail
