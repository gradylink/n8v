#include "ftxui_tree_backend.hpp"

#include "core/entry_lines.hpp"
#include "core/image_loader.hpp"
#include "core/markdown/markdown.hpp"
#include "core/theme_color.hpp"
#include "core/tree/dispatch.hpp"
#include "core/tree/persistent.hpp"
#include "core/tree/props.hpp"
#include "core/tree/reconcile.hpp"

#include "backends/text_edit_utils.hpp"
#include "kitty_graphics.hpp"
#include "nerd_icons.hpp"

#include <ftxui/component/app.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/loop.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/node.hpp>
#include <ftxui/screen/string.hpp>
#include <ftxui/screen/terminal.hpp>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

namespace n8v::detail {
namespace {

namespace ui = ftxui;
using tree::OwnedProps;
using tree::PNode;
namespace dispatch = tree::dispatch;

template <typename T> const T *propsOf(const PNode &node) { return std::get_if<T>(&node.current.props); }

constexpr float kCellWidthPx = 8.0f;
constexpr float kCellHeightPx = 16.0f;
constexpr int kTickMs = 33;
constexpr int kIdleTickEvery = 3;
constexpr int kWheelRows = 3;
constexpr int kDefaultEntryColumns = 20;

int cellsX(float px) {
  if (px <= 0.0f) return 0;
  return std::max(1, (int)std::lround(px / kCellWidthPx));
}
int cellsY(float px) {
  if (px <= 0.0f) return 0;
  return std::max(1, (int)std::lround(px / kCellHeightPx));
}
int offsetX(float px) { return (int)std::lround(px / kCellWidthPx); }
int offsetY(float px) { return (int)std::lround(px / kCellHeightPx); }
int paddingX(uint16_t px) { return (int)std::lround(px / kCellWidthPx); }
int paddingY(uint16_t px) { return (int)std::lround(px / kCellHeightPx); }

bool inside(const ui::Box &box, int x, int y) { return x >= box.x_min && x <= box.x_max && y >= box.y_min && y <= box.y_max; }

enum class IconStyle { Nerd, Unicode };

IconStyle detectIconStyle() {
  const char *v = std::getenv("N8V_TUI_ICONS");
  return v && std::string_view(v) == "unicode" ? IconStyle::Unicode : IconStyle::Nerd;
}

enum class Kind {
  Root,
  Flex,
  Panel,
  Floating,
  Sidebar,
  Page,
  Button,
  Text,
  Checkbox,
  Toggle,
  Radio,
  Entry,
  Dropdown,
  Slider,
  Image,
  Icon,
  Document,
  SidebarSection,
  SidebarDivider,
  Video,
  Audio,
  Divider
};

struct NativeNode {
  Kind kind() const { return node ? (Kind)node->current.props.index() : Kind::Root; }

  PNode *node = nullptr;
  NativeNode *parent = nullptr;
  std::vector<NativeNode *> kids;
  std::string registeredId;

  ui::Box box;
  ui::Box view;
  bool hasView = false;

  bool hovered = false;

  // Entry
  std::string localText;
  size_t cursor = 0;
  int hscroll = 0;
  int vscroll = 0;

  // Dropdown
  bool open = false;
  int highlighted = 0;
  std::vector<ui::Box> itemBoxes;

  // Scroll containers
  int scrollX = 0;
  int scrollY = 0;
  int contentW = 0;
  int contentH = 0;
  bool sticky = false;
  bool stuck = true;
  bool scrollFocused = false;
};

struct PendingScroll {
  NativeScrollRequest request;
  int framesLeft = 30;
};

struct DialogState {
  MessageDialogRequest request;
  MessageDialogDone done;
  int selected = 0;
  std::vector<ui::Box> buttonBoxes;
};

class FtxuiTreeBackend;

class ScrollNode final : public ui::Node {
public:
  ScrollNode(ui::Element child, NativeNode &state, bool vertical, bool horizontal) : state_(state), vertical_(vertical), horizontal_(horizontal) {
    children_.push_back(std::move(child));
  }

  void ComputeRequirement() override {
    children_[0]->ComputeRequirement();
    const ui::Requirement inner = children_[0]->requirement();
    requirement_ = {};
    requirement_.min_x = horizontal_ ? 0 : inner.min_x;
    requirement_.min_y = vertical_ ? 0 : inner.min_y;
    requirement_.flex_grow_x = horizontal_ ? 1 : inner.flex_grow_x;
    requirement_.flex_shrink_x = horizontal_ ? 1 : inner.flex_shrink_x;
    requirement_.flex_grow_y = vertical_ ? 1 : inner.flex_grow_y;
    requirement_.flex_shrink_y = vertical_ ? 1 : inner.flex_shrink_y;
    requirement_.focused = inner.focused;
  }

  void SetBox(ui::Box box) override {
    ui::Node::SetBox(box);
    const ui::Requirement inner = children_[0]->requirement();
    const int viewW = box.x_max - box.x_min + 1;
    const int viewH = box.y_max - box.y_min + 1;
    const int contentW = horizontal_ ? std::max(inner.min_x, viewW) : viewW;
    const int contentH = vertical_ ? std::max(inner.min_y, viewH) : viewH;

    state_.view = box;
    state_.hasView = true;
    state_.contentW = contentW;
    state_.contentH = contentH;
    const int maxY = std::max(0, contentH - viewH);
    const int maxX = std::max(0, contentW - viewW);
    if (state_.sticky && state_.stuck) state_.scrollY = maxY;
    state_.scrollY = std::clamp(state_.scrollY, 0, maxY);
    state_.scrollX = std::clamp(state_.scrollX, 0, maxX);

    const int x = box.x_min - state_.scrollX;
    const int y = box.y_min - state_.scrollY;
    children_[0]->SetBox(ui::Box{x, x + contentW - 1, y, y + contentH - 1});
  }

  void Render(ui::Screen &screen) override {
    const ui::Box saved = screen.stencil;
    screen.stencil = ui::Box::Intersection(saved, box_);
    children_[0]->Render(screen);
    screen.stencil = saved;

    const int viewH = box_.y_max - box_.y_min + 1;
    if (!vertical_ || state_.contentH <= viewH || viewH < 2) return;
    const int thumbH = std::max(1, viewH * viewH / state_.contentH);
    const int range = state_.contentH - viewH;
    const int thumbTop = range > 0 ? state_.scrollY * (viewH - thumbH) / range : 0;
    for (int row = 0; row < viewH; ++row) {
      if (!inside(screen.stencil, box_.x_max, box_.y_min + row)) continue;
      ui::Cell &pixel = screen.PixelAt(box_.x_max, box_.y_min + row);
      const bool thumb = row >= thumbTop && row < thumbTop + thumbH;
      pixel.character = thumb ? "█" : "│";
      pixel.dim = !thumb && !state_.scrollFocused;
      if (state_.scrollFocused) pixel.foreground_color = ui::Color::Blue;
    }
  }

private:
  NativeNode &state_;
  bool vertical_;
  bool horizontal_;
};

class CrossAlignNode : public ui::Node {
public:
  CrossAlignNode(ui::Element child, bool horizontalMain, bool center) : ui::Node(ui::Elements{std::move(child)}), horizontalMain_(horizontalMain), center_(center) {}

  void ComputeRequirement() override {
    children_[0]->ComputeRequirement();
    requirement_ = children_[0]->requirement();
    if (horizontalMain_) requirement_.flex_grow_y = requirement_.flex_shrink_y = 0;
    else requirement_.flex_grow_x = requirement_.flex_shrink_x = 0;
  }

  void SetBox(ui::Box box) override {
    ui::Node::SetBox(box);
    const auto &req = children_[0]->requirement();
    ui::Box inner = box;
    if (horizontalMain_) {
      const int free = std::max(0, (box.y_max - box.y_min + 1) - req.min_y);
      const int shift = center_ ? free / 2 : free;
      inner.y_min = box.y_min + shift;
      inner.y_max = std::min(box.y_max, inner.y_min + req.min_y - 1);
    } else {
      const int free = std::max(0, (box.x_max - box.x_min + 1) - req.min_x);
      const int shift = center_ ? free / 2 : free;
      inner.x_min = box.x_min + shift;
      inner.x_max = std::min(box.x_max, inner.x_min + req.min_x - 1);
    }
    children_[0]->SetBox(inner);
  }

private:
  bool horizontalMain_;
  bool center_;
};

class PaddingNode final : public ui::Node {
public:
  PaddingNode(ui::Element child, int left, int right, int top, int bottom) : left_(left), right_(right), top_(top), bottom_(bottom) { children_.push_back(std::move(child)); }

  void ComputeRequirement() override {
    children_[0]->ComputeRequirement();
    requirement_ = children_[0]->requirement();
    requirement_.min_x += left_ + right_;
    requirement_.min_y += top_ + bottom_;
    if (requirement_.focused.enabled) requirement_.focused.box.Shift(left_, top_);
  }

  void SetBox(ui::Box box) override {
    ui::Node::SetBox(box);
    children_[0]->SetBox(ui::Box{box.x_min + left_, box.x_max - right_, box.y_min + top_, box.y_max - bottom_});
  }

  void Render(ui::Screen &screen) override { children_[0]->Render(screen); }

private:
  int left_, right_, top_, bottom_;
};

class TightBoxNode final : public ui::Node {
public:
  TightBoxNode(ui::Element child, ui::Box &out) : out_(out) { children_.push_back(std::move(child)); }

  void ComputeRequirement() override {
    children_[0]->ComputeRequirement();
    requirement_ = children_[0]->requirement();
  }

  void SetBox(ui::Box box) override {
    ui::Node::SetBox(box);
    const ui::Requirement inner = children_[0]->requirement();
    box.x_max = std::min(box.x_max, box.x_min + std::max(inner.min_x, 1) - 1);
    box.y_max = std::min(box.y_max, box.y_min + std::max(inner.min_y, 1) - 1);
    out_ = box;
    children_[0]->SetBox(box);
  }

  void Render(ui::Screen &screen) override { children_[0]->Render(screen); }

private:
  ui::Box &out_;
};

struct OverlayHost {
  std::vector<std::function<void(ui::Screen &)>> pending;
  KittyGraphics *kitty = nullptr;
};

class OverlayNode final : public ui::Node {
public:
  using Place = std::function<ui::Box(int width, int height, const ui::Box &clip)>;

  OverlayNode(ui::Element child, OverlayHost &host, Place place) : host_(host), place_(std::move(place)) { children_.push_back(ui::clear_under(std::move(child))); }

