#include "html_tree_backend.hpp"

#include "html_assets.hpp"
#include "html_css.hpp"

#include "core/entry_lines.hpp"
#include "core/icon_loader.hpp"
#include "core/image_loader.hpp"
#include "core/markdown/markdown.hpp"
#include "core/markdown/markdown_to_html.hpp"
#include "core/style.hpp"
#include "core/tree/dispatch.hpp"
#include "core/tree/persistent.hpp"
#include "core/tree/props.hpp"
#include "core/tree/reconcile.hpp"
#include "core/widgets/page_badge.hpp"

#include <emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/val.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

extern "C" void n8vHtmlInstallListenersBridge();
extern "C" int n8vHtmlMessageBridge(const char *text, int confirm);
extern "C" void n8vHtmlPickFilesBridge(int id, int multiple, int folder, const char *accept);

namespace n8v::detail {
namespace {

using emscripten::val;
using tree::OwnedProps;
using tree::PNode;
namespace dispatch = tree::dispatch;
namespace css = html_css;

template <typename... Ts> struct Overloaded : Ts... {
  using Ts::operator()...;
};
template <typename... Ts> Overloaded(Ts...) -> Overloaded<Ts...>;

template <typename T> const T *propsOf(const PNode &node) { return std::get_if<T>(&node.current.props); }

constexpr double kScrollRequestTimeoutMs = 2000.0;
constexpr double kStickyBottomEpsilonPx = 24.0;
constexpr int kDefaultIconSize = 16;

class HtmlTreeBackend;

std::string acceptAttribute(const std::vector<FileDialogFilter> &filters) {
  std::string accept;
  for (const FileDialogFilter &f : filters) {
    for (const std::string &glob : f.patterns) {
      const std::string ext = globExtension(glob);
      if (ext.empty()) return {};
      accept += (accept.empty() ? "." : ",.") + ext;
    }
  }
  return accept;
}
HtmlTreeBackend *gInstance = nullptr;

enum class NativeKind { Root, Flex, Panel, Floating, Sidebar, Page, Button, Text, Checkbox, Toggle, Radio, Entry, Dropdown, Slider, Image, Icon, Document };

struct NativeNode {
  NativeKind kind() const { return node ? (NativeKind)node->current.props.index() : NativeKind::Root; }

  PNode *node = nullptr;
  val outer = val::undefined();
  val content = val::undefined();
  val widget = val::undefined();
  val label = val::undefined();
  val image = val::undefined();

  // sidebar
  val listEl = val::undefined();
  val titleEl = val::undefined();
  val contentArea = val::undefined();

  bool floating = false;
  bool parentIsRow = false;
  bool stickyBottom = false;
  bool stickyAtBottom = true;
  double lastScrollHeight = 0.0;
  std::string lastItemsKey;
  std::string registeredId;
  std::vector<NativeNode *> kids;
};

struct PendingScroll {
  NativeScrollRequest request;
  double requestedAt = 0.0;
};

n8v_sizing fitSizing() { return n8v_sizing_fit(0.0f, 0.0f); }
n8v_sizing growSizing() { return n8v_sizing_grow(0.0f, 0.0f); }

n8v_sizing sizingFor(const NativeNode &nn, bool horizontal) {
  return std::visit(
    Overloaded{
      [&](const n8v_flex_options &o) { return horizontal ? o.width : o.height; },
      [&](const n8v_panel_options &o) { return horizontal ? o.width : o.height; },
      [&](const n8v_floating_options &o) { return horizontal ? o.width : o.height; },
      [&](const n8v_document_options &o) { return horizontal ? o.width : o.height; },
      [&](const n8v_icon_options &o) { return horizontal ? o.width : o.height; },
      [&](const n8v_image_options &o) { return horizontal ? o.width : o.height; },
      [&](const n8v_sidebar_options &) { return growSizing(); },
      [&](const n8v_entry_options &) { return horizontal ? growSizing() : fitSizing(); },
      [&](const n8v_dropdown_options &) { return horizontal ? growSizing() : fitSizing(); },
      [&](const n8v_slider_options &) { return horizontal ? growSizing() : fitSizing(); },
      [&](const auto &) { return fitSizing(); },
    },
    nn.node->current.props
  );
}

ButtonStyle toButtonStyle(n8v_button_style s) {
  switch (s) {
  case N8V_BUTTON_STYLE_SECONDARY:
    return ButtonStyle::Secondary;
  case N8V_BUTTON_STYLE_GHOST:
    return ButtonStyle::Ghost;
  case N8V_BUTTON_STYLE_PRIMARY:
    break;
  }
  return ButtonStyle::Primary;
}

const char *alignItems(n8v_align a) {
  switch (a) {
  case N8V_ALIGN_CENTER:
    return "center";
  case N8V_ALIGN_END:
    return "flex-end";
  case N8V_ALIGN_START:
    break;
  }
  return "flex-start";
}

void attachFraction(n8v_attach_point p, double &fx, double &fy) {
  switch (p) {
  case N8V_ATTACH_POINT_LEFT_TOP:
    fx = 0.0, fy = 0.0;
    break;
  case N8V_ATTACH_POINT_LEFT_CENTER:
    fx = 0.0, fy = 0.5;
    break;
  case N8V_ATTACH_POINT_LEFT_BOTTOM:
    fx = 0.0, fy = 1.0;
    break;
  case N8V_ATTACH_POINT_CENTER_TOP:
    fx = 0.5, fy = 0.0;
    break;
  case N8V_ATTACH_POINT_CENTER_CENTER:
    fx = 0.5, fy = 0.5;
    break;
  case N8V_ATTACH_POINT_CENTER_BOTTOM:
    fx = 0.5, fy = 1.0;
    break;
  case N8V_ATTACH_POINT_RIGHT_TOP:
    fx = 1.0, fy = 0.0;
    break;
  case N8V_ATTACH_POINT_RIGHT_CENTER:
    fx = 1.0, fy = 0.5;
    break;
  case N8V_ATTACH_POINT_RIGHT_BOTTOM:
    fx = 1.0, fy = 1.0;
    break;
  }
}

std::string pct(double fraction) { return css::cssNumber((float)(fraction * 100.0)) + "%"; }

void applyStyleFromQueryString() {
  val params = val::global("URLSearchParams").new_(val::global("location")["search"]);
  val styleParam = params.call<val>("get", std::string("style"));
  if (styleParam.isNull() || styleParam.isUndefined()) return;

  std::string value = styleParam.as<std::string>();
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return std::tolower(c); });

