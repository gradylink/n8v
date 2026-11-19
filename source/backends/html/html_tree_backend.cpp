#include "html_tree_backend.hpp"

#include "html_assets.hpp"
#include "html_builder.hpp"
#include "html_css.hpp"

#include "core/entry_lines.hpp"
#include "core/icon_loader.hpp"
#include "core/image_loader.hpp"
#include "core/markdown/markdown.hpp"
#include "core/markdown/markdown_to_html.hpp"
#include "core/open_url.hpp"
#include "core/style.hpp"
#include "core/theme_color.hpp"
#include "core/tree/dispatch.hpp"
#include "core/tree/persistent.hpp"
#include "core/tree/props.hpp"
#include "core/tree/reconcile.hpp"
#include "core/widgets/page_badge.hpp"

#include <emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/val.h>

#include <algorithm>
#include <atomic>
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
extern "C" char *n8vHtmlMediaUrlBridge(const char *path);
extern "C" char *n8vHtmlBytesUrlBridge(const unsigned char *bytes, int size, const char *mime);
extern "C" void n8vHtmlPickFilesBridge(int id, int multiple, int folder, const char *accept);

static std::string bytesUrl(const std::vector<unsigned char> &bytes, const char *mime) {
  char *url = n8vHtmlBytesUrlBridge(bytes.data(), (int)bytes.size(), mime);
  std::string out = url;
  std::free(url);
  return out;
}

static std::string mediaUrl(const std::string &source) {
  if (source.empty()) return source;
  char *converted = n8vHtmlMediaUrlBridge(source.c_str());
  if (!converted) return source;
  std::string url = converted;
  std::free(converted);
  return url;
}

namespace n8v::detail {
namespace {

using emscripten::val;
using html::Element;
using html::HtmlBuilder;
using html::NativeKind;
using html::NativeNode;
using html::propsOf;
using tree::OwnedProps;
using tree::PNode;
namespace dispatch = tree::dispatch;
namespace css = html_css;

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

struct PendingScroll {
  NativeScrollRequest request;
  double requestedAt = 0.0;
};

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

class HtmlTreeBackend final : public Backend, public HtmlBuilder {
public:
  ~HtmlTreeBackend() override { shutdown(); }

  bool initialize(int width, int height, std::string_view title) override {
    gInstance = this;
    doc_ = val::global("document");
    val win = val::global("window");
    applyStyleFromQueryString();
    doc_.set("title", std::string(title));

    val baseStyle = doc_.call<val>("createElement", std::string("style"));
    baseStyle.set("textContent", HtmlBuilder::baseCss());
    doc_["head"].call<void>("appendChild", baseStyle);

    val dynamicStyle = doc_.call<val>("createElement", std::string("style"));
    dynamicText_ = doc_.call<val>("createTextNode", std::string(""));
    dynamicStyle.call<void>("appendChild", dynamicText_);
    doc_["head"].call<void>("appendChild", dynamicStyle);

    rootEl_ = Element::create("div");
    rootEl_.setAttr("id", "n8v-root");
    doc_["body"].call<void>("appendChild", rootEl_.raw());

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
    constexpr double busyFrameMs = 1000.0 / 60.0, idleFrameMs = 250.0, sliceMs = 8.0;
    constexpr int settleFrames = 4;
    const bool busy = settleFrames_ > 0 || !pendingScrolls_.empty() || !pendingFocus_.empty();
    const double interval = busy ? busyFrameMs : idleFrameMs;
    const double started = lastFrameTime_ > 0.0 ? lastFrameTime_ : emscripten_get_now() - interval;

    bool activity = false, userActivity = false;
    for (;;) {
      const double counter = val::module_property("n8vActivity").as<double>();
      const bool woken = wakeRequested_.exchange(false);
      const bool touched = counter != lastActivityCounter_;
      if (woken || touched) {
        lastActivityCounter_ = counter;
        activity = true;
        userActivity = touched;
        break;
      }
      const double left = interval - (emscripten_get_now() - started);
      if (left <= 0.0) break;
      emscripten_sleep((unsigned int)std::max(1.0, std::min(sliceMs, left)));
    }
    if (activity) {
      const double left = busyFrameMs - (emscripten_get_now() - started);
      if (left > 0.0) emscripten_sleep((unsigned int)left);
      settleFrames_ = std::max(settleFrames_, userActivity ? settleFrames : 1);
    } else if (settleFrames_ > 0) {
      --settleFrames_;
    }
    lastFrameTime_ = emscripten_get_now();
    val win = val::global("window");
    width_ = win["innerWidth"].as<int>();
    height_ = win["innerHeight"].as<int>();
    return true;
  }