  void ComputeRequirement() override {
    children_[0]->ComputeRequirement();
    requirement_ = {};
  }

  void Render(ui::Screen &queuedOn) override {
    const ui::Box clip = queuedOn.stencil;
    host_.pending.push_back([this, clip](ui::Screen &screen) {
      const ui::Requirement inner = children_[0]->requirement();
      const ui::Box placed = place_(inner.min_x, inner.min_y, clip);
      if (placed.x_max < placed.x_min || placed.y_max < placed.y_min) return;
      children_[0]->SetBox(placed);
      if (host_.kitty) host_.kitty->occlude(placed.x_min, placed.x_max, placed.y_min, placed.y_max);
      const ui::Box saved = screen.stencil;
      screen.stencil = ui::Box::Intersection(saved, placed);
      children_[0]->Render(screen);
      screen.stencil = saved;
    });
  }

private:
  OverlayHost &host_;
  Place place_;
};

class LayerRoot final : public ui::Node {
public:
  LayerRoot(ui::Element child, OverlayHost &host, std::function<void()> beforeRender, std::function<void()> afterRender)
      : host_(host), beforeRender_(std::move(beforeRender)), afterRender_(std::move(afterRender)) {
    children_.push_back(std::move(child));
  }

  void ComputeRequirement() override {
    children_[0]->ComputeRequirement();
    requirement_ = children_[0]->requirement();
  }

  void SetBox(ui::Box box) override {
    ui::Node::SetBox(box);
    children_[0]->SetBox(box);
  }

  void Render(ui::Screen &screen) override {
    host_.pending.clear();
    beforeRender_();
    children_[0]->Render(screen);
    for (size_t i = 0; i < host_.pending.size(); ++i) host_.pending[i](screen);
    host_.pending.clear();
    afterRender_();
  }

private:
  OverlayHost &host_;
  std::function<void()> beforeRender_;
  std::function<void()> afterRender_;
};

class ImageNode final : public ui::Node {
public:
  ImageNode(const DecodedImage *image, int cols, int rows, uint32_t kittyId, KittyGraphics *direct)
      : image_(image), cols_(cols), rows_(rows), kittyId_(kittyId), direct_(direct) {}

  void ComputeRequirement() override {
    requirement_ = {};
    requirement_.min_x = cols_;
    requirement_.min_y = rows_;
  }

  void Render(ui::Screen &screen) override {
    if (!image_ || !image_->rgba || image_->width <= 0 || image_->height <= 0) return;
    if (direct_ && kittyId_) {
      const ui::Box grid{box_.x_min, box_.x_min + cols_ - 1, box_.y_min, box_.y_min + rows_ - 1};
      const ui::Box visible = ui::Box::Intersection(ui::Box::Intersection(grid, box_), screen.stencil);
      if (visible.x_max >= visible.x_min && visible.y_max >= visible.y_min) {
        direct_->place(
          kittyId_,
          visible.x_min,
          visible.y_min,
          visible.x_min - box_.x_min,
          visible.y_min - box_.y_min,
          visible.x_max - box_.x_min + 1,
          visible.y_max - box_.y_min + 1,
          cols_,
          rows_
        );
      }
      return;
    }

    const int cols = std::min(cols_, box_.x_max - box_.x_min + 1);
    const int rows = std::min(rows_, box_.y_max - box_.y_min + 1);
    if (cols <= 0 || rows <= 0) return;

    if (kittyId_) {
      uint8_t r, g, b;
      KittyGraphics::idToRgb(kittyId_, r, g, b);
      const ui::Color idColor = ui::Color::RGB(r, g, b);
      for (int cy = 0; cy < rows; ++cy) {
        for (int cx = 0; cx < cols; ++cx) {
          const int x = box_.x_min + cx, y = box_.y_min + cy;
          if (!inside(screen.stencil, x, y)) continue;
          ui::Cell &cell = screen.PixelAt(x, y);
          cell.character = KittyGraphics::placeholder(cy, cx);
          cell.foreground_color = idColor;
        }
      }
      return;
    }

    auto sample = [&](int cx, int halfRow) {
      const int px = std::min(image_->width - 1, cx * image_->width / cols);
      const int py = std::min(image_->height - 1, halfRow * image_->height / (rows * 2));
      return image_->rgba + ((size_t)py * image_->width + px) * 4;
    };
    for (int cy = 0; cy < rows; ++cy) {
      for (int cx = 0; cx < cols; ++cx) {
        if (!inside(screen.stencil, box_.x_min + cx, box_.y_min + cy)) continue;
        const uint8_t *top = sample(cx, cy * 2);
        const uint8_t *bottom = sample(cx, cy * 2 + 1);
        ui::Cell &pixel = screen.PixelAt(box_.x_min + cx, box_.y_min + cy);
        pixel.character = "\u2580";
        if (top[3] >= 128) pixel.foreground_color = ui::Color::RGB(top[0], top[1], top[2]);
        else pixel.character = bottom[3] >= 128 ? "\u2584" : " ";
        if (bottom[3] >= 128) {
          if (top[3] >= 128) pixel.background_color = ui::Color::RGB(bottom[0], bottom[1], bottom[2]);
          else pixel.foreground_color = ui::Color::RGB(bottom[0], bottom[1], bottom[2]);
        }
      }
    }
  }

private:
  const DecodedImage *image_;
  int cols_;
  int rows_;
  uint32_t kittyId_;
  KittyGraphics *direct_;
};

std::optional<ui::Color> explicitColor(const n8v_color &requested) {
  const n8v_color c = resolveThemeColor(requested, true);
  const bool isDefault = c.a <= 0.0f || (c.r == 0.0f && c.g == 0.0f && c.b == 0.0f && c.a == 255.0f);
  if (isDefault) return std::nullopt;
  return ui::Color::RGB((uint8_t)std::clamp(c.r, 0.0f, 255.0f), (uint8_t)std::clamp(c.g, 0.0f, 255.0f), (uint8_t)std::clamp(c.b, 0.0f, 255.0f));
}

ui::Color toColor(const n8v_color &c) {
  return ui::Color::RGB((uint8_t)std::clamp(c.r, 0.0f, 255.0f), (uint8_t)std::clamp(c.g, 0.0f, 255.0f), (uint8_t)std::clamp(c.b, 0.0f, 255.0f));
}

const char *unicodeIconGlyph(const char *name) {
  static const std::pair<std::string_view, const char *> glyphs[] = {
    {"home", "\u2302"},
    {"settings", "*"},
    {"file", "\u2261"},
    {"info", "i"},
    {"plus", "+"},
    {"x", "\u00d7"},
    {"close", "\u00d7"},
    {"check", "\u2713"},
    {"search", "?"},
    {"trash", "\u2717"},
    {"star", "\u2605"},
    {"heart", "\u2665"},
    {"menu", "\u2630"},
    {"chevron-right", "\u203a"},
    {"chevron-left", "\u2039"},
    {"arrow-right", "\u2192"},
    {"arrow-left", "\u2190"},
    {"user", "@"},
    {"folder", "\u25a4"},
    {"notification", "\u266a"},
    {"notification-off", "\u00b7"},
  };
  if (!name) return nullptr;
  for (const auto &[key, glyph] : glyphs)
    if (key == name) return glyph;
  return nullptr;
}

std::vector<std::string> splitLines(std::string_view text) {
  std::vector<std::string> lines;
  size_t start = 0;
  for (;;) {
    const size_t end = text.find('\n', start);
    lines.emplace_back(text.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start));
    if (end == std::string_view::npos) break;
    start = end + 1;
  }
  return lines;
}

size_t glyphIndexAt(const std::vector<std::string> &glyphs, size_t byteOffset) {
  size_t bytes = 0;
  for (size_t i = 0; i < glyphs.size(); ++i) {
    if (bytes >= byteOffset) return i;
    bytes += glyphs[i].size();
  }
  return glyphs.size();
}

size_t byteOffsetOfGlyph(const std::vector<std::string> &glyphs, size_t index) {
  size_t bytes = 0;
  for (size_t i = 0; i < std::min(index, glyphs.size()); ++i) bytes += glyphs[i].size();
  return bytes;
}

int redirectStderrToLog() {
  if (!isatty(STDERR_FILENO)) return -1;
  std::string path;
  if (const char *explicitPath = std::getenv("N8V_TUI_LOG")) path = explicitPath;
  else {
    const char *state = std::getenv("XDG_STATE_HOME");
    const char *home = std::getenv("HOME");
    std::string dir = state && *state ? std::string(state) : home && *home ? std::string(home) + "/.local/state" : std::string();
    if (!dir.empty()) {
      dir += "/n8v";
      mkdir((dir.substr(0, dir.size() - 4)).c_str(), 0700);
      mkdir(dir.c_str(), 0700);
      path = dir + "/tui.log";
    }
  }
  int target = path.empty() ? -1 : open(path.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0600);
  if (target < 0) target = open("/dev/null", O_WRONLY);
  if (target < 0) return -1;
  const int saved = dup(STDERR_FILENO);
  dup2(target, STDERR_FILENO);
  close(target);
  return saved;
}

class FtxuiTreeBackend final : public Backend, public tree::TreeOps {
public:
  ~FtxuiTreeBackend() override { shutdown(); }

  bool initialize(int, int, std::string_view) override {
    if (!isatty(STDOUT_FILENO) || !isatty(STDIN_FILENO)) {
      std::fprintf(stderr, "[n8v] The terminal backend needs an interactive terminal.\n");
      return false;
    }
    savedStderr_ = redirectStderrToLog();
    app_.emplace(ui::App::Fullscreen());
    app_->ForceHandleCtrlC(false);
    kitty_.setProtocol(imageProtocol_);
    overlays_.kitty = &kitty_;
    root_ = ui::CatchEvent(ui::Renderer([this] { return renderRoot(); }), [this](ui::Event event) { return onEvent(std::move(event)); });
    loop_ = std::make_unique<ui::Loop>(&*app_, root_);

    running_ = true;
    ticker_ = std::thread([this] {
      int idle = 0;
      while (running_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(kTickMs));
        if (!running_) break;
        const bool busy = busyTicks_.load() > 0;
        if (busy) --busyTicks_;
        if (busy || ++idle % kIdleTickEvery == 0) app_->PostEvent(ui::Event::Custom);
      }
    });
    initialized_ = true;
    return true;
  }