  if (value == "plain") n8v_set_style_family(N8V_STYLE_FAMILY_PLAIN);
  else if (value == "material") n8v_set_style_family(N8V_STYLE_FAMILY_MATERIAL);
  else if (value == "cupertino") n8v_set_style_family(N8V_STYLE_FAMILY_CUPERTINO);
  else if (value == "fluent") n8v_set_style_family(N8V_STYLE_FAMILY_FLUENT);
}

class HtmlTreeBackend final : public Backend, public tree::TreeOps {
public:
  ~HtmlTreeBackend() override { shutdown(); }

  bool initialize(int width, int height, std::string_view title) override {
    gInstance = this;
    doc_ = val::global("document");
    val win = val::global("window");
    applyStyleFromQueryString();
    doc_.set("title", std::string(title));

    val baseStyle = doc_.call<val>("createElement", std::string("style"));
    baseStyle.set(
      "textContent",
      std::string(
        "html,body{margin:0;padding:0;width:100%;height:100%;overflow:hidden;background:#ffffff}\n"
        "*,*::before,*::after{box-sizing:border-box}\n"
        "#n8v-root{position:relative;display:flex;flex-direction:column;align-items:flex-start;width:100%;height:100%;overflow:hidden}\n"
        "#n8v-root button,#n8v-root input,#n8v-root select{font-family:inherit}\n"
        "#n8v-root img{pointer-events:none}\n"
      ) +
        html_assets::fontFaceCss()
    );
    doc_["head"].call<void>("appendChild", baseStyle);

    val dynamicStyle = doc_.call<val>("createElement", std::string("style"));
    dynamicText_ = doc_.call<val>("createTextNode", std::string(""));
    dynamicStyle.call<void>("appendChild", dynamicText_);
    doc_["head"].call<void>("appendChild", dynamicStyle);

    rootEl_ = doc_.call<val>("createElement", std::string("div"));
    rootEl_.call<void>("setAttribute", std::string("id"), std::string("n8v-root"));
    doc_["body"].call<void>("appendChild", rootEl_);

    root_.outer = rootEl_;
    root_.content = rootEl_;
    installRootStyle();
    n8vHtmlInstallListenersBridge();

    width_ = win["innerWidth"].as<int>();
    height_ = win["innerHeight"].as<int>();
    if (width_ <= 0) width_ = width;
    if (height_ <= 0) height_ = height;
    styleGeneration_ = activeStyleGeneration();
    initialized_ = true;
    return true;
  }

  bool pumpEvents() override {
    constexpr double targetFrameMs = 1000.0 / 60.0;
    const double now = emscripten_get_now();
    const double elapsed = lastFrameTime_ > 0.0 ? now - lastFrameTime_ : targetFrameMs;
    const double remaining = targetFrameMs - elapsed;
    emscripten_sleep(remaining > 0.0 ? (unsigned int)remaining : 0);
    lastFrameTime_ = emscripten_get_now();
    val win = val::global("window");
    width_ = win["innerWidth"].as<int>();
    height_ = win["innerHeight"].as<int>();
    return true;
  }

  bool pointerDown() const override { return false; }
  Clay_Dimensions windowSize() const override { return {(float)width_, (float)height_}; }
  Clay_Dimensions measureText(std::string_view, FontFamily, uint16_t, bool, bool) const override { return {0, 0}; }

  bool rendersFromClay() const override { return false; }
  bool rendersNativeChrome() const override { return true; }
  tree::TreeOps *treeOps() override { return this; }
  void beginFrame() override {}
  void present(Clay_RenderCommandArray) override {}
  void setCursor(CursorKind) override {}

  void commitFrame() override {
    if (styleGeneration_ != activeStyleGeneration()) restyleAll();
    applyStickyBottom();
    resolvePendingScrolls();
    resolvePendingFocus();
  }

  bool isHovered(std::string_view id) const override { return hovered_.count(std::string(id)) != 0; }

  void requestScroll(const NativeScrollRequest &request) override { pendingScrolls_.push_back(PendingScroll{request, emscripten_get_now()}); }

  void requestFocus(std::string_view id) override { pendingFocus_.push_back(PendingFocus{std::string(id), 0}); }

  bool showMessageDialog(const MessageDialogRequest &request, MessageDialogDone done) override {
    const std::string text = request.title.empty() ? request.message : request.title + "\n\n" + request.message;
    const bool confirm = request.buttons.size() > 1;
    const int ok = n8vHtmlMessageBridge(text.c_str(), confirm);
    done(confirm && !ok ? 1 : 0);
    return true;
  }

  bool showFileDialog(const FileDialogRequest &request, FileDialogDone done) override {
    if (request.kind == N8V_FILE_DIALOG_SAVE_FILE) return false;
    const std::string accept = request.kind == N8V_FILE_DIALOG_OPEN_FILE ? acceptAttribute(request.filters) : std::string();
    const int id = ++nextFileDialogId_;
    fileDialogs_[id] = std::move(done);
    n8vHtmlPickFilesBridge(id, request.multiple, request.kind == N8V_FILE_DIALOG_OPEN_FOLDER, accept.c_str());
    return true;
  }

  void onFileDialogDone(int id, std::string_view joinedPaths) {
    auto it = fileDialogs_.find(id);
    if (it == fileDialogs_.end()) return;
    FileDialogDone done = std::move(it->second);
    fileDialogs_.erase(it);
    FileDialogResult result;
    for (size_t pos = 0; pos <= joinedPaths.size() && !joinedPaths.empty();) {
      size_t nl = joinedPaths.find('\n', pos);
      if (nl == std::string_view::npos) nl = joinedPaths.size();
      if (nl > pos) result.paths.emplace_back(joinedPaths.substr(pos, nl - pos));
      pos = nl + 1;
    }
    result.status = result.paths.empty() ? FileDialogResult::Status::Cancelled : FileDialogResult::Status::Done;
    done(std::move(result));
  }