  void wake() override { wakeRequested_ = true; }

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
    applyKeepOnScreen();
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
    if (!rootEl_.isNull()) rootEl_.remove();
    gInstance = nullptr;
  }

  bool onKey(int ordinal, int key, unsigned mods) {
    auto it = byOrdinal_.find(ordinal);
    if (it == byOrdinal_.end() || native(*it->second)->kind() != Kind::Entry) return false;
    return dispatch::key(*it->second, (n8v_key)key, mods);
  }

  void onPasteFiles(int ordinal, const std::string &joinedPaths) {
    auto it = byOrdinal_.find(ordinal);
    if (it == byOrdinal_.end()) return;
    std::vector<std::string> paths;
    size_t start = 0;
    while (start <= joinedPaths.size()) {
      const size_t end = joinedPaths.find('\n', start);
      const std::string path = joinedPaths.substr(start, end == std::string::npos ? std::string::npos : end - start);
      if (!path.empty()) paths.push_back(path);
      if (end == std::string::npos) break;
      start = end + 1;
    }
    if (!paths.empty()) dispatch::pasteFiles(*it->second, paths);
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
    case Kind::Sidebar:
      if (type == "click") {
        const auto *o = propsOf<n8v_sidebar_options>(node);
        if (!o || !o->collapsed) break;
        *o->collapsed = !*o->collapsed;
        if (o->on_collapse) o->on_collapse(*o->collapsed, o->on_collapse_userdata);
        apply(nn);
        for (NativeNode *kid : nn.kids)
          if (kid->kind() == Kind::Page) apply(*kid);
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
  void applyStickyBottom();
  void applyKeepOnScreen();
  void resolvePendingScrolls();
  void resolvePendingFocus();

  void cssAppend(const std::string &css) override { dynamicText_.call<void>("appendData", css); }
  void cssReset() override { dynamicText_.set("data", std::string()); }
  std::string mediaUrl(const std::string &source) override { return ::mediaUrl(source); }
  std::string fontUrl(const std::string &, const std::vector<unsigned char> &bytes) override { return bytesUrl(bytes, "font/ttf"); }
  std::string imageUrl(const DecodedImage &image) override { return html_assets::imageDataUri(image); }
  std::string imagePathUrl(const std::string &path, bool decoded) override { return decoded ? std::string() : path; }

  bool hasFocus(const Element &el) const override { return doc_["activeElement"].strictlyEquals(el.raw()); }
  void focus(Element &el) override { el.raw().call<void>("focus"); }
  void autoGrow(Element &el) override { val::global("n8vAutoGrow")(el.raw()); }

  val doc_ = val::undefined();
  Element rootEl_;
  val dynamicText_ = val::undefined();
  struct PendingFocus {
    std::string id;
    int framesWaited;
  };
  std::vector<PendingFocus> pendingFocus_;
  std::vector<PendingScroll> pendingScrolls_;
  std::unordered_map<int, FileDialogDone> fileDialogs_;
  int nextFileDialogId_ = 0;
  int width_ = 0, height_ = 0;
  double lastFrameTime_ = 0.0;
  double lastActivityCounter_ = -1.0;
  int settleFrames_ = 0;
  std::atomic<bool> wakeRequested_{false};
  bool initialized_ = false;
};

void HtmlTreeBackend::applyKeepOnScreen() {
  const val window = val::global("window");
  const double width = window["innerWidth"].as<double>();
  const double height = window["innerHeight"].as<double>();
  for (PNode *pn : live_) {
    NativeNode *nn = native(*pn);
    if (!nn || nn->kind() != Kind::Floating) continue;
    const auto *o = propsOf<n8v_floating_options>(*pn);
    if (!o || !o->keep_on_screen || nn->outer.isNull()) continue;
    const val rect = nn->outer.raw().call<val>("getBoundingClientRect");
    const double w = rect["width"].as<double>(), h = rect["height"].as<double>();
    if (w <= 0 || h <= 0) continue;
    const double margin = o->screen_margin;
    double currentX = 0.0, currentY = 0.0;
    const std::string applied = nn->outer.raw()["style"]["translate"].as<std::string>();
    if (!applied.empty()) std::sscanf(applied.c_str(), "%lfpx %lfpx", &currentX, &currentY);
    const double plainX = rect["left"].as<double>() - currentX, plainY = rect["top"].as<double>() - currentY;
    const double newX = std::clamp(plainX, margin, std::max(margin, width - w - margin)) - plainX;
    const double newY = std::clamp(plainY, margin, std::max(margin, height - h - margin)) - plainY;
    if (std::abs(newX - currentX) < 0.5 && std::abs(newY - currentY) < 0.5) continue;
    nn->outer.raw()["style"].set("translate", css::cssNumber((float)newX) + "px " + css::cssNumber((float)newY) + "px");
  }
}

void HtmlTreeBackend::applyStickyBottom() {
  for (auto &[id, nn] : byId_) {
    if (!nn->stickyBottom) continue;
    val el = nn->outer.raw();
    const double height = el["scrollHeight"].as<double>();
    const double viewport = el["clientHeight"].as<double>();
    if (height != nn->lastScrollHeight || viewport != nn->lastClientHeight) {
      if (nn->stickyAtBottom) el.set("scrollTop", height);
      nn->lastScrollHeight = height;
      nn->lastClientHeight = viewport;
    }
    nn->stickyAtBottom = el["scrollTop"].as<double>() + el["clientHeight"].as<double>() >= height - kStickyBottomEpsilonPx;
  }
}

void HtmlTreeBackend::resolvePendingFocus() {
  for (auto it = pendingFocus_.begin(); it != pendingFocus_.end();) {
    auto found = byId_.find(it->id);
    if (found != byId_.end()) {
      found->second->widget.raw().call<void>("focus");
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
      val el = container->second->outer.raw();
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
        val target = element->second->outer.raw().call<val>("getBoundingClientRect");
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

} // namespace

std::unique_ptr<Backend> makeHtmlTreeBackend() { return std::make_unique<HtmlTreeBackend>(); }

} // namespace n8v::detail

extern "C" {

EMSCRIPTEN_KEEPALIVE void n8v_html_file_dialog_done(int id, const char *paths) {
  if (n8v::detail::gInstance) n8v::detail::gInstance->onFileDialogDone(id, paths ? paths : "");
}

EMSCRIPTEN_KEEPALIVE void n8v_html_open_url(const char *url) {
  if (url) n8v::detail::openUrl(url);
}

EMSCRIPTEN_KEEPALIVE int n8v_html_key(int ordinal, int key, unsigned mods) { return n8v::detail::gInstance && n8v::detail::gInstance->onKey(ordinal, key, mods) ? 1 : 0; }

EMSCRIPTEN_KEEPALIVE void n8v_html_paste_files(int ordinal, const char *paths) {
  if (n8v::detail::gInstance) n8v::detail::gInstance->onPasteFiles(ordinal, paths ? paths : "");
}

EMSCRIPTEN_KEEPALIVE void n8v_html_event(int ordinal, const char *type, const char *value) {
  if (n8v::detail::gInstance) n8v::detail::gInstance->onEvent(ordinal, type ? type : "", value ? value : "");
}

} // extern "C"