  bool pumpEvents() override {
    if (!loop_) return false;
    loop_->RunOnceBlocking();
    kitty_.endFrame();
    return !loop_->HasQuitted();
  }

  void wake() override {
    busyTicks_ = kBusyTicks;
    if (app_ && initialized_) app_->PostEvent(ui::Event::Custom);
  }

  bool prefersDark() const override { return true; }
  bool pointerDown() const override { return false; }

  Clay_Dimensions windowSize() const override {
    const ui::Dimensions size = ui::Terminal::Size();
    return {(float)size.dimx * kCellWidthPx, (float)size.dimy * kCellHeightPx};
  }
  Clay_Dimensions measureText(std::string_view, FontFamily, uint16_t, bool, bool) const override { return {0, 0}; }
  Clay_Dimensions cellSize() const override { return {kCellWidthPx, kCellHeightPx}; }

  bool rendersFromClay() const override { return false; }
  bool rendersNativeChrome() const override { return true; }
  tree::TreeOps *treeOps() override { return this; }
  void beginFrame() override {}
  void present(Clay_RenderCommandArray) override {}
  void setCursor(CursorKind) override {}
  void commitFrame() override {}

  bool isEntryFocused(int ordinal) const override { return focused_ && focused_->kind() == Kind::Entry && focused_->node->ordinal == ordinal; }
  bool consumeEscapePress() override { return std::exchange(escapePressed_, false); }
  bool isHovered(std::string_view id) const override { return hoveredIds_.count(std::string(id)) != 0; }

  void requestScroll(const NativeScrollRequest &request) override { pendingScrolls_.push_back({request, 30}); }

  bool scrollState(std::string_view containerId, float &offsetX, float &offsetY, float &viewWidth, float &viewHeight) const override {
    auto it = byId_.find(std::string(containerId));
    if (it == byId_.end() || !it->second->hasView) return false;
    const NativeNode &n = *it->second;
    offsetX = (float)n.scrollX * kCellWidthPx;
    offsetY = (float)n.scrollY * kCellHeightPx;
    viewWidth = (float)(n.view.x_max - n.view.x_min + 1) * kCellWidthPx;
    viewHeight = (float)(n.view.y_max - n.view.y_min + 1) * kCellHeightPx;
    return true;
  }

  void requestFocus(std::string_view id) override {
    auto it = byId_.find(std::string(id));
    if (it != byId_.end()) focus(it->second);
  }

  bool showMessageDialog(const MessageDialogRequest &request, MessageDialogDone done) override {
    if (dialog_) return false;
    dialog_ = DialogState{request, std::move(done), 0, {}};
    busyTicks_ = kBusyTicks;
    return true;
  }

  void shutdown() override {
    if (!initialized_) return;
    initialized_ = false;
    running_ = false;
    if (ticker_.joinable()) ticker_.join();
    kitty_.deleteAll();
    loop_.reset();
    app_.reset();
    root_ = nullptr;
    if (savedStderr_ >= 0) {
      dup2(savedStderr_, STDERR_FILENO);
      close(savedStderr_);
      savedStderr_ = -1;
    }
  }

  void create(PNode &node) override {
    auto &nn = owned_[&node];
    nn = std::make_unique<NativeNode>();
    nn->node = &node;
    node.handle = nn.get();
    updateId(*nn);
  }

  void update(PNode &node, const OwnedProps &, unsigned) override {
    NativeNode &nn = *native(node);
    updateId(nn);
    if (const auto *flex = propsOf<n8v_flex_options>(node)) nn.sticky = flex->clip_vertical && flex->stick_to_bottom;
  }

  void insert(PNode &parent, PNode &child, size_t index) override {
    NativeNode &p = nativeOrRoot(parent);
    NativeNode &c = *native(child);
    c.parent = &p;
    p.kids.insert(p.kids.begin() + (ptrdiff_t)std::min(index, p.kids.size()), &c);
    if (const auto *flex = propsOf<n8v_flex_options>(child)) c.sticky = flex->clip_vertical && flex->stick_to_bottom;
  }

  void move(PNode &parent, PNode &child, size_t index) override {
    NativeNode &p = nativeOrRoot(parent);
    NativeNode &c = *native(child);
    p.kids.erase(std::find(p.kids.begin(), p.kids.end(), &c));
    p.kids.insert(p.kids.begin() + (ptrdiff_t)std::min(index, p.kids.size()), &c);
  }

  void remove(PNode &parent, PNode &child) override {
    NativeNode &p = nativeOrRoot(parent);
    p.kids.erase(std::find(p.kids.begin(), p.kids.end(), native(child)));
    native(child)->parent = nullptr;
  }

  void destroy(PNode &node) override {
    NativeNode *nn = native(node);
    if (!nn) return;
    if (!nn->registeredId.empty()) {
      auto it = byId_.find(nn->registeredId);
      if (it != byId_.end() && it->second == nn) byId_.erase(it);
      hoveredIds_.erase(nn->registeredId);
    }
    if (focused_ == nn) focused_ = nullptr;
    if (dragging_ == nn) dragging_ = nullptr;
    if (dragImage_ == nn) dragImage_ = nullptr;
    std::erase(lastHoverPath_, nn);
    std::erase(focusables_, nn);
    std::erase(floatings_, nn);
    node.handle = nullptr;
    owned_.erase(&node);
  }

private:
  static constexpr int kBusyTicks = 20;

  NativeNode *native(PNode &node) const { return static_cast<NativeNode *>(node.handle); }
  NativeNode &nativeOrRoot(PNode &node) { return node.handle ? *native(node) : rootNode_; }

  void updateId(NativeNode &nn) {
    const char *id = tree::idOf(nn.node->current.props);
    const std::string fresh = id ? id : "";
    if (fresh == nn.registeredId) return;
    if (!nn.registeredId.empty()) {
      auto it = byId_.find(nn.registeredId);
      if (it != byId_.end() && it->second == &nn) byId_.erase(it);
    }
    nn.registeredId = fresh;
    if (!fresh.empty()) byId_[fresh] = &nn;
  }

  ui::Element renderRoot() {
    kitty_.beginFrame();
    focusables_.clear();
    floatings_.clear();
    ui::Elements top = renderKids(rootNode_);
    ui::Element body = ui::vbox(std::move(top));
    if (dialog_) body = ui::dbox({body, renderDialog()});
    if (!focused_ && !focusables_.empty()) focused_ = focusables_.front();
    return std::make_shared<LayerRoot>(std::move(body), overlays_, [this] { kitty_.restartPlacements(); }, [this] { afterRender(); });
  }

  ui::Elements renderKids(NativeNode &n) {
    ui::Elements out;
    for (NativeNode *kid : n.kids) out.push_back(render(*kid));
    return out;
  }

  ui::Element render(NativeNode &n) {
    ui::Element e;
    switch (n.kind()) {
    case Kind::Flex:
      e = renderFlex(n);
      break;
    case Kind::Panel:
      e = renderPanel(n);
      break;
    case Kind::Floating:
      e = renderFloating(n);
      break;
    case Kind::Sidebar:
      e = renderSidebar(n);
      break;
    case Kind::Page:
      e = renderKidsBox(n, N8V_DIRECTION_VERTICAL);
      break;
    case Kind::Button:
      e = renderButton(n);
      break;
    case Kind::Text:
      e = renderText(n);
      break;
    case Kind::Checkbox:
      e = renderChoice(n, checkedOf(n) ? "[✓] " : "[ ] ");
      break;
    case Kind::Toggle:
      e = renderChoice(n, checkedOf(n) ? "[ ●] " : "[● ] ");
      break;
    case Kind::Radio:
      e = renderChoice(n, radioSelected(n) ? "(●) " : "( ) ");
      break;
    case Kind::Entry:
      e = renderEntry(n);
      break;
    case Kind::Dropdown:
      e = renderDropdown(n);
      break;
    case Kind::Slider:
      e = renderSlider(n);
      break;
    case Kind::Image:
      e = renderImage(n);
      break;
    case Kind::Icon:
      e = ui::text(glyphFor(propsOf<n8v_icon_options>(*n.node)->name) ?: "\u25ab");
      break;
    case Kind::Document:
      e = renderDocument(n);
      break;
    case Kind::SidebarSection:
      e = ui::text(propsOf<n8v_sidebar_section_options>(*n.node)->title ? propsOf<n8v_sidebar_section_options>(*n.node)->title : "") | ui::bold | ui::dim;
      break;
    case Kind::SidebarDivider:
    case Kind::Divider:
      e = ui::separator();
      break;
    case Kind::Video:
      e = ui::text("▶ video: " + std::string(propsOf<n8v_video_options>(*n.node)->source ? propsOf<n8v_video_options>(*n.node)->source : "")) | ui::dim;
      break;
    case Kind::Audio:
      e = ui::text("♪ audio: " + std::string(propsOf<n8v_audio_options>(*n.node)->source ? propsOf<n8v_audio_options>(*n.node)->source : "")) | ui::dim;
      break;
    case Kind::Root:
      e = ui::emptyElement();
      break;
    }
    switch (n.kind()) {
    case Kind::Floating:
      return e;
    case Kind::Button:
    case Kind::Text:
    case Kind::Checkbox:
    case Kind::Toggle:
    case Kind::Radio:
    case Kind::Dropdown:
    case Kind::Slider:
    case Kind::Image:
    case Kind::Icon:
      return std::make_shared<TightBoxNode>(std::move(e), n.box);
    default:
      return e | ui::reflect(n.box);
    }
  }

  static bool hasGrow(const n8v_sizing &s) { return s.mode == N8V_SIZING_GROW || s.mode == N8V_SIZING_PERCENT; }

  ui::Element sized(ui::Element e, const n8v_sizing &s, ui::WidthOrHeight axis) {
    const bool horizontal = axis == ui::WIDTH;
    auto cells = [&](float px) { return horizontal ? cellsX(px) : cellsY(px); };
    switch (s.mode) {
    case N8V_SIZING_FIXED:
      return std::move(e) | ui::size(axis, ui::EQUAL, cells(s.value));
    case N8V_SIZING_PERCENT:
      warnOnce("percent sizing is treated as grow");
      [[fallthrough]];
    case N8V_SIZING_GROW:
      e = horizontal ? ui::xflex_grow(std::move(e)) : ui::yflex_grow(std::move(e));
      break;
    case N8V_SIZING_FIT:
      break;
    }
    if (s.min > 0.0f) e = std::move(e) | ui::size(axis, ui::GREATER_THAN, cells(s.min));
    if (s.max > 0.0f) e = std::move(e) | ui::size(axis, ui::LESS_THAN, cells(s.max));
    return e;
  }