  void shutdown() override {
    if (!initialized_) return;
    initialized_ = false;
    if (!rootEl_.isUndefined() && !rootEl_.isNull()) rootEl_.call<void>("remove");
    gInstance = nullptr;
  }

  void create(PNode &node) override {
    auto *nn = new NativeNode;
    nn->node = &node;
    node.handle = nn;
    live_.insert(&node);
    byOrdinal_[node.ordinal] = &node;
    build(*nn);
    apply(*nn);
    registerId(*nn);
  }

  void update(PNode &node, const OwnedProps &, unsigned) override {
    NativeNode &nn = *native(node);
    apply(nn);
    registerId(nn);
    for (NativeNode *kid : nn.kids) refreshChild(nn, *kid);
    if (nn.kind() == Kind::Sidebar) {
      for (NativeNode *kid : nn.kids)
        if (kid->kind() == Kind::Page) apply(*kid);
    }
  }

  void insert(PNode &parent, PNode &child, size_t index) override {
    NativeNode &p = nativeOrRoot(parent);
    NativeNode &c = *native(child);
    p.kids.insert(p.kids.begin() + (ptrdiff_t)index, &c);
    attach(p, c, index);
    refreshChild(p, c);
    afterKidsChanged(p);
  }

  void move(PNode &parent, PNode &child, size_t index) override {
    NativeNode &p = nativeOrRoot(parent);
    NativeNode &c = *native(child);
    const bool hadFocus = !c.widget.isUndefined() && doc_["activeElement"].strictlyEquals(c.widget);
    p.kids.erase(std::find(p.kids.begin(), p.kids.end(), &c));
    p.kids.insert(p.kids.begin() + (ptrdiff_t)index, &c);
    attach(p, c, index);
    if (hadFocus) c.widget.call<void>("focus");
    afterKidsChanged(p);
  }

  void remove(PNode &parent, PNode &child) override {
    NativeNode &p = nativeOrRoot(parent);
    NativeNode &c = *native(child);
    c.outer.call<void>("remove");
    if (c.kind() == Kind::Page) c.content.call<void>("remove");
    p.kids.erase(std::find(p.kids.begin(), p.kids.end(), &c));
    afterKidsChanged(p);
  }

  void destroy(PNode &node) override {
    NativeNode *nn = native(node);
    if (!nn) return;
    unregisterId(*nn);
    live_.erase(&node);
    byOrdinal_.erase(node.ordinal);
    delete nn;
    node.handle = nullptr;
  }

  void onEvent(int ordinal, const std::string &type, const std::string &value) {
    auto it = byOrdinal_.find(ordinal);
    if (it == byOrdinal_.end()) return;
    PNode &node = *it->second;
    NativeNode &nn = *native(node);

    if (type == "hover") {
      const bool hovered = value == "1";
      if (!nn.registeredId.empty()) {
        if (hovered) hovered_.insert(nn.registeredId);
        else hovered_.erase(nn.registeredId);
      }
      dispatch::hover(node, hovered);
      return;
    }
    switch (nn.kind()) {
    case Kind::Button:
    case Kind::Text:
      if (type == "click") dispatch::activate(node);
      break;
    case Kind::Page:
      if (type == "click") {
        dispatch::selectPage(node);
        if (node.parent && node.parent->handle) {
          for (NativeNode *kid : native(*node.parent)->kids)
            if (kid->kind() == Kind::Page) apply(*kid);
        }
      }
      break;
    case Kind::Checkbox:
    case Kind::Toggle:
      if (type == "change") dispatch::setChecked(node, value == "1");
      break;
    case Kind::Radio:
      if (type == "change" && value == "1") dispatch::select(node, 0);
      break;
    case Kind::Entry:
      if (type == "input") dispatch::setText(node, value);
      else if (type == "submit") dispatch::submit(node);
      break;
    case Kind::Dropdown:
      if (type == "change") {
        const auto *o = propsOf<n8v_dropdown_options>(node);
        const int offset = o && o->placeholder && *o->placeholder ? 1 : 0;
        const int index = std::atoi(value.c_str()) - offset;
        if (index >= 0) dispatch::select(node, index);
      }
      break;
    case Kind::Slider:
      if (type == "input") dispatch::setSlider(node, (float)std::atof(value.c_str()));
      break;
    default:
      break;
    }
  }

private:
  using Kind = NativeKind;

  NativeNode *native(PNode &node) const { return static_cast<NativeNode *>(node.handle); }
  NativeNode &nativeOrRoot(PNode &node) { return node.parent ? *native(node) : root_; }

  val createEl(const char *tag) { return doc_.call<val>("createElement", std::string(tag)); }

  static void setCss(val &el, const std::string &cssText) { el["style"].set("cssText", cssText); }
  static void setClass(val &el, const std::string &name) { el.set("className", name); }

  std::string classFor(const std::string &tmpl) {
    auto it = classes_.find(tmpl);
    if (it != classes_.end()) return it->second;
    std::string name = "n8v-c" + std::to_string(nextClass_++);
    dynamicText_.call<void>("appendData", css::expand(tmpl, "." + name));
    classes_.emplace(tmpl, name);
    return name;
  }

  void installRootStyle() { dynamicText_.call<void>("appendData", css::expand(css::textCss(activePaint(), {}), "#n8v-root") + "\n"); }

  void restyleAll() {
    styleGeneration_ = activeStyleGeneration();
    classes_.clear();
    nextClass_ = 0;
    dynamicText_.set("data", std::string());
    installRootStyle();
    for (PNode *pn : live_) apply(*native(*pn));
  }

  void setAttr(val &el, const char *name, const std::string &value) { el.call<void>("setAttribute", std::string(name), value); }

  void bindEvents(NativeNode &nn, val &el, bool click) {
    setAttr(el, "data-n8v", std::to_string(nn.node->ordinal));
    if (click) setAttr(el, "data-n8v-click", "1");
  }