  void warnOnce(const char *what) {
    if (!warned_.insert(what).second) return;
    std::fprintf(stderr, "[n8v] ftxui: %s\n", what);
  }

  static ui::Element padded(ui::Element e, const n8v_padding &p) {
    const int l = paddingX(p.left), r = paddingX(p.right), t = paddingY(p.top), b = paddingY(p.bottom);
    if (!l && !r && !t && !b) return e;
    return std::make_shared<PaddingNode>(std::move(e), l, r, t, b);
  }

  static const n8v_sizing *sizingOf(const NativeNode &n, bool horizontal) {
    const auto &props = n.node->current.props;
    if (const auto *o = std::get_if<n8v_flex_options>(&props)) return horizontal ? &o->width : &o->height;
    if (const auto *o = std::get_if<n8v_panel_options>(&props)) return horizontal ? &o->width : &o->height;
    if (const auto *o = std::get_if<n8v_image_options>(&props)) return horizontal ? &o->width : &o->height;
    if (const auto *o = std::get_if<n8v_document_options>(&props)) return horizontal ? &o->width : &o->height;
    return nullptr;
  }

  ui::Element renderKidsBox(NativeNode &n, n8v_direction direction, uint16_t gap = 0, n8v_align hAlign = N8V_ALIGN_START, n8v_align vAlign = N8V_ALIGN_START) {
    const bool horizontal = direction == N8V_DIRECTION_HORIZONTAL;
    const n8v_align mainAlign = horizontal ? hAlign : vAlign;
    const n8v_align crossAlign = horizontal ? vAlign : hAlign;
    const int gapCells = horizontal ? paddingX(gap) : paddingY(gap);

    bool mainGrows = false;
    ui::Elements items;
    for (NativeNode *kid : n.kids) {
      const n8v_sizing *mainSize = sizingOf(*kid, horizontal);
      const n8v_sizing *crossSize = sizingOf(*kid, !horizontal);
      if (mainSize && hasGrow(*mainSize)) mainGrows = true;
      ui::Element e = render(*kid);
      if (crossAlign != N8V_ALIGN_START && !(crossSize && hasGrow(*crossSize))) {
        e = std::make_shared<CrossAlignNode>(std::move(e), horizontal, crossAlign == N8V_ALIGN_CENTER);
      }
      if (gapCells > 0 && !items.empty()) items.push_back(ui::emptyElement() | ui::size(horizontal ? ui::WIDTH : ui::HEIGHT, ui::EQUAL, gapCells));
      items.push_back(std::move(e));
    }
    if (!mainGrows && mainAlign != N8V_ALIGN_START) {
      items.insert(items.begin(), ui::filler());
      if (mainAlign == N8V_ALIGN_CENTER) items.push_back(ui::filler());
    }
    return horizontal ? ui::hbox(std::move(items)) : ui::vbox(std::move(items));
  }

  ui::Element scrollable(ui::Element e, NativeNode &n, bool vertical, bool horizontal) {
    if (!vertical && !horizontal) return e;
    return std::make_shared<ScrollNode>(std::move(e), n, vertical, horizontal);
  }

  void registerScroller(NativeNode &n) {
    n.scrollFocused = focused_ == &n;
    if (!n.hasView) return;
    const bool overflows = n.contentH > n.view.y_max - n.view.y_min + 1 || n.contentW > n.view.x_max - n.view.x_min + 1;
    if (overflows) focusables_.push_back(&n);
  }

  bool scrollKey(NativeNode &n, const ui::Event &event) {
    const int viewH = n.view.y_max - n.view.y_min + 1;
    const int viewW = n.view.x_max - n.view.x_min + 1;
    const int maxY = std::max(0, n.contentH - viewH);
    const int maxX = std::max(0, n.contentW - viewW);
    int y = n.scrollY, x = n.scrollX;
    if (event == ui::Event::ArrowUp) y -= 1;
    else if (event == ui::Event::ArrowDown) y += 1;
    else if (event == ui::Event::PageUp) y -= std::max(1, viewH - 1);
    else if (event == ui::Event::PageDown) y += std::max(1, viewH - 1);
    else if (event == ui::Event::Home) y = 0;
    else if (event == ui::Event::End) y = maxY;
    else if (event == ui::Event::ArrowLeft) x -= 1;
    else if (event == ui::Event::ArrowRight) x += 1;
    else return false;
    y = std::clamp(y, 0, maxY);
    x = std::clamp(x, 0, maxX);
    if (y == n.scrollY && x == n.scrollX) return false;
    n.scrollY = y;
    n.scrollX = x;
    n.stuck = y >= maxY;
    return true;
  }

  ui::Element renderFlex(NativeNode &n) {
    const auto &o = *propsOf<n8v_flex_options>(*n.node);
    if (o.clip_vertical || o.clip_horizontal) registerScroller(n);
    ui::Element e = padded(renderKidsBox(n, o.direction, o.gap, o.h_align, o.v_align), o.padding);
    e = scrollable(std::move(e), n, o.clip_vertical, o.clip_horizontal);
    e = sized(std::move(e), o.width, ui::WIDTH);
    return sized(std::move(e), o.height, ui::HEIGHT);
  }

  ui::Element renderPanel(NativeNode &n) {
    const auto &o = *propsOf<n8v_panel_options>(*n.node);
    if (o.clip_vertical || o.clip_horizontal) registerScroller(n);
    ui::Element e = renderKidsBox(n, o.direction, o.gap, o.h_align, o.v_align);
    if (o.has_padding) e = padded(std::move(e), o.padding);
    else e = padded(std::move(e), {8, 8, 0, 0});
    e = scrollable(std::move(e), n, o.clip_vertical, o.clip_horizontal);
    if (o.role == N8V_PANEL_ROLE_CARD) e = ui::borderRounded(std::move(e));
    const n8v_color accent = resolveThemeColor(o.accent, true);
    if (accent.a > 0.0f) {
      if (hasGrow(o.width)) e = ui::xflex_grow(std::move(e));
      e = ui::hbox({ui::separatorHeavy() | ui::color(toColor(accent)), std::move(e)});
    }
    e = sized(std::move(e), o.width, ui::WIDTH);
    return sized(std::move(e), o.height, ui::HEIGHT);
  }

  static int anchorX(n8v_attach_point p, const ui::Box &b) {
    switch (p) {
    case N8V_ATTACH_POINT_LEFT_TOP:
    case N8V_ATTACH_POINT_LEFT_CENTER:
    case N8V_ATTACH_POINT_LEFT_BOTTOM:
      return b.x_min;
    case N8V_ATTACH_POINT_CENTER_TOP:
    case N8V_ATTACH_POINT_CENTER_CENTER:
    case N8V_ATTACH_POINT_CENTER_BOTTOM:
      return (b.x_min + b.x_max) / 2;
    default:
      return b.x_max + 1;
    }
  }
  static int anchorY(n8v_attach_point p, const ui::Box &b) {
    switch (p) {
    case N8V_ATTACH_POINT_LEFT_TOP:
    case N8V_ATTACH_POINT_CENTER_TOP:
    case N8V_ATTACH_POINT_RIGHT_TOP:
      return b.y_min;
    case N8V_ATTACH_POINT_LEFT_CENTER:
    case N8V_ATTACH_POINT_CENTER_CENTER:
    case N8V_ATTACH_POINT_RIGHT_CENTER:
      return (b.y_min + b.y_max) / 2;
    default:
      return b.y_max + 1;
    }
  }

  ui::Element renderFloating(NativeNode &n) {
    const auto &o = *propsOf<n8v_floating_options>(*n.node);
    floatings_.push_back(&n);
    ui::Element content = renderKidsBox(n, N8V_DIRECTION_VERTICAL);
    const bool ownsBorder = n.kids.size() == 1 && n.kids[0]->kind() == Kind::Panel && propsOf<n8v_panel_options>(*n.kids[0]->node) &&
                            propsOf<n8v_panel_options>(*n.kids[0]->node)->role == N8V_PANEL_ROLE_CARD;
    if (o.opaque && !ownsBorder) content = ui::borderRounded(std::move(content));
    content = sized(std::move(content), o.width, ui::WIDTH);
    content = sized(std::move(content), o.height, ui::HEIGHT);
    NativeNode *parent = n.parent ? n.parent : &rootNode_;
    return std::make_shared<OverlayNode>(std::move(content) | ui::reflect(n.box), overlays_, [&n, parent](int width, int height, const ui::Box &clip) {
      const auto &opts = *propsOf<n8v_floating_options>(*n.node);
      const ui::Dimensions term = ui::Terminal::Size();
      const ui::Box anchor = parent->kind() == Kind::Root ? ui::Box{0, term.dimx - 1, 0, term.dimy - 1} : parent->box;
      if (parent->kind() != Kind::Root) {
        const ui::Box seen = ui::Box::Intersection(anchor, clip);
        if (seen.x_max < seen.x_min || seen.y_max < seen.y_min) return ui::Box{0, -1, 0, -1};
      }
      auto span = [](const n8v_sizing &sizing, int natural, int available, float cellPx) {
        int size = natural;
        if (sizing.mode == N8V_SIZING_PERCENT) size = std::max(natural, (int)std::lround(sizing.value * available));
        else if (sizing.mode == N8V_SIZING_GROW) size = std::max(natural, available);
        else return natural;
        if (sizing.max > 0.0f) size = std::min(size, std::max(natural, (int)std::lround(sizing.max / cellPx)));
        if (sizing.min > 0.0f) size = std::max(size, (int)std::lround(sizing.min / cellPx));
        return size;
      };
      width = span(opts.width, width, anchor.x_max - anchor.x_min + 1, kCellWidthPx);
      height = span(opts.height, height, anchor.y_max - anchor.y_min + 1, kCellHeightPx);
      const ui::Box own{0, std::max(0, width - 1), 0, std::max(0, height - 1)};
      int x = anchorX(opts.parent_attach_point, anchor) - anchorX(opts.element_attach_point, own) + offsetX(opts.offset_x);
      int y = anchorY(opts.parent_attach_point, anchor) - anchorY(opts.element_attach_point, own) + offsetY(opts.offset_y);
      if (opts.keep_on_screen) {
        x = std::clamp(x, 0, std::max(0, term.dimx - width));
        y = std::clamp(y, 0, std::max(0, term.dimy - height));
      }
      return ui::Box{x, x + width - 1, y, y + height - 1};
    });
  }

  ui::Element renderSidebar(NativeNode &n) {
    const auto &o = *propsOf<n8v_sidebar_options>(*n.node);
    const bool collapsed = o.collapsed && *o.collapsed;
    const int selected = o.selected ? *o.selected : 0;

    ui::Elements rows;
    const bool hasTitle = o.title && *o.title && !collapsed && !o.compact;
    if (hasTitle) {
      rows.push_back(ui::emptyElement() | ui::size(ui::HEIGHT, ui::EQUAL, 1));
      rows.push_back(ui::hbox({ui::filler(), ui::text(o.title) | ui::bold | ui::underlined, ui::filler()}));
      rows.push_back(ui::emptyElement() | ui::size(ui::HEIGHT, ui::EQUAL, 1));
    }
    NativeNode *content = nullptr;
    int pageIndex = 0;
    int widest = 0;
    for (NativeNode *kid : n.kids) {
      if (kid->kind() == Kind::Page) {
        const bool isSelected = pageIndex == selected;
        if (isSelected) content = kid;
        rows.push_back(renderPageRow(*kid, isSelected, collapsed));
        const auto &page = *propsOf<n8v_page_options>(*kid->node);
        widest = std::max(widest, ui::string_width(page.name ? page.name : "") + 8);
        ++pageIndex;
      } else {
        rows.push_back(render(*kid));
      }
    }

    if (hasTitle) widest = std::max(widest, ui::string_width(o.title) + 4);
    int width = collapsed ? 5 : widest;
    if (!collapsed && o.width.mode == N8V_SIZING_FIXED) width = cellsX(o.width.value);
    if (!collapsed && o.min_width > 0.0f) width = std::max(width, cellsX(o.min_width));
    if (!collapsed && o.max_width > 0.0f) width = std::min(width, cellsX(o.max_width));
    ui::Element pane = ui::vbox(std::move(rows)) | ui::size(ui::WIDTH, ui::EQUAL, std::max(width, 4));

    ui::Element body = content ? renderKidsBox(*content, N8V_DIRECTION_VERTICAL) : ui::emptyElement();
    return ui::hbox({std::move(pane), ui::separator(), ui::xflex(std::move(body)) | ui::yflex}) | ui::flex;
  }

  ui::Element renderPageRow(NativeNode &page, bool selected, bool collapsed) {
    const auto &o = *propsOf<n8v_page_options>(*page.node);
    focusables_.push_back(&page);
    const bool focused = focused_ == &page;

    const char *glyph = glyphFor(o.icon);
    std::string label = " ";
    if (glyph) label += std::string(glyph) + " ";
    if (!collapsed) label += o.name ? o.name : "";
    else if (!glyph) label += (o.name && *o.name) ? std::string(o.name, 1) : "";

    ui::Elements row{ui::text(label)};
    if (!collapsed) {
      row.push_back(ui::filler());
      if (o.badge && *o.badge) row.push_back(ui::text(std::string(o.badge) + " ") | ui::color(ui::Color::Red));
      else if (o.notify) row.push_back(ui::text("● ") | ui::color(ui::Color::Red));
    }
    ui::Element e = ui::hbox(std::move(row));
    if (selected) e = std::move(e) | ui::bold | ui::inverted;
    if (focused) e = std::move(e) | ui::underlined | ui::bold;
    return e | ui::reflect(page.box);
  }

  bool checkedOf(const NativeNode &n) const {
    if (const auto *o = propsOf<n8v_checkbox_options>(*n.node)) return o->checked && *o->checked;
    if (const auto *o = propsOf<n8v_toggle_options>(*n.node)) return o->checked && *o->checked;
    return false;
  }
  bool radioSelected(const NativeNode &n) const {
    const auto *o = propsOf<n8v_radio_options>(*n.node);
    return o && o->selected && *o->selected == o->value;
  }

  // Only attributes, never fixed colours, so contrast holds under whatever palette the terminal uses.
  ui::Element focusStyle(ui::Element e, NativeNode &n, bool alreadyInverted = false) {
    focusables_.push_back(&n);
    if (focused_ == &n) e = std::move(e) | (alreadyInverted ? ui::Decorator(ui::underlined) : ui::Decorator(ui::inverted));
    else if (n.hovered) e = std::move(e) | ui::underlined;
    return e;
  }

  ui::Element renderChoice(NativeNode &n, const char *indicator) {
    ui::Element label = ui::text(indicator + std::string(n.node->current.label));
    if (n.kind() == Kind::Toggle && checkedOf(n)) label = std::move(label) | ui::color(ui::Color::Green);
    return focusStyle(std::move(label), n);
  }

  ui::Element renderButton(NativeNode &n) {
    const auto &o = *propsOf<n8v_button_options>(*n.node);
    const char *glyph = glyphFor(o.icon);
    const std::string_view label = n.node->current.label;
    const std::string gap = glyph && !label.empty() ? " " : "";
    std::string text = " ";
    if (glyph && o.icon_position == N8V_ICON_POSITION_LEADING) text += glyph + gap;
    text += label;
    if (glyph && o.icon_position == N8V_ICON_POSITION_TRAILING) text += gap + glyph;
    text += " ";

    const bool primary = o.style == N8V_BUTTON_STYLE_PRIMARY;
    if (o.style == N8V_BUTTON_STYLE_SECONDARY) text = "[" + text + "]";
    ui::Element e = ui::text(std::move(text));
    const bool pressed = o.checked && *o.checked;
    if (primary) e = std::move(e) | ui::bold | ui::inverted;
    if (pressed) e = std::move(e) | (primary ? ui::Decorator(ui::underlined) : ui::Decorator(ui::inverted));
    return focusStyle(std::move(e), n, primary);
  }

  ui::Element styledText(std::string_view text, bool bold, bool italic, bool strike, bool dim, const n8v_color *color) {
    ui::Element e = text.find(' ') != std::string_view::npos && text.size() > 48 ? ui::paragraph(text) : ui::text(std::string(text));
    if (bold) e = std::move(e) | ui::bold;
    if (italic) e = std::move(e) | ui::italic;
    if (strike) e = std::move(e) | ui::strikethrough;
    if (dim) e = std::move(e) | ui::dim;
    if (color)
      if (const auto explicitTextColor = explicitColor(*color)) e = std::move(e) | ui::color(*explicitTextColor);
    return e;
  }

  ui::Element renderText(NativeNode &n) {
    const auto &o = *propsOf<n8v_text_options>(*n.node);
    ui::Elements lines;
    for (const std::string &line : splitLines(n.node->current.label))
      lines.push_back(styledText(line, o.bold || o.large, o.italic, o.strikethrough, o.darken || o.small, &o.color));
    ui::Element e = lines.size() == 1 ? std::move(lines[0]) : ui::vbox(std::move(lines));

    const bool clickable = o.on_click || (o.url && *o.url);
    if (!clickable) return e;
    if (!(o.on_click && o.click_unstyled)) e = std::move(e) | ui::underlined | ui::color(ui::Color::Blue);
    if (o.url && *o.url && !o.on_click) e = ui::hyperlink(o.url, std::move(e));
    return focusStyle(std::move(e), n);
  }

  ui::Element renderSlider(NativeNode &n) {
    const auto &o = *propsOf<n8v_slider_options>(*n.node);
    const float range = o.max - o.min;
    const float value = o.value ? *o.value : o.min;
    const float progress = range > 0.0f ? std::clamp((value - o.min) / range, 0.0f, 1.0f) : 0.0f;
    ui::Element bar = ui::gauge(progress) | ui::xflex | ui::size(ui::WIDTH, ui::GREATER_THAN, 12);
    if (focused_ == &n) bar = std::move(bar) | ui::color(ui::Color::Blue);
    focusables_.push_back(&n);
    return ui::hbox({ui::text("["), std::move(bar), ui::text("]")});
  }

  ui::Element renderDropdown(NativeNode &n) {
    const auto &o = *propsOf<n8v_dropdown_options>(*n.node);
    const int selected = o.selected ? *o.selected : -1;
    const bool hasSelection = selected >= 0 && (size_t)selected < o.item_count;
    std::string current = hasSelection && o.items[selected] ? o.items[selected] : (o.placeholder ? o.placeholder : "");

    ui::Element header = ui::text("[ " + current + " " + (n.open ? "▴" : "▾") + " ]");
    if (!hasSelection) header = std::move(header) | ui::dim;
    header = focusStyle(std::move(header), n);
    if (!n.open) return header;

    n.itemBoxes.assign(o.item_count, ui::Box{});
    ui::Elements col{std::move(header)};
    for (size_t i = 0; i < o.item_count; ++i) {
      ui::Element row = ui::text("  " + std::string(o.items[i] ? o.items[i] : "") + " ") | ui::reflect(n.itemBoxes[i]);
      if ((int)i == n.highlighted) row = std::move(row) | ui::inverted;
      else if ((int)i == selected) row = std::move(row) | ui::bold;
      col.push_back(std::move(row));
    }
    return ui::vbox(std::move(col));
  }

  const char *glyphFor(const char *name) const {
    if (!name) return nullptr;
    if (iconStyle_ == IconStyle::Nerd)
      if (const char *glyph = nerdIconGlyph(name)) return glyph;
    return unicodeIconGlyph(name);
  }

  ui::Element renderImage(NativeNode &n) {
    const auto &o = *propsOf<n8v_image_options>(*n.node);
    const DecodedImage *image = getOrDecodeImage(o);
    if (!image || image->width <= 0 || image->height <= 0) return ui::text("[image]") | ui::dim;

    int cols = o.width.mode == N8V_SIZING_FIXED ? cellsX(o.width.value) : 0;
    int rows = o.height.mode == N8V_SIZING_FIXED ? cellsY(o.height.value) : 0;
    if (!cols && !rows) cols = std::clamp(image->width / (int)kCellWidthPx, 1, 40);
    if (!cols) cols = std::max(1, rows * 2 * image->width / image->height);
    if (!rows) rows = std::max(1, (cols * image->height / image->width + 1) / 2);

    const uint32_t kittyId = imageProtocol_ != ImageProtocol::Blocks ? kitty_.ensure(image, cols, rows) : 0;
    ui::Element e = std::make_shared<ImageNode>(image, cols, rows, kittyId, imageProtocol_ == ImageProtocol::KittyDirect ? &kitty_ : nullptr);
    if (o.on_click || o.on_click_at) e = focusStyle(std::move(e), n);
    return e;
  }