  void setImage(val &img, const DecodedImage *decoded) {
    if (!decoded) {
      img.call<void>("removeAttribute", std::string("src"));
      return;
    }
    img.set("src", html_assets::imageDataUri(*decoded));
  }

  void build(NativeNode &nn);
  void apply(NativeNode &nn);
  void applyButton(NativeNode &nn, const n8v_button_options &o);
  std::string multilineEntryCss(NativeNode &nn, const n8v_entry_options &o, const Paint &paint);
  void applyPage(NativeNode &nn, const n8v_page_options &o);
  void applyLayout(NativeNode &nn);
  std::string containerCss(NativeNode &nn);
  std::string sizingCss(NativeNode &nn);
  std::string floatingCss(NativeNode &nn);
  void layoutPageContent(NativeNode &page);
  void refreshChild(NativeNode &parent, NativeNode &child);
  void attach(NativeNode &parent, NativeNode &child, size_t index);
  void afterKidsChanged(NativeNode &container);
  size_t pageIndex(NativeNode &sidebar, NativeNode &page) const;
  void registerId(NativeNode &nn);
  void unregisterId(NativeNode &nn);
  void applyStickyBottom();
  void resolvePendingScrolls();
  void resolvePendingFocus();

  val doc_ = val::undefined();
  val rootEl_ = val::undefined();
  val dynamicText_ = val::undefined();
  NativeNode root_;
  std::unordered_set<PNode *> live_;
  std::unordered_map<int, PNode *> byOrdinal_;
  std::unordered_map<std::string, NativeNode *> byId_;
  struct PendingFocus {
    std::string id;
    int framesWaited;
  };
  std::vector<PendingFocus> pendingFocus_;
  std::unordered_set<std::string> hovered_;
  std::vector<PendingScroll> pendingScrolls_;
  std::unordered_map<int, FileDialogDone> fileDialogs_;
  int nextFileDialogId_ = 0;
  std::unordered_map<std::string, std::string> classes_;
  int nextClass_ = 0;
  uint64_t styleGeneration_ = 0;
  int width_ = 0, height_ = 0;
  double lastFrameTime_ = 0.0;
  bool initialized_ = false;
};

std::string HtmlTreeBackend::containerCss(NativeNode &nn) {
  std::string out;
  bool hasFloatingKid = false;
  for (NativeNode *kid : nn.kids)
    if (kid->floating) hasFloatingKid = true;

  auto flexBox = [&](n8v_direction direction, uint16_t gap, const n8v_padding *padding, n8v_align hAlign, n8v_align vAlign, bool clipH, bool clipV) {
    const bool row = direction == N8V_DIRECTION_HORIZONTAL;
    out += std::string("display:flex;flex-direction:") + (row ? "row" : "column") + ";gap:" + std::to_string(gap) + "px;";
    if (padding)
      out += "padding:" + std::to_string(padding->top) + "px " + std::to_string(padding->right) + "px " + std::to_string(padding->bottom) + "px " +
             std::to_string(padding->left) + "px;";
    out += std::string("align-items:") + alignItems(row ? vAlign : hAlign) + ";";
    const n8v_align mainAlign = row ? hAlign : vAlign;
    out += std::string("justify-content:") + alignItems(mainAlign) + ";";
    if (clipH || clipV) {
      out += std::string("overflow-x:") + (clipH ? "auto" : "hidden") + ";overflow-y:" + (clipV ? "auto" : "hidden") + ";min-width:0;min-height:0;";
    }
  };
  std::visit(
    Overloaded{
      [&](const n8v_flex_options &o) { flexBox(o.direction, o.gap, &o.padding, o.h_align, o.v_align, o.clip_horizontal, o.clip_vertical); },
      [&](const n8v_panel_options &o) { flexBox(o.direction, o.gap, nullptr, o.h_align, o.v_align, o.clip_horizontal, o.clip_vertical); },
      [&](const n8v_floating_options &) { out += "display:flex;flex-direction:column;align-items:flex-start;"; },
      [&](const n8v_sidebar_options &) { out += "display:flex;flex-direction:row;min-width:0;min-height:0;"; },
      [](const auto &) {},
    },
    nn.node->current.props
  );
  if (hasFloatingKid) out += "position:relative;";
  return out;
}

std::string HtmlTreeBackend::sizingCss(NativeNode &nn) {
  if (nn.floating) return {};
  std::string out;
  for (bool horizontal : {true, false}) {
    const n8v_sizing s = sizingFor(nn, horizontal);
    const bool main = nn.parentIsRow == horizontal;
    const std::string dim = horizontal ? "width" : "height";
    switch (s.mode) {
    case N8V_SIZING_FIT:
      break;
    case N8V_SIZING_GROW:
      if (main) out += "flex:1 1 0%;min-" + dim + ":0;";
      else out += "align-self:stretch;";
      break;
    case N8V_SIZING_FIXED:
      out += dim + ":" + css::cssNumber(s.value) + "px;";
      if (main) out += "flex:0 0 auto;";
      break;
    case N8V_SIZING_PERCENT:
      if (main) out += "flex:0 0 " + css::cssNumber(s.value * 100.0f) + "%;";
      else out += dim + ":" + css::cssNumber(s.value * 100.0f) + "%;";
      break;
    }
    if (s.mode != N8V_SIZING_FIXED) {
      if (s.min > 0.0f) out += "min-" + dim + ":" + css::cssNumber(s.min) + "px;";
      if (s.max > 0.0f) out += "max-" + dim + ":" + css::cssNumber(s.max) + "px;";
    }
  }
  return out;
}

std::string HtmlTreeBackend::floatingCss(NativeNode &nn) {
  const auto *o = propsOf<n8v_floating_options>(*nn.node);
  if (!o) return {};
  double pfx = 0, pfy = 0, efx = 0, efy = 0;
  attachFraction(o->parent_attach_point, pfx, pfy);
  attachFraction(o->element_attach_point, efx, efy);
  std::string out = "position:absolute;z-index:10;left:calc(" + pct(pfx) + " + " + css::cssNumber(o->offset_x) + "px);top:calc(" + pct(pfy) + " + " +
                    css::cssNumber(o->offset_y) + "px);" + "transform:translate(" + pct(-efx) + "," + pct(-efy) + ");";
  auto dim = [&](const n8v_sizing &s, const char *name) {
    if (s.mode == N8V_SIZING_FIXED) out += std::string(name) + ":" + css::cssNumber(s.value) + "px;";
    else if (s.mode == N8V_SIZING_GROW) out += std::string(name) + ":100%;";
  };
  dim(o->width, "width");
  dim(o->height, "height");
  return out;
}

void HtmlTreeBackend::applyLayout(NativeNode &nn) {
  if (!nn.node || nn.outer.isUndefined()) return;
  if (nn.kind() == Kind::Page) return;
  std::string css = containerCss(nn) + sizingCss(nn) + floatingCss(nn);
  if (const auto *slider = propsOf<n8v_slider_options>(*nn.node)) {
    float value = slider->min;
    if (const float *bound = std::get_if<float>(&nn.node->bound)) value = *bound;
    const float span = slider->max - slider->min;
    css += "--n8v-p:" + css::cssNumber(span > 0.0f ? (value - slider->min) / span * 100.0f : 0.0f) + "%;";
  }
  setCss(nn.outer, css);
}

void HtmlTreeBackend::layoutPageContent(NativeNode &page) {
  setCss(
    page.content,
    std::string("flex:1 1 0%;min-width:0;min-height:0;display:") + (page.kids.empty() ? "none" : "flex") + ";flex-direction:column;align-items:flex-start;"
  );
}

void HtmlTreeBackend::refreshChild(NativeNode &parent, NativeNode &child) {
  bool row = false;
  if (parent.node) {
    if (const auto *flex = propsOf<n8v_flex_options>(*parent.node)) row = flex->direction == N8V_DIRECTION_HORIZONTAL;
    else if (const auto *panel = propsOf<n8v_panel_options>(*parent.node)) row = panel->direction == N8V_DIRECTION_HORIZONTAL;
  }
  child.parentIsRow = row;
  applyLayout(child);
}

size_t HtmlTreeBackend::pageIndex(NativeNode &sidebar, NativeNode &page) const {
  size_t index = 0;
  for (NativeNode *kid : sidebar.kids) {
    if (kid == &page) break;
    if (kid->kind() == Kind::Page) ++index;
  }
  return index;
}

void HtmlTreeBackend::attach(NativeNode &parent, NativeNode &child, size_t index) {
  if (parent.kind() == Kind::Sidebar && child.kind() == Kind::Page) {
    val reference = val::null();
    for (size_t i = index + 1; i < parent.kids.size(); ++i) {
      if (parent.kids[i]->kind() == Kind::Page) {
        reference = parent.kids[i]->outer;
        break;
      }
    }
    parent.listEl.call<void>("insertBefore", child.outer, reference);
    parent.contentArea.call<void>("appendChild", child.content);
    return;
  }
  val container = parent.kind() == Kind::Sidebar ? parent.contentArea : parent.content;
  val reference = index + 1 < parent.kids.size() ? parent.kids[index + 1]->outer : val::null();
  container.call<void>("insertBefore", child.outer, reference);
}

void HtmlTreeBackend::afterKidsChanged(NativeNode &container) {
  if (container.kind() == Kind::Sidebar) {
    for (NativeNode *kid : container.kids)
      if (kid->kind() == Kind::Page) layoutPageContent(*kid);
  } else if (container.kind() == Kind::Page) {
    layoutPageContent(container);
  } else {
    applyLayout(container);
  }
}

void HtmlTreeBackend::unregisterId(NativeNode &nn) {
  if (nn.registeredId.empty()) return;
  auto it = byId_.find(nn.registeredId);
  if (it != byId_.end() && it->second == &nn) byId_.erase(it);
  hovered_.erase(nn.registeredId);
  nn.registeredId.clear();
}

void HtmlTreeBackend::registerId(NativeNode &nn) {
  const char *id = tree::idOf(nn.node->current.props);
  const std::string wanted = id ? id : "";
  if (wanted != nn.registeredId) {
    unregisterId(nn);
    if (!wanted.empty()) {
      byId_[wanted] = &nn;
      nn.registeredId = wanted;
    }
  }
  const auto *flex = propsOf<n8v_flex_options>(*nn.node);
  if (!nn.outer.isUndefined() && nn.kind() != Kind::Page && (!wanted.empty() || (flex && flex->on_hover))) {
    setAttr(nn.outer, "data-n8v", std::to_string(nn.node->ordinal));
    setAttr(nn.outer, "data-n8v-hover", std::to_string(nn.node->ordinal));
  }
}

void HtmlTreeBackend::applyStickyBottom() {
  for (auto &[id, nn] : byId_) {
    if (!nn->stickyBottom) continue;
    val el = nn->outer;
    const double height = el["scrollHeight"].as<double>();
    if (height != nn->lastScrollHeight) {
      if (nn->stickyAtBottom) el.set("scrollTop", height);
      nn->lastScrollHeight = height;
    }
    nn->stickyAtBottom = el["scrollTop"].as<double>() + el["clientHeight"].as<double>() >= height - kStickyBottomEpsilonPx;
  }
}

void HtmlTreeBackend::resolvePendingFocus() {
  for (auto it = pendingFocus_.begin(); it != pendingFocus_.end();) {
    auto found = byId_.find(it->id);
    if (found != byId_.end()) {
      found->second->widget.call<void>("focus");
      it = pendingFocus_.erase(it);
    } else if (++it->framesWaited > 10) {
      it = pendingFocus_.erase(it);
    } else {
      ++it;
    }
  }
}