  std::string entryText(const NativeNode &n) const {
    const auto &o = *propsOf<n8v_entry_options>(*n.node);
    if (o.value) return o.value->data ? std::string(o.value->data, o.value->length) : std::string();
    return n.localText;
  }

  void commitEntryText(NativeNode &n, const std::string &text) {
    dispatch::setText(*n.node, text);
    if (!propsOf<n8v_entry_options>(*n.node)->value) n.localText = text;
  }

  ui::Element renderEntryLine(const std::string &line, bool focused, int cursorColumnByte, bool password, int width, int *hscroll) {
    std::vector<std::string> glyphs = ui::Utf8ToGlyphs(line);
    if (password)
      for (std::string &g : glyphs) g = "•";
    const int cursorGlyph = focused && cursorColumnByte >= 0 ? (int)glyphIndexAt(glyphs, (size_t)cursorColumnByte) : -1;

    int first = hscroll ? *hscroll : 0;
    if (hscroll) {
      const int window = std::max(1, width - 1);
      if (cursorGlyph >= 0) {
        if (cursorGlyph < first) first = cursorGlyph;
        if (cursorGlyph >= first + window) first = cursorGlyph - window + 1;
      }
      first = std::clamp(first, 0, std::max(0, (int)glyphs.size() - window));
      *hscroll = first;
    }

    auto join = [&](int from, int to) {
      std::string s;
      for (int i = std::max(from, 0); i < std::min(to, (int)glyphs.size()); ++i) s += glyphs[i];
      return s;
    };
    const int end = hscroll ? first + std::max(1, width - 1) + 1 : (int)glyphs.size() + 1;
    if (cursorGlyph < 0) return ui::text(join(first, end));
    const std::string at = cursorGlyph < (int)glyphs.size() ? glyphs[cursorGlyph] : " ";
    return ui::hbox({ui::text(join(first, cursorGlyph)), ui::focusCursorBar(ui::text(at)), ui::text(join(cursorGlyph + 1, end))});
  }

  ui::Element renderEntry(NativeNode &n) {
    const auto &o = *propsOf<n8v_entry_options>(*n.node);
    focusables_.push_back(&n);
    const bool focused = focused_ == &n;
    const std::string text = entryText(n);
    n.cursor = std::min(n.cursor, text.size());
    const int innerWidth = n.box.x_max > n.box.x_min ? n.box.x_max - n.box.x_min - 1 : kDefaultEntryColumns;

    ui::Elements lines;
    if (!o.multiline) {
      if (text.empty() && o.placeholder && *o.placeholder) {
        lines.push_back(focused ? ui::hbox({ui::focusCursorBar(ui::text(" ")), ui::text(o.placeholder) | ui::dim}) : ui::text(o.placeholder) | ui::dim);
      } else {
        lines.push_back(renderEntryLine(text, focused, (int)n.cursor, o.password, innerWidth, &n.hscroll));
      }
    } else {
      const EntryLines shape = entryLines(o);
      const std::vector<std::string> all = splitLines(text);
      size_t cursorLine = 0, lineStart = 0;
      for (size_t i = 0; i < all.size(); ++i) {
        if (n.cursor >= lineStart && n.cursor <= lineStart + all[i].size()) cursorLine = i;
        lineStart += all[i].size() + 1;
      }
      const int visible = std::clamp((int)all.size(), shape.min, shape.max);
      if ((int)cursorLine < n.vscroll) n.vscroll = (int)cursorLine;
      if ((int)cursorLine >= n.vscroll + visible) n.vscroll = (int)cursorLine - visible + 1;
      n.vscroll = std::clamp(n.vscroll, 0, std::max(0, (int)all.size() - visible));

      size_t offset = 0;
      for (int i = 0; i < n.vscroll; ++i) offset += all[i].size() + 1;
      for (int i = n.vscroll; i < n.vscroll + visible; ++i) {
        if (i >= (int)all.size()) {
          lines.push_back(ui::text(" "));
          continue;
        }
        const bool here = i == (int)cursorLine;
        if (text.empty() && o.placeholder && *o.placeholder)
          lines.push_back(focused ? ui::hbox({ui::focusCursorBar(ui::text(" ")), ui::text(o.placeholder) | ui::dim}) : ui::text(o.placeholder) | ui::dim);
        else lines.push_back(renderEntryLine(all[i], focused && here, here ? (int)(n.cursor - offset) : -1, o.password, innerWidth, nullptr));
        offset += all[i].size() + 1;
      }
    }

    ui::Element e = ui::hbox({ui::text(" "), ui::vbox(std::move(lines)) | ui::xflex, ui::text(" ")}) | ui::underlined;
    if (focused) e = std::move(e) | ui::bold;
    return std::move(e) | ui::size(ui::WIDTH, ui::GREATER_THAN, kDefaultEntryColumns) | ui::xflex;
  }

  ui::Element renderInlines(const std::vector<markdown::InlineRun> &runs, bool bold = false) {
    ui::Elements words;
    for (const markdown::InlineRun &run : runs) {
      if (!run.imageUrl.empty()) {
        words.push_back(ui::text("[image] ") | ui::dim);
        continue;
      }
      size_t pos = 0;
      while (pos < run.text.size()) {
        size_t end = run.text.find(' ', pos);
        end = end == std::string::npos ? run.text.size() : end + 1;
        ui::Element w = ui::text(run.text.substr(pos, end - pos));
        if (bold || run.bold) w = std::move(w) | ui::bold;
        if (run.italic) w = std::move(w) | ui::italic;
        if (run.strikethrough) w = std::move(w) | ui::strikethrough;
        if (run.code) w = std::move(w) | ui::inverted;
        if (!run.linkUrl.empty()) w = ui::hyperlink(run.linkUrl, std::move(w) | ui::underlined | ui::color(ui::Color::Blue));
        words.push_back(std::move(w));
        pos = end;
      }
    }
    return ui::flexbox(std::move(words), ui::FlexboxConfig().Set(ui::FlexboxConfig::Direction::Row).Set(ui::FlexboxConfig::Wrap::Wrap));
  }

  ui::Element renderBlocks(const markdown::Document &doc) {
    ui::Elements out;
    for (const markdown::Block &block : doc) {
      switch (block.kind) {
      case markdown::BlockKind::Heading: {
        ui::Element h = renderInlines(block.inlines, true);
        if (block.level <= 1) h = std::move(h) | ui::underlined;
        out.push_back(std::move(h));
        break;
      }
      case markdown::BlockKind::Paragraph:
        out.push_back(renderInlines(block.inlines));
        break;
      case markdown::BlockKind::CodeBlock: {
        ui::Elements code;
        for (const std::string &line :
             splitLines(block.codeText.size() && block.codeText.back() == '\n' ? block.codeText.substr(0, block.codeText.size() - 1) : block.codeText))
          code.push_back(ui::text(line));
        out.push_back(ui::vbox(std::move(code)) | ui::border | ui::dim);
        break;
      }
      case markdown::BlockKind::BulletList:
      case markdown::BlockKind::OrderedList: {
        ui::Elements items;
        int number = block.orderedStart;
        for (const auto &item : block.listItems) {
          const std::string marker = block.kind == markdown::BlockKind::OrderedList ? std::to_string(number++) + ". " : "• ";
          items.push_back(ui::hbox({ui::text(marker), renderInlines(item) | ui::xflex}));
        }
        out.push_back(ui::vbox(std::move(items)));
        break;
      }
      case markdown::BlockKind::BlockQuote:
        out.push_back(ui::hbox({ui::separatorLight(), ui::text(" "), renderBlocks(block.children) | ui::xflex}) | ui::dim);
        break;
      case markdown::BlockKind::ThematicBreak:
        out.push_back(ui::separator());
        break;
      }
      out.push_back(ui::emptyElement() | ui::size(ui::HEIGHT, ui::EQUAL, 1));
    }
    if (!out.empty()) out.pop_back();
    return ui::vbox(std::move(out));
  }

  ui::Element renderDocument(NativeNode &n) {
    const auto &o = *propsOf<n8v_document_options>(*n.node);
    const markdown::CachedDocument cached = markdown::getOrParse(o.markdown ? o.markdown : "");
    ui::Element e = cached.document ? renderBlocks(*cached.document) : ui::emptyElement();
    e = std::move(e) | ui::xflex;
    e = sized(std::move(e), o.width, ui::WIDTH);
    return sized(std::move(e), o.height, ui::HEIGHT);
  }

  ui::Element renderDialog() {
    DialogState &d = *dialog_;
    d.buttonBoxes.assign(d.request.buttons.size(), ui::Box{});
    ui::Elements buttons;
    for (size_t i = 0; i < d.request.buttons.size(); ++i) {
      ui::Element b = ui::text("[ " + d.request.buttons[i].label + " ]") | ui::reflect(d.buttonBoxes[i]);
      if (d.request.buttons[i].destructive) b = std::move(b) | ui::color(ui::Color::Red);
      if (d.selected == (int)i) b = std::move(b) | ui::inverted | ui::bold;
      if (!buttons.empty()) buttons.push_back(ui::text(" "));
      buttons.push_back(std::move(b));
    }
    ui::Element body =
      ui::vbox({ui::paragraph(d.request.message), ui::emptyElement() | ui::size(ui::HEIGHT, ui::EQUAL, 1), ui::hbox({ui::filler(), ui::hbox(std::move(buttons))})});
    ui::Element box = ui::window(ui::text(" " + d.request.title + " ") | ui::bold, std::move(body)) | ui::size(ui::WIDTH, ui::LESS_THAN, 60) |
                      ui::size(ui::WIDTH, ui::GREATER_THAN, 30) | ui::clear_under;
    return ui::vbox({ui::filler(), ui::hbox({ui::filler(), std::move(box), ui::filler()}), ui::filler()});
  }

  void afterRender() {
    resolveFocusVisibility();
    resolvePendingScrolls();
  }

  void revealInScrollers(NativeNode &target, int padding = 0) {
    for (NativeNode *scroller = target.parent; scroller; scroller = scroller->parent) {
      if (!scroller->hasView) continue;
      const ui::Box &view = scroller->view;
      if (target.box.y_min < view.y_min + padding) scroller->scrollY -= view.y_min + padding - target.box.y_min;
      else if (target.box.y_max > view.y_max - padding) scroller->scrollY += target.box.y_max - (view.y_max - padding);
      if (target.box.x_min < view.x_min) scroller->scrollX -= view.x_min - target.box.x_min;
      else if (target.box.x_max > view.x_max) scroller->scrollX += target.box.x_max - view.x_max;
      scroller->scrollY = std::max(0, scroller->scrollY);
      scroller->scrollX = std::max(0, scroller->scrollX);
      scroller->stuck = false;
    }
  }