void HtmlTreeBackend::resolvePendingScrolls() {
  const double now = emscripten_get_now();
  for (auto it = pendingScrolls_.begin(); it != pendingScrolls_.end();) {
    const NativeScrollRequest &req = it->request;
    bool resolved = false;
    auto container = byId_.find(req.containerId);
    if (container != byId_.end()) {
      val el = container->second->outer;
      resolved = true;
      switch (req.kind) {
      case NativeScrollRequest::Kind::Bottom:
        el.set("scrollTop", el["scrollHeight"]);
        break;
      case NativeScrollRequest::Kind::Top:
        el.set("scrollTop", 0);
        break;
      case NativeScrollRequest::Kind::Left:
        el.set("scrollLeft", 0);
        break;
      case NativeScrollRequest::Kind::Right:
        el.set("scrollLeft", el["scrollWidth"]);
        break;
      case NativeScrollRequest::Kind::Offset:
        el.set("scrollLeft", (double)req.x);
        el.set("scrollTop", (double)req.y);
        break;
      case NativeScrollRequest::Kind::Element: {
        auto element = byId_.find(req.elementId);
        if (element == byId_.end()) {
          resolved = false;
          break;
        }
        val target = element->second->outer.call<val>("getBoundingClientRect");
        val box = el.call<val>("getBoundingClientRect");
        el.set("scrollTop", el["scrollTop"].as<double>() + target["top"].as<double>() - box["top"].as<double>() - (double)req.paddingTop);
        el.set("scrollLeft", el["scrollLeft"].as<double>() + target["left"].as<double>() - box["left"].as<double>() - (double)req.paddingLeft);
        break;
      }
      }
    }
    if (resolved) {
      it = pendingScrolls_.erase(it);
    } else if (now - it->requestedAt > kScrollRequestTimeoutMs) {
      std::fprintf(stderr, "[n8v] scroll request for container '%s' timed out (container or target element id not found)\n", req.containerId.c_str());
      it = pendingScrolls_.erase(it);
    } else {
      ++it;
    }
  }
}

void HtmlTreeBackend::build(NativeNode &nn) {
  switch (nn.kind()) {
  case Kind::Flex:
  case Kind::Panel:
  case Kind::Floating:
    nn.outer = nn.content = createEl("div");
    nn.floating = nn.kind() == Kind::Floating;
    break;
  case Kind::Text: {
    nn.outer = nn.widget = createEl("span");
    break;
  }
  case Kind::Button: {
    nn.outer = nn.widget = createEl("button");
    nn.outer.call<void>("setAttribute", std::string("type"), std::string("button"));
    nn.label = createEl("span");
    nn.image = createEl("img");
    nn.outer.call<void>("appendChild", nn.label);
    break;
  }
  case Kind::Checkbox:
  case Kind::Toggle:
  case Kind::Radio: {
    nn.outer = createEl("label");
    nn.widget = createEl("input");
    nn.widget.set("type", std::string(nn.kind() == Kind::Radio ? "radio" : "checkbox"));
    if (nn.kind() == Kind::Toggle) setAttr(nn.widget, "role", "switch");
    nn.label = createEl("span");
    nn.outer.call<void>("appendChild", nn.widget);
    nn.outer.call<void>("appendChild", nn.label);
    break;
  }
  case Kind::Entry: {
    const auto *entry = propsOf<n8v_entry_options>(*nn.node);
    if (entry && entry->multiline) {
      nn.outer = nn.widget = createEl("textarea");
    } else {
      nn.outer = nn.widget = createEl("input");
      nn.widget.set("type", std::string("text"));
    }
    break;
  }
  case Kind::Dropdown:
    nn.outer = nn.widget = createEl("select");
    break;
  case Kind::Slider: {
    nn.outer = nn.widget = createEl("input");
    nn.widget.set("type", std::string("range"));
    nn.widget.set("step", std::string("any"));
    break;
  }
  case Kind::Image:
  case Kind::Icon:
    nn.outer = nn.widget = createEl("img");
    break;
  case Kind::Document:
    nn.outer = nn.widget = createEl("div");
    break;
  case Kind::Sidebar: {
    nn.outer = createEl("div");
    nn.listEl = createEl("nav");
    nn.titleEl = createEl("div");
    nn.contentArea = createEl("div");
    nn.listEl.call<void>("appendChild", nn.titleEl);
    nn.outer.call<void>("appendChild", nn.listEl);
    nn.outer.call<void>("appendChild", nn.contentArea);
    nn.content = nn.contentArea;
    break;
  }
  case Kind::Page: {
    nn.outer = createEl("button");
    nn.outer.call<void>("setAttribute", std::string("type"), std::string("button"));
    nn.widget = nn.outer;
    nn.label = createEl("span");
    nn.image = createEl("img");
    nn.outer.call<void>("appendChild", nn.image);
    nn.outer.call<void>("appendChild", nn.label);
    nn.content = createEl("div");
    break;
  }
  case Kind::Root:
    break;
  }
}

void HtmlTreeBackend::applyButton(NativeNode &nn, const n8v_button_options &o) {
  const Paint &paint = activePaint();
  ButtonStyle style = toButtonStyle(o.style);
  if (o.checked) {
    const bool on = *o.checked;
    nn.outer.call<void>("setAttribute", std::string("aria-pressed"), std::string(on ? "true" : "false"));
    if (on && style == ButtonStyle::Ghost) style = ButtonStyle::Secondary;
    else if (on && style == ButtonStyle::Secondary) style = ButtonStyle::Primary;
  }
  setClass(nn.outer, classFor(css::buttonCss(paint, style)));
  bindEvents(nn, nn.outer, true);
  nn.label.set("textContent", nn.node->current.label);
  const bool hasIcon = o.icon && *o.icon;
  if (hasIcon) {
    const ButtonPaint bp = paint.button(style, false, false);
    setImage(nn.image, getOrDecodeIcon(o.icon, bp.fontSize, bp.textColor));
    if (o.icon_position == N8V_ICON_POSITION_TRAILING) nn.outer.call<void>("appendChild", nn.image);
    else nn.outer.call<void>("insertBefore", nn.image, nn.label);
  } else if (!nn.image["parentNode"].isNull()) {
    nn.image.call<void>("remove");
  }
  nn.label.set("hidden", nn.node->current.label.empty());
}

void HtmlTreeBackend::applyPage(NativeNode &nn, const n8v_page_options &o) {
  const Paint &paint = activePaint();
  const NativeNode *sidebar = nn.node->parent ? native(*nn.node->parent) : nullptr;
  int selected = 0;
  if (sidebar) {
    if (const int *bound = std::get_if<int>(&sidebar->node->bound)) selected = *bound;
  }
  const bool isSelected = sidebar && (int)pageIndex(*const_cast<NativeNode *>(sidebar), nn) == selected;
  const ButtonStyle style = isSelected ? ButtonStyle::Primary : ButtonStyle::Secondary;
  setClass(nn.outer, classFor(css::buttonCss(paint, style)));
  setCss(nn.outer, "width:100%;justify-content:flex-start;flex:none;");
  bindEvents(nn, nn.outer, true);
  nn.label.set("textContent", pageLabelWithBadges(o.name ? o.name : "", o.notify, o.badge));
  const bool hasIcon = o.icon && *o.icon;
  const bool hasImage = !hasIcon && o.image && *o.image;
  const ButtonPaint bp = paint.button(style, false, false);
  if (hasIcon) {
    setImage(nn.image, getOrDecodeIcon(o.icon, bp.fontSize, bp.textColor));
  } else if (hasImage) {
    n8v_image_options imageOptions = {};
    imageOptions.source_kind = N8V_IMAGE_SOURCE_PATH;
    imageOptions.path = o.image;
    const int box = bp.fontSize * 2;
    setImage(nn.image, getOrFitImageInSquare(getOrDecodeImage(imageOptions), box));
    nn.image["style"].set("width", std::to_string(box) + "px");
    nn.image["style"].set("height", std::to_string(box) + "px");
  }
  nn.image.set("hidden", !(hasIcon || hasImage));
  layoutPageContent(nn);
}

std::string HtmlTreeBackend::multilineEntryCss(NativeNode &nn, const n8v_entry_options &o, const Paint &paint) {
  const EntryLines lines = entryLines(o);
  const EntryPaint idle = paint.entry(false, false);
  const std::string chrome = css::cssNumber((float)(idle.padding.top + idle.padding.bottom) + 2.0f * idle.borderWidth) + "px";
  auto heightFor = [&](int count) { return "calc(" + std::to_string(count) + " * 1lh + " + chrome + ")"; };

  setAttr(nn.widget, "data-n8v-submit", o.submit_mode == N8V_SUBMIT_ON_CTRL_ENTER ? "ctrl" : "enter");
  std::string extra = "&{resize:none;overflow-y:auto;display:block;";
  if (lines.fixed) {
    extra += "height:" + heightFor(lines.min) + ";}\n";
    nn.widget.call<void>("removeAttribute", std::string("data-n8v-grow"));
  } else {
    extra += "height:auto;field-sizing:content;min-height:" + heightFor(lines.min) + ";max-height:" + heightFor(lines.max) + ";}\n";
    setAttr(nn.widget, "data-n8v-grow", "1");
  }
  return extra;
}