  void resolveFocusVisibility() {
    if (!focusMoved_ || !focused_) return;
    focusMoved_ = false;
    revealInScrollers(*focused_);
  }

  void resolvePendingScrolls() {
    for (auto it = pendingScrolls_.begin(); it != pendingScrolls_.end();) {
      auto container = byId_.find(it->request.containerId);
      bool done = false;
      if (container != byId_.end() && container->second->hasView) {
        NativeNode &c = *container->second;
        const int viewH = c.view.y_max - c.view.y_min + 1;
        const int viewW = c.view.x_max - c.view.x_min + 1;
        switch (it->request.kind) {
        case NativeScrollRequest::Kind::Bottom:
          c.scrollY = std::max(0, c.contentH - viewH);
          c.stuck = true;
          done = true;
          break;
        case NativeScrollRequest::Kind::Top:
          c.scrollY = 0;
          c.stuck = false;
          done = true;
          break;
        case NativeScrollRequest::Kind::Left:
          c.scrollX = 0;
          done = true;
          break;
        case NativeScrollRequest::Kind::Right:
          c.scrollX = std::max(0, c.contentW - viewW);
          done = true;
          break;
        case NativeScrollRequest::Kind::Offset:
          c.scrollX = std::max(0, cellsX(it->request.x));
          c.scrollY = std::max(0, cellsY(it->request.y));
          c.stuck = false;
          done = true;
          break;
        case NativeScrollRequest::Kind::Element: {
          auto element = byId_.find(it->request.elementId);
          if (element != byId_.end()) {
            revealInScrollers(*element->second, cellsY(it->request.paddingTop));
            done = true;
          }
          break;
        }
        }
      }
      if (done || --it->framesLeft <= 0) it = pendingScrolls_.erase(it);
      else ++it;
    }
  }

  void focus(NativeNode *n) {
    if (focused_ == n) return;
    if (focused_ && focused_->kind() == Kind::Dropdown) focused_->open = false;
    focused_ = n;
    focusMoved_ = true;
    if (n && n->kind() == Kind::Entry) n->cursor = entryText(*n).size();
  }

  const NativeNode *topDismissable() const {
    for (auto it = floatings_.rbegin(); it != floatings_.rend(); ++it) {
      const auto *opts = propsOf<n8v_floating_options>(*(*it)->node);
      if (opts && opts->on_dismiss) return *it;
    }
    return nullptr;
  }

  void moveFocus(int direction) {
    std::vector<NativeNode *> candidates = focusables_;
    if (const NativeNode *overlay = topDismissable()) {
      std::erase_if(candidates, [&](NativeNode *n) {
        for (const NativeNode *up = n; up; up = up->parent) {
          if (up == overlay) return false;
        }
        return true;
      });
    }
    if (candidates.empty()) return;
    auto it = std::find(candidates.begin(), candidates.end(), focused_);
    size_t index = it == candidates.end() ? (direction > 0 ? 0 : candidates.size() - 1) : (size_t)(it - candidates.begin());
    if (it != candidates.end()) index = (index + candidates.size() + (size_t)direction) % candidates.size();
    focus(candidates[index]);
  }

  void activate(NativeNode &n) {
    switch (n.kind()) {
    case Kind::Button:
    case Kind::Text:
    case Kind::Image:
      dispatch::activate(*n.node);
      break;
    case Kind::Checkbox:
    case Kind::Toggle:
      dispatch::setChecked(*n.node, !checkedOf(n));
      break;
    case Kind::Radio:
      dispatch::select(*n.node, 0);
      break;
    case Kind::Page:
      dispatch::selectPage(*n.node);
      break;
    case Kind::Dropdown: {
      const auto &o = *propsOf<n8v_dropdown_options>(*n.node);
      n.open = !n.open;
      n.highlighted = o.selected && *o.selected >= 0 ? *o.selected : 0;
      break;
    }
    default:
      break;
    }
  }

  void setSliderFromX(NativeNode &n, int x) {
    const auto &o = *propsOf<n8v_slider_options>(*n.node);
    const int left = n.box.x_min + 1;
    const int width = std::max(1, n.box.x_max - n.box.x_min - 2);
    const float t = std::clamp((float)(x - left) / (float)std::max(1, width - 1), 0.0f, 1.0f);
    dispatch::setSlider(*n.node, o.min + t * (o.max - o.min));
  }

  void nudgeSlider(NativeNode &n, int direction) {
    const auto &o = *propsOf<n8v_slider_options>(*n.node);
    const float step = (o.max - o.min) / 20.0f;
    const float value = (o.value ? *o.value : o.min) + step * (float)direction;
    dispatch::setSlider(*n.node, std::clamp(value, o.min, o.max));
  }

  bool onEvent(ui::Event event) {
    busyTicks_ = kBusyTicks;
    if (dialog_) return onDialogEvent(event);
    if (event.is_mouse()) return onMouse(event.mouse());
    return onKey(event);
  }

  bool onDialogEvent(ui::Event &event) {
    DialogState &d = *dialog_;
    const int count = (int)d.request.buttons.size();
    auto finish = [&](int button) {
      MessageDialogDone done = std::move(d.done);
      dialog_.reset();
      if (done) done(button);
    };
    if (event == ui::Event::ArrowRight || event == ui::Event::Tab) d.selected = count ? (d.selected + 1) % count : 0;
    else if (event == ui::Event::ArrowLeft || event == ui::Event::TabReverse) d.selected = count ? (d.selected + count - 1) % count : 0;
    else if (event == ui::Event::Return) finish(d.selected);
    else if (event == ui::Event::Escape) finish(-1);
    else if (event.is_mouse()) {
      const ui::Mouse &m = event.mouse();
      if (m.button == ui::Mouse::Left && m.motion == ui::Mouse::Pressed) {
        for (int i = 0; i < count; ++i)
          if (inside(d.buttonBoxes[i], m.x, m.y)) return finish(i), true;
      }
    }
    return true;
  }

  bool onKey(const ui::Event &event) {
    if (event == ui::Event::CtrlC) return app_->Exit(), true;
    if (event == ui::Event::Escape) {
      if (focused_ && focused_->kind() == Kind::Dropdown && focused_->open) return focused_->open = false, true;
      if (!floatings_.empty()) {
        dispatch::dismiss(*floatings_.back()->node);
        return true;
      }
      if (focused_ && focused_->kind() == Kind::Entry && dispatch::key(*focused_->node, N8V_KEY_ESCAPE, 0)) return true;
      escapePressed_ = true;
      return true;
    }
    if (event == ui::Event::Tab) return moveFocus(1), true;
    if (event == ui::Event::TabReverse) return moveFocus(-1), true;
    if (!focused_) return false;
    NativeNode &n = *focused_;

    if (n.hasView && (n.kind() == Kind::Flex || n.kind() == Kind::Panel) && scrollKey(n, event)) return true;
    if (n.kind() == Kind::Entry) return onEntryKey(n, event);
    if (n.kind() == Kind::Dropdown && n.open) {
      const auto &o = *propsOf<n8v_dropdown_options>(*n.node);
      const int count = (int)o.item_count;
      if (event == ui::Event::ArrowDown && count) n.highlighted = (n.highlighted + 1) % count;
      else if (event == ui::Event::ArrowUp && count) n.highlighted = (n.highlighted + count - 1) % count;
      else if (event == ui::Event::Return || event == ui::Event::Character(' ')) {
        if (count) dispatch::select(*n.node, n.highlighted);
        n.open = false;
      } else return false;
      return true;
    }
    if (n.kind() == Kind::Slider) {
      if (event == ui::Event::ArrowRight) return nudgeSlider(n, 1), true;
      if (event == ui::Event::ArrowLeft) return nudgeSlider(n, -1), true;
    }
    if (event == ui::Event::Return || event == ui::Event::Character(' ')) return activate(n), true;
    if (event == ui::Event::ArrowDown || event == ui::Event::ArrowRight) return moveFocus(1), true;
    if (event == ui::Event::ArrowUp || event == ui::Event::ArrowLeft) return moveFocus(-1), true;
    return false;
  }

  bool onEntryKey(NativeNode &n, const ui::Event &event) {
    const auto &o = *propsOf<n8v_entry_options>(*n.node);
    std::string text = entryText(n);
    size_t cursor = std::min(n.cursor, text.size());
    auto edited = [&](size_t newCursor) {
      n.cursor = newCursor;
      commitEntryText(n, text);
    };

    auto lineBounds = [&](size_t at) {
      const size_t begin = at == 0 ? 0 : text.rfind('\n', at - 1) == std::string::npos ? 0 : text.rfind('\n', at - 1) + 1;
      size_t end = text.find('\n', at);
      if (end == std::string::npos) end = text.size();
      return std::pair<size_t, size_t>{begin, end};
    };
    auto columnOf = [&](size_t at, size_t begin) {
      size_t col = 0;
      for (size_t i = begin; i < at; i = nextCodepointStart(text, i)) ++col;
      return col;
    };
    auto moveVertically = [&](int direction) {
      if (!o.multiline) return false;
      const auto [begin, end] = lineBounds(cursor);
      const size_t col = columnOf(cursor, begin);
      size_t target;
      if (direction < 0) {
        if (begin == 0) return false;
        target = begin - 1;
      } else {
        if (end >= text.size()) return false;
        target = end + 1;
      }
      const auto [tb, te] = lineBounds(direction < 0 ? target : target);
      size_t pos = tb;
      for (size_t c = 0; c < col && pos < te; ++c) pos = nextCodepointStart(text, pos);
      n.cursor = pos;
      return true;
    };

    const bool upOrDown = event == ui::Event::ArrowUp || event == ui::Event::ArrowDown;
    if (upOrDown) {
      if (dispatch::key(*n.node, event == ui::Event::ArrowUp ? N8V_KEY_UP : N8V_KEY_DOWN, 0)) return true;
      if (moveVertically(event == ui::Event::ArrowUp ? -1 : 1)) return true;
      moveFocus(event == ui::Event::ArrowUp ? -1 : 1);
      return true;
    }
    if (event == ui::Event::ArrowLeft) return n.cursor = prevCodepointStart(text, cursor), true;
    if (event == ui::Event::ArrowRight) return n.cursor = nextCodepointStart(text, cursor), true;
    if (event == ui::Event::ArrowLeftCtrl) return n.cursor = wordLeft(text, cursor), true;
    if (event == ui::Event::ArrowRightCtrl) return n.cursor = wordRight(text, cursor), true;
    if (event == ui::Event::Home || event == ui::Event::CtrlA) return n.cursor = lineBounds(cursor).first, true;
    if (event == ui::Event::End || event == ui::Event::CtrlE) return n.cursor = lineBounds(cursor).second, true;
    if (event == ui::Event::Backspace) {
      if (cursor == 0) return true;
      const size_t from = prevCodepointStart(text, cursor);
      text.erase(from, cursor - from);
      return edited(from), true;
    }
    if (event == ui::Event::Delete) {
      if (cursor >= text.size()) return true;
      text.erase(cursor, nextCodepointStart(text, cursor) - cursor);
      return edited(cursor), true;
    }
    if (event == ui::Event::CtrlU) {
      const size_t begin = lineBounds(cursor).first;
      text.erase(begin, cursor - begin);
      return edited(begin), true;
    }

    const bool plainEnter = event == ui::Event::Return;
    const bool modifiedEnter = event == ui::Event::CtrlJ || event.input() == "\x1b\r";
    if (plainEnter || modifiedEnter) {
      const bool submits = !o.multiline || (o.submit_mode == N8V_SUBMIT_ON_ENTER ? plainEnter : modifiedEnter);
      if (submits) return dispatch::submit(*n.node), true;
      text.insert(cursor, "\n");
      return edited(cursor + 1), true;
    }

    if (event.is_character()) {
      const std::string typed = event.character();
      if (typed.empty() || (unsigned char)typed[0] < 0x20) return false;
      text.insert(cursor, typed);
      return edited(cursor + typed.size()), true;
    }
    return false;
  }

  bool hitPath(NativeNode &n, int x, int y, std::vector<NativeNode *> &path) {
    if (n.hasView && !inside(n.view, x, y)) return false;
    if (n.kind() == Kind::Page) {
      if (!isSelectedPage(n)) return inside(n.box, x, y) ? (path.push_back(&n), true) : false;
    }
    for (auto it = n.kids.rbegin(); it != n.kids.rend(); ++it) {
      if (hitPath(**it, x, y, path)) {
        path.push_back(&n);
        return true;
      }
    }
    if (n.kind() != Kind::Root && inside(n.box, x, y)) return path.push_back(&n), true;
    return false;
  }

  std::vector<NativeNode *> pathAt(int x, int y) {
    std::vector<NativeNode *> path;
    for (auto it = floatings_.rbegin(); it != floatings_.rend(); ++it) {
      if (!hitPath(**it, x, y, path)) continue;
      for (NativeNode *ancestor = (*it)->parent; ancestor; ancestor = ancestor->parent) path.push_back(ancestor);
      return path;
    }
    hitPath(rootNode_, x, y, path);
    return path;
  }

  bool isSelectedPage(const NativeNode &page) const {
    if (!page.parent || page.parent->kind() != Kind::Sidebar) return true;
    const auto &sidebar = *propsOf<n8v_sidebar_options>(*page.parent->node);
    int index = 0;
    for (const NativeNode *kid : page.parent->kids) {
      if (kid == &page) break;
      if (kid->kind() == Kind::Page) ++index;
    }
    return index == (sidebar.selected ? *sidebar.selected : 0);
  }

  void updateHover(const std::vector<NativeNode *> &path) {
    std::unordered_map<NativeNode *, bool> now;
    for (NativeNode *p : path)
      if (p->kind() != Kind::Root) now[p] = true;
    auto set = [&](NativeNode *p, bool hovered) {
      if (p->hovered == hovered) return;
      p->hovered = hovered;
      if (!p->registeredId.empty()) {
        if (hovered) hoveredIds_.insert(p->registeredId);
        else hoveredIds_.erase(p->registeredId);
      }
      dispatch::hover(*p->node, hovered);
    };
    for (NativeNode *p : lastHoverPath_)
      if (!now.count(p)) set(p, false);
    lastHoverPath_.clear();
    for (const auto &entry : now) {
      set(entry.first, true);
      lastHoverPath_.push_back(entry.first);
    }
  }

  bool onMouse(ui::Mouse &m) {
    std::vector<NativeNode *> path = pathAt(m.x, m.y);
    updateHover(path);

    if (m.button == ui::Mouse::WheelUp || m.button == ui::Mouse::WheelDown) {
      const int delta = (m.button == ui::Mouse::WheelUp ? -1 : 1) * kWheelRows;
      for (NativeNode *p : path) {
        if (!p->hasView) continue;
        const int viewH = p->view.y_max - p->view.y_min + 1;
        const int maxY = std::max(0, p->contentH - viewH);
        if (maxY == 0) continue;
        p->scrollY = std::clamp(p->scrollY + delta, 0, maxY);
        p->stuck = p->scrollY >= maxY;
        return true;
      }
      return false;
    }

    if (m.button == ui::Mouse::Left && m.motion == ui::Mouse::Released) {
      dragging_ = nullptr;
      if (NativeNode *image = std::exchange(dragImage_, nullptr)) {
        if (!imageDragged_) activateImageAt(*image, dragFromX_, dragFromY_);
        return true;
      }
      return false;
    }
    if (m.motion == ui::Mouse::Moved && dragImage_ && m.button == ui::Mouse::Left) {
      if (m.x != dragLastX_ || m.y != dragLastY_) {
        imageDragged_ = true;
        dispatch::drag(*dragImage_->node, (float)(m.x - dragLastX_) * kCellWidthPx, (float)(m.y - dragLastY_) * kCellHeightPx);
        dragLastX_ = m.x;
        dragLastY_ = m.y;
      }
      return true;
    }
    if (m.motion == ui::Mouse::Moved && dragging_ && m.button == ui::Mouse::Left) return setSliderFromX(*dragging_, m.x), true;
    if (!(m.button == ui::Mouse::Left && m.motion == ui::Mouse::Pressed)) return false;

    if (!floatings_.empty()) {
      NativeNode *top = floatings_.back();
      const auto &f = *propsOf<n8v_floating_options>(*top->node);
      if (f.on_dismiss && !inside(top->box, m.x, m.y)) dispatch::dismiss(*top->node);
    }

    if (focused_ && focused_->kind() == Kind::Dropdown && focused_->open) {
      NativeNode &dd = *focused_;
      for (size_t i = 0; i < dd.itemBoxes.size(); ++i) {
        if (inside(dd.itemBoxes[i], m.x, m.y)) {
          dispatch::select(*dd.node, (int)i);
          dd.open = false;
          return true;
        }
      }
      dd.open = false;
    }

    NativeNode *target = nullptr;
    for (NativeNode *p : path) {
      if (std::find(focusables_.begin(), focusables_.end(), p) != focusables_.end()) {
        target = p;
        break;
      }
    }
    if (!target) return true;
    focus(target);
    switch (target->kind()) {
    case Kind::Slider:
      dragging_ = target;
      setSliderFromX(*target, m.x);
      break;
    case Kind::Radio:
      dispatch::select(*target->node, 0);
      break;
    case Kind::Entry:
      placeEntryCursor(*target, m.x, m.y);
      break;
    case Kind::Image: {
      if (propsOf<n8v_image_options>(*target->node)->on_drag) {
        dragImage_ = target;
        imageDragged_ = false;
        dragFromX_ = dragLastX_ = m.x;
        dragFromY_ = dragLastY_ = m.y;
      } else {
        activateImageAt(*target, m.x, m.y);
      }
      break;
    }
    default:
      activate(*target);
      break;
    }
    return true;
  }

  void activateImageAt(NativeNode &image, int x, int y) {
    const int w = image.box.x_max - image.box.x_min + 1, h = image.box.y_max - image.box.y_min + 1;
    if (w > 0 && h > 0) dispatch::activateAt(*image.node, ((float)(x - image.box.x_min) + 0.5f) / (float)w, ((float)(y - image.box.y_min) + 0.5f) / (float)h);
    else activate(image);
  }

  void placeEntryCursor(NativeNode &n, int x, int y) {
    const auto &o = *propsOf<n8v_entry_options>(*n.node);
    const std::string text = entryText(n);
    std::string line = text;
    size_t lineStart = 0;
    if (o.multiline) {
      const std::vector<std::string> all = splitLines(text);
      const int row = std::clamp(y - n.box.y_min + n.vscroll, 0, (int)all.size() - 1);
      for (int i = 0; i < row; ++i) lineStart += all[i].size() + 1;
      line = all[row];
    }
    const std::vector<std::string> glyphs = ui::Utf8ToGlyphs(line);
    const int column = std::clamp(x - n.box.x_min - 1 + (o.multiline ? 0 : n.hscroll), 0, (int)glyphs.size());
    n.cursor = lineStart + byteOffsetOfGlyph(glyphs, (size_t)column);
  }

  std::optional<ui::App> app_;
  ui::Component root_;
  std::unique_ptr<ui::Loop> loop_;
  std::thread ticker_;
  std::atomic<bool> running_{false};
  std::atomic<int> busyTicks_{kBusyTicks};
  bool initialized_ = false;
  int savedStderr_ = -1;

  NativeNode rootNode_;
  IconStyle iconStyle_ = detectIconStyle();
  ImageProtocol imageProtocol_ = detectImageProtocol();
  KittyGraphics kitty_;
  std::unordered_map<const PNode *, std::unique_ptr<NativeNode>> owned_;
  OverlayHost overlays_;
  std::unordered_map<std::string, NativeNode *> byId_;
  std::unordered_set<std::string> hoveredIds_;
  std::unordered_set<const char *> warned_;
  std::vector<NativeNode *> focusables_;
  std::vector<NativeNode *> floatings_;
  std::vector<NativeNode *> lastHoverPath_;
  std::vector<PendingScroll> pendingScrolls_;
  NativeNode *focused_ = nullptr;
  NativeNode *dragging_ = nullptr;
  NativeNode *dragImage_ = nullptr;
  bool imageDragged_ = false;
  int dragFromX_ = 0, dragFromY_ = 0, dragLastX_ = 0, dragLastY_ = 0;
  bool focusMoved_ = false;
  bool escapePressed_ = false;
  std::optional<DialogState> dialog_;
};

} // namespace

std::unique_ptr<Backend> makeFtxuiTreeBackend() { return std::make_unique<FtxuiTreeBackend>(); }

} // namespace n8v::detail