void HtmlTreeBackend::apply(NativeNode &nn) {
  PNode &pn = *nn.node;
  const Paint &paint = activePaint();
  const std::string &label = pn.current.label;

  std::visit(
    Overloaded{
      [&](const std::monostate &) {},
      [&](const n8v_flex_options &o) { nn.stickyBottom = o.stick_to_bottom && o.clip_vertical; },
      [&](const n8v_panel_options &o) { setClass(nn.outer, classFor(css::panelCss(paint, (o.role == N8V_PANEL_ROLE_LIST_ITEM ? PanelRole::ListItem : PanelRole::Card)))); },
      [&](const n8v_floating_options &) {},
      [&](const n8v_sidebar_options &o) {
        float listWidth = o.width.mode == N8V_SIZING_FIXED ? o.width.value : 0.0f;
        std::string listCss = "display:flex;flex-direction:column;align-items:stretch;flex:none;min-height:0;";
        if (listWidth > 0.0f) listCss += "width:" + css::cssNumber(listWidth) + "px;";
        if (o.min_width > 0.0f) listCss += "min-width:" + css::cssNumber(o.min_width) + "px;";
        if (o.max_width > 0.0f) listCss += "max-width:" + css::cssNumber(o.max_width) + "px;";
        setCss(nn.listEl, listCss);
        setClass(nn.listEl, classFor(css::sidebarListCss(paint)));
        setCss(nn.contentArea, "flex:1 1 0%;min-width:0;min-height:0;display:flex;flex-direction:column;align-items:stretch;position:relative;");
        const std::string title = o.title ? o.title : "";
        TextOptions titleOptions;
        titleOptions.bold = true;
        const TextPaint tp = paint.text(titleOptions);
        nn.titleEl.set("textContent", title);
        setCss(
          nn.titleEl,
          std::string("display:") + (title.empty() ? "none" : "block") + ";text-align:center;font-weight:bold;color:" + css::cssColor(tp.color) +
            ";font-family:" + css::cssFontFamily(tp.font) + ";font-size:" + css::cssNumber(tp.fontSize) + "px;padding:4px 0;flex:none;"
        );
      },
      [&](const n8v_page_options &o) { applyPage(nn, o); },
      [&](const n8v_button_options &o) { applyButton(nn, o); },
      [&](const n8v_text_options &o) {
        TextOptions options;
        options.bold = o.bold;
        options.italic = o.italic;
        options.strikethrough = o.strikethrough;
        options.url = o.url ? o.url : "";
        options.color = {o.color.r, o.color.g, o.color.b, o.color.a};
        if (o.on_click) options.onClick = [] {};
        setClass(nn.outer, classFor(css::textCss(paint, options)));
        nn.outer.set("textContent", label);
        const bool link = (o.url && *o.url) || o.on_click;
        bindEvents(nn, nn.outer, link);
        if (!link) nn.outer.call<void>("removeAttribute", std::string("data-n8v-click"));
      },
      [&](const n8v_checkbox_options &) {
        setClass(nn.outer, classFor(css::checkboxCss(paint)));
        bindEvents(nn, nn.widget, false);
        nn.label.set("textContent", label);
        if (const bool *checked = std::get_if<bool>(&pn.bound)) nn.widget.set("checked", *checked);
      },
      [&](const n8v_toggle_options &) {
        setClass(nn.outer, classFor(css::toggleCss(paint)));
        bindEvents(nn, nn.widget, false);
        nn.label.set("textContent", label);
        if (const bool *checked = std::get_if<bool>(&pn.bound)) nn.widget.set("checked", *checked);
      },
      [&](const n8v_radio_options &o) {
        setClass(nn.outer, classFor(css::radioCss(paint)));
        bindEvents(nn, nn.widget, false);
        nn.label.set("textContent", label);
        nn.widget.set("name", "n8v-radio-" + std::to_string((uintptr_t)o.selected));
        if (const int *selected = std::get_if<int>(&pn.bound)) nn.widget.set("checked", *selected == o.value);
      },
      [&](const n8v_entry_options &o) {
        setClass(nn.outer, classFor(css::entryCss(paint) + (o.multiline ? multilineEntryCss(nn, o, paint) : std::string())));
        bindEvents(nn, nn.widget, false);
        if (!o.multiline) nn.widget.set("type", std::string(o.password ? "password" : "text"));
        nn.widget.set("placeholder", std::string(o.placeholder ? o.placeholder : ""));
        if (const std::string *text = std::get_if<std::string>(&pn.bound)) {
          if (nn.widget["value"].as<std::string>() != *text) {
            nn.widget.set("value", *text);
            if (o.multiline) val::global("n8vAutoGrow")(nn.widget);
          }
        }
      },
      [&](const n8v_dropdown_options &o) {
        setClass(nn.outer, classFor(css::dropdownCss(paint)));
        bindEvents(nn, nn.widget, false);
        const bool hasPlaceholder = o.placeholder && *o.placeholder;
        std::string key = std::string(o.placeholder ? o.placeholder : "") + '\n';
        for (size_t i = 0; i < o.item_count; ++i) key += std::string(o.items[i] ? o.items[i] : "") + '\n';
        if (key != nn.lastItemsKey) {
          nn.widget.set("innerHTML", std::string());
          if (hasPlaceholder) {
            val option = createEl("option");
            option.set("textContent", std::string(o.placeholder));
            option.set("value", std::string("-1"));
            option.set("disabled", true);
            option.set("hidden", true);
            nn.widget.call<void>("appendChild", option);
          }
          for (size_t i = 0; i < o.item_count; ++i) {
            val option = createEl("option");
            option.set("textContent", std::string(o.items[i] ? o.items[i] : ""));
            option.set("value", std::to_string(i));
            nn.widget.call<void>("appendChild", option);
          }
          nn.lastItemsKey = key;
        }
        const int *selected = std::get_if<int>(&pn.bound);
        const bool inRange = selected && *selected >= 0 && (size_t)*selected < o.item_count;
        nn.widget.set("selectedIndex", inRange ? *selected + (hasPlaceholder ? 1 : 0) : (hasPlaceholder ? 0 : -1));
      },
      [&](const n8v_slider_options &o) {
        setClass(nn.outer, classFor(css::sliderCss(paint)));
        bindEvents(nn, nn.widget, false);
        nn.widget.set("min", css::cssNumber(o.min));
        nn.widget.set("max", css::cssNumber(o.max));
        if (const float *value = std::get_if<float>(&pn.bound)) nn.widget.set("value", css::cssNumber(*value));
      },
      [&](const n8v_image_options &o) {
        const DecodedImage *decoded = getOrDecodeImage(o);
        CornerRadius radius;
        switch (o.rounding.mode) {
        case N8V_ROUNDING_STYLE_DEFAULT:
          radius = paint.image().cornerRadius;
          break;
        case N8V_ROUNDING_NONE:
          radius = {};
          break;
        case N8V_ROUNDING_FIXED:
          radius = {o.rounding.radius.top_left, o.rounding.radius.top_right, o.rounding.radius.bottom_left, o.rounding.radius.bottom_right};
          break;
        }
        setClass(nn.outer, classFor(css::imageCss(radius)));
        setImage(nn.widget, decoded);
        if (decoded) {
          if (o.width.mode == N8V_SIZING_FIT && o.width.min <= 0.0f && o.width.max <= 0.0f) nn.widget.set("width", decoded->width);
          if (o.height.mode == N8V_SIZING_FIT && o.height.min <= 0.0f && o.height.max <= 0.0f) nn.widget.set("height", decoded->height);
        }
      },
      [&](const n8v_icon_options &o) {
        const IconPaint ip = paint.icon();
        const float size = o.width.mode == N8V_SIZING_FIXED ? o.width.value : ip.defaultSize;
        const Color tint = o.tint.a > 0.0f ? Color{o.tint.r, o.tint.g, o.tint.b, o.tint.a} : ip.tint;
        setClass(nn.outer, classFor(css::imageCss({})));
        setImage(nn.widget, getOrDecodeIcon(o.name ? o.name : "", (uint16_t)size, tint));
        nn.widget.set("width", (int)size);
        nn.widget.set("height", (int)size);
      },
      [&](const n8v_document_options &o) {
        setClass(nn.outer, classFor(css::documentCss(paint)));
        const markdown::CachedDocument cached = markdown::getOrParse(o.markdown ? o.markdown : "");
        markdown::HtmlOptions htmlOptions;
        htmlOptions.linkStyler = markdown::makeLinkStyler(o.link_style, o.link_style_userdata);
        htmlOptions.defaultImageHeight = (int)(paint.text({}).fontSize * 1.4f);
        const std::string html = markdown::toHtml(*cached.document, htmlOptions);
        if (nn.lastItemsKey != html) {
          nn.widget.set("innerHTML", html);
          nn.lastItemsKey = html;
        }
      },
    },
    pn.current.props
  );
  applyLayout(nn);
}

} // namespace

std::unique_ptr<Backend> makeHtmlTreeBackend() { return std::make_unique<HtmlTreeBackend>(); }

} // namespace n8v::detail

extern "C" {

EMSCRIPTEN_KEEPALIVE void n8v_html_file_dialog_done(int id, const char *paths) {
  if (n8v::detail::gInstance) n8v::detail::gInstance->onFileDialogDone(id, paths ? paths : "");
}

EMSCRIPTEN_KEEPALIVE void n8v_html_event(int ordinal, const char *type, const char *value) {
  if (n8v::detail::gInstance) n8v::detail::gInstance->onEvent(ordinal, type ? type : "", value ? value : "");
}

} // extern "C"
