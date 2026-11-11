#include "qt_tree_backend.hpp"

#include "qt_icons.hpp"

#include "core/entry_lines.hpp"
#include "core/freedesktop_icon_theme.hpp"
#include "core/icon_loader.hpp"
#include "core/icon_registry.hpp"
#include "core/image_loader.hpp"
#include "core/markdown/markdown.hpp"
#include "core/markdown/markdown_to_html.hpp"
#include "core/open_url.hpp"
#include "core/style.hpp"
#include "core/tree/dispatch.hpp"
#include "core/tree/persistent.hpp"
#include "core/tree/props.hpp"
#include "core/tree/reconcile.hpp"
#include "core/widgets/page_badge.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>

#include <QApplication>
#include <QBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QCursor>
#include <QDir>
#include <QEvent>
#include <QEventLoop>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSizePolicy>
#include <QSlider>
#include <QStyle>
#include <QTextDocument>
#include <QVBoxLayout>
#include <QWidget>

#include <cstdio>
#include <cstdlib>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

const char qt_version_tag = 0;

namespace n8v::detail {
namespace {

using tree::BoundValue;
using tree::OwnedProps;
using tree::PNode;
namespace dispatch = tree::dispatch;

template <typename... Ts> struct Overloaded : Ts... {
  using Ts::operator()...;
};
template <typename... Ts> Overloaded(Ts...) -> Overloaded<Ts...>;

template <typename T> const T *propsOf(const PNode &node) { return std::get_if<T>(&node.current.props); }

constexpr std::chrono::milliseconds kScrollSettleTime{50};
constexpr std::chrono::milliseconds kScrollRequestTimeout{2000};
constexpr int kStickyBottomEpsilonPx = 24;
constexpr int kDefaultIconSize = 16;
constexpr int kSliderSteps = 1000;

QString qstr(std::string_view s) { return QString::fromUtf8(s.data(), (qsizetype)s.size()); }
QString qstr(const char *s) { return s ? QString::fromUtf8(s) : QString(); }

void warnOnce(bool &flag, const char *message) {
  if (flag) return;
  flag = true;
  std::fprintf(stderr, "[n8v] qt: %s\n", message);
}

Qt::Alignment hAlignFlag(n8v_align a) {
  switch (a) {
  case N8V_ALIGN_CENTER:
    return Qt::AlignHCenter;
  case N8V_ALIGN_END:
    return Qt::AlignRight;
  case N8V_ALIGN_START:
    break;
  }
  return Qt::AlignLeft;
}

Qt::Alignment vAlignFlag(n8v_align a) {
  switch (a) {
  case N8V_ALIGN_CENTER:
    return Qt::AlignVCenter;
  case N8V_ALIGN_END:
    return Qt::AlignBottom;
  case N8V_ALIGN_START:
    break;
  }
  return Qt::AlignTop;
}

struct ChildContext {
  Qt::Orientation orientation = Qt::Vertical;
  n8v_align hAlign = N8V_ALIGN_START;
  n8v_align vAlign = N8V_ALIGN_START;
};

class N8VButton final : public QPushButton {
public:
  using QPushButton::QPushButton;
  PNode *node = nullptr;

protected:
  void mouseReleaseEvent(QMouseEvent *event) override {
    const bool wasDown = isDown();
    QPushButton::mouseReleaseEvent(event);
    if (wasDown && node && rect().contains(event->position().toPoint())) dispatch::activate(*node);
  }
  void keyReleaseEvent(QKeyEvent *event) override {
    const bool wasDown = isDown();
    QPushButton::keyReleaseEvent(event);
    if (wasDown && node && event->key() == Qt::Key_Space) dispatch::activate(*node);
  }
};

class N8VLabel final : public QLabel {
public:
  using QLabel::QLabel;
  PNode *node = nullptr;
  bool clickable = false;

  void invalidateHint() {
    hintDirty_ = true;
    updateGeometry();
  }

  QSize sizeHint() const override {
    if (!wordWrap()) return QLabel::sizeHint();
    if (hintDirty_) {
      QTextDocument doc;
      doc.setDefaultFont(font());
      doc.setHtml(text());
      doc.setTextWidth(-1);
      idealHint_ = QSize((int)std::ceil(doc.idealWidth()), (int)std::ceil(doc.size().height()));
      hintDirty_ = false;
    }
    const QMargins m = contentsMargins();
    return idealHint_ + QSize(m.left() + m.right() + 2 * frameWidth(), m.top() + m.bottom() + 2 * frameWidth());
  }

protected:
  void changeEvent(QEvent *event) override {
    QLabel::changeEvent(event);
    if (event->type() == QEvent::FontChange) hintDirty_ = true;
  }
  void mouseReleaseEvent(QMouseEvent *event) override {
    QLabel::mouseReleaseEvent(event);
    if (clickable && node && event->button() == Qt::LeftButton) dispatch::activate(*node);
  }

private:
  mutable bool hintDirty_ = true;
  mutable QSize idealHint_;
};

class N8VLineEdit final : public QLineEdit {
public:
  using QLineEdit::QLineEdit;
  PNode *node = nullptr;

protected:
  void keyPressEvent(QKeyEvent *event) override {
    QLineEdit::keyPressEvent(event);
    if (node && (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)) dispatch::submit(*node);
  }
};

class N8VPlainTextEdit final : public QPlainTextEdit {
public:
  using QPlainTextEdit::QPlainTextEdit;
  PNode *node = nullptr;

protected:
  void keyPressEvent(QKeyEvent *event) override {
    if (node && (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)) {
      const auto *o = propsOf<n8v_entry_options>(*node);
      if (o && enterSubmits(o->submit_mode, event->modifiers() & Qt::ControlModifier, event->modifiers() & Qt::ShiftModifier)) {
        dispatch::submit(*node);
        event->accept();
        return;
      }
    }
    QPlainTextEdit::keyPressEvent(event);
  }
};

class N8VRootWidget final : public QWidget {
public:
  using QWidget::QWidget;
  std::function<void(QObject *, bool)> onHover;

  bool eventFilter(QObject *object, QEvent *event) override {
    if (onHover) {
      if (event->type() == QEvent::Enter) onHover(object, true);
      else if (event->type() == QEvent::Leave) onHover(object, false);
    }
    return false;
  }
};

enum class NativeKind { Root, Flex, Panel, Floating, Sidebar, Page, Button, Text, Checkbox, Toggle, Radio, Entry, Dropdown, Slider, Image, Icon, Document };

struct NativeNode {
  NativeKind kind() const { return node ? (NativeKind)node->current.props.index() : NativeKind::Root; }

  PNode *node = nullptr;
  NativeNode *parentNode = nullptr;
  QWidget *outer = nullptr;
  QWidget *content = nullptr;
  QBoxLayout *layout = nullptr;
  QScrollArea *scroll = nullptr;
  QWidget *widget = nullptr;
  int entryHeight = 0;

  // sidebar
  QWidget *panelRoot = nullptr;
  QLabel *titleLabel = nullptr;
  QListWidget *list = nullptr;
  QWidget *contentArea = nullptr;
  QBoxLayout *contentLayout = nullptr;
  // page
  QListWidgetItem *item = nullptr;

  bool floating = false;
  bool startStretch = false;
  bool endStretch = false;
  bool stickyBottom = false;
  bool stickyAtBottom = true;
  int stickyLastMax = 0;
  double percentW = 0.0;
  double percentH = 0.0;
  std::string lastItemsKey;
  std::string iconKey;
  const void *pixmapSource = nullptr;
  std::string registeredId;

  BoundValue lastValue;

  std::vector<NativeNode *> kids;
  ChildContext ctx;
};

struct PendingScroll {
  NativeScrollRequest request;
  std::chrono::steady_clock::time_point requestedAt = std::chrono::steady_clock::now();
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
      [&](const n8v_entry_options &o) {
        if (horizontal) return growSizing();
        return o.multiline && nn.entryHeight > 0 ? n8v_sizing_fixed((float)nn.entryHeight) : fitSizing();
      },
      [&](const n8v_dropdown_options &) { return horizontal ? growSizing() : fitSizing(); },
      [&](const n8v_slider_options &) { return horizontal ? growSizing() : fitSizing(); },
      [&](const auto &) { return fitSizing(); },
    },
    nn.node->current.props
  );
}

ChildContext contextFor(const NativeNode &parent) {
  ChildContext c;
  if (!parent.node) return c;
  std::visit(
    Overloaded{
      [&](const n8v_flex_options &o) {
        c.orientation = o.direction == N8V_DIRECTION_HORIZONTAL ? Qt::Horizontal : Qt::Vertical;
        c.hAlign = o.h_align;
        c.vAlign = o.v_align;
      },
      [&](const n8v_panel_options &o) {
        c.orientation = o.direction == N8V_DIRECTION_HORIZONTAL ? Qt::Horizontal : Qt::Vertical;
        c.hAlign = o.h_align;
        c.vAlign = o.v_align;
      },
      [](const auto &) {},
    },
    parent.node->current.props
  );
  return c;
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

QString escapeHtml(std::string_view text) { return qstr(text).toHtmlEscaped(); }

QPixmap pixmapFromDecoded(const DecodedImage *image) {
  QImage qImage(image->rgba, image->width, image->height, image->width * 4, QImage::Format_RGBA8888);
  return QPixmap::fromImage(qImage);
}

class QtTreeBackend final : public Backend, public tree::TreeOps {
public:
  ~QtTreeBackend() override { shutdown(); }

  bool initialize(int width, int height, std::string_view title) override {
    static int argc = 0;
    static char *argv[] = {nullptr};
    app_ = std::make_unique<QApplication>(argc, argv);

    window_ = new N8VRootWidget();
    window_->setWindowTitle(qstr(title));
    window_->resize(width, height);
    rootLayout_ = new QVBoxLayout(window_);
    rootLayout_->setContentsMargins(0, 0, 0, 0);
    rootLayout_->setSpacing(0);
    window_->onHover = [this](QObject *object, bool entered) {
      auto it = watched_.find(object);
      if (it != watched_.end()) setHover(*it->second, entered);
    };
    app_->installEventFilter(window_);

    root_.outer = window_;
    root_.content = window_;
    root_.layout = rootLayout_;
    window_->show();
    return true;
  }

  bool pumpEvents() override {
    QCoreApplication::processEvents(QEventLoop::AllEvents);
    return window_->isVisible();
  }

  bool pointerDown() const override { return false; }
  Clay_Dimensions windowSize() const override { return {(float)window_->width(), (float)window_->height()}; }
  Clay_Dimensions measureText(std::string_view, FontFamily, uint16_t, bool, bool) const override { return {0, 0}; }

  bool rendersFromClay() const override { return false; }
  bool rendersNativeChrome() const override { return true; }
  tree::TreeOps *treeOps() override { return this; }
  void present(Clay_RenderCommandArray) override {}
  void setCursor(CursorKind) override {}

  void beginFrame() override { pollNative(); }

  void commitFrame() override {
    if (++frames_ == 30 && std::getenv("N8V_QT_DUMP")) dumpWidget(window_, 0);
    updatePercentSizes();
    updateFloating();
    applyStickyBottom();
    resolvePendingScrolls();
  }

  bool isHovered(std::string_view id) const override { return hovered_.count(std::string(id)) != 0; }

  void requestScroll(const NativeScrollRequest &request) override { pendingScrolls_.push_back(PendingScroll{request, std::chrono::steady_clock::now()}); }

  bool showMessageDialog(const MessageDialogRequest &request, MessageDialogDone done) override {
    if (!window_) return false;
    QMessageBox::Icon icon = request.kind == N8V_MESSAGE_DIALOG_ERROR     ? QMessageBox::Critical
                             : request.kind == N8V_MESSAGE_DIALOG_WARNING ? QMessageBox::Warning
                                                                          : QMessageBox::Information;
    QMessageBox box(icon, qstr(request.title), qstr(request.message), QMessageBox::NoButton, window_);
    std::vector<QPushButton *> buttons;
    for (const MessageDialogButton &b : request.buttons)
      buttons.push_back(box.addButton(qstr(b.label), b.destructive ? QMessageBox::DestructiveRole : QMessageBox::ActionRole));
    box.setDefaultButton(buttons.front());
    box.exec();
    int pressed = -1;
    for (size_t i = 0; i < buttons.size(); ++i) {
      if (box.clickedButton() == buttons[i]) pressed = static_cast<int>(i);
    }
    done(pressed);
    return true;
  }

  bool showFileDialog(const FileDialogRequest &request, FileDialogDone done) override {
    if (!window_) return false;
    const QString title = request.title.empty() ? QString() : qstr(request.title);
    const QString filter = request.filters.empty() ? QString() : qstr(joinFilters(request.filters));
    QString dir = qstr(request.initialDir);
    FileDialogResult result;

    switch (request.kind) {
    case N8V_FILE_DIALOG_OPEN_FILE:
      if (request.multiple) {
        for (const QString &p : QFileDialog::getOpenFileNames(window_, title, dir, filter)) result.paths.push_back(p.toStdString());
      } else {
        const QString p = QFileDialog::getOpenFileName(window_, title, dir, filter);
        if (!p.isEmpty()) result.paths.push_back(p.toStdString());
      }
      break;
    case N8V_FILE_DIALOG_SAVE_FILE: {
      if (!request.defaultName.empty()) dir = dir.isEmpty() ? qstr(request.defaultName) : QDir(dir).filePath(qstr(request.defaultName));
      const QString p = QFileDialog::getSaveFileName(window_, title, dir, filter);
      if (!p.isEmpty()) result.paths.push_back(p.toStdString());
      break;
    }
    case N8V_FILE_DIALOG_OPEN_FOLDER: {
      const QString p = QFileDialog::getExistingDirectory(window_, title, dir);
      if (!p.isEmpty()) result.paths.push_back(p.toStdString());
      break;
    }
    }
    result.status = result.paths.empty() ? FileDialogResult::Status::Cancelled : FileDialogResult::Status::Done;
    done(std::move(result));
    return true;
  }

  void shutdown() override {
    if (window_) {
      delete window_;
      window_ = nullptr;
    }
    app_.reset();
  }

  void create(PNode &node) override {
    auto *nn = new NativeNode;
    nn->node = &node;
    node.handle = nn;
    live_.insert(&node);
    build(*nn);
    apply(*nn);
    registerId(*nn);
  }

  void update(PNode &node, const OwnedProps &, unsigned) override {
    NativeNode &nn = *native(node);
    apply(nn);
    registerId(nn);
    if (nn.parentNode && nn.kind() != Kind::Page) layoutChild(nn);
    for (NativeNode *kid : nn.kids) refreshChild(nn, *kid);
    updateStretches(nn);
    if (nn.kind() == Kind::Sidebar) {
      for (NativeNode *kid : nn.kids)
        if (kid->kind() == Kind::Page) apply(*kid);
      syncSidebarSelection(nn);
    }
  }

  void insert(PNode &parent, PNode &child, size_t index) override {
    NativeNode &p = nativeOrRoot(parent);
    NativeNode &c = *native(child);
    c.parentNode = &p;
    p.kids.insert(p.kids.begin() + (ptrdiff_t)index, &c);
    attach(p, c, index);
    refreshChild(p, c);
    updateStretches(p);
    afterKidsChanged(p);
  }

  void move(PNode &parent, PNode &child, size_t index) override {
    NativeNode &p = nativeOrRoot(parent);
    NativeNode &c = *native(child);
    detach(p, c);
    p.kids.erase(std::find(p.kids.begin(), p.kids.end(), &c));
    p.kids.insert(p.kids.begin() + (ptrdiff_t)index, &c);
    attach(p, c, index);
    refreshChild(p, c);
    updateStretches(p);
    afterKidsChanged(p);
  }

  void remove(PNode &parent, PNode &child) override {
    NativeNode &p = nativeOrRoot(parent);
    NativeNode &c = *native(child);
    detach(p, c);
    p.kids.erase(std::find(p.kids.begin(), p.kids.end(), &c));
    c.parentNode = nullptr;
    updateStretches(p);
    afterKidsChanged(p);
  }

  void destroy(PNode &node) override {
    NativeNode *nn = native(node);
    if (!nn) return;
    unregisterId(*nn);
    live_.erase(&node);
    delete nn->item;
    if (nn->kind() == Kind::Page) delete nn->content;
    else delete nn->outer;
    delete nn;
    node.handle = nullptr;
  }

private:
  using Kind = NativeKind;

  NativeNode *native(PNode &node) const { return static_cast<NativeNode *>(node.handle); }
  NativeNode &nativeOrRoot(PNode &node) { return node.parent ? *native(node) : root_; }

  void build(NativeNode &nn);
  void apply(NativeNode &nn);
  void applyFlex(NativeNode &nn, const n8v_flex_options &o);
  void applyPanel(NativeNode &nn, const n8v_panel_options &o);
  void applyButton(NativeNode &nn, const n8v_button_options &o);
  void applyText(NativeNode &nn, const n8v_text_options &o);
  void applySidebar(NativeNode &nn, const n8v_sidebar_options &o);
  void applyPage(NativeNode &nn, const n8v_page_options &o);

  void attach(NativeNode &parent, NativeNode &child, size_t index);
  void detach(NativeNode &parent, NativeNode &child);
  QBoxLayout *layoutFor(NativeNode &parent);
  int layoutIndex(NativeNode &parent, size_t kidIndex) const;
  size_t pageIndex(NativeNode &sidebar, NativeNode &page) const;
  void refreshChild(NativeNode &parent, NativeNode &child);
  void layoutChild(NativeNode &child);
  void updateStretches(NativeNode &container);
  void afterKidsChanged(NativeNode &container);
  void syncSidebarSelection(NativeNode &sidebar);
  void setScrolled(NativeNode &nn, bool wantScrolled, bool horizontal, bool vertical);

  void registerId(NativeNode &nn);
  void unregisterId(NativeNode &nn);
  void setHover(NativeNode &nn, bool hovered);

  static void dumpWidget(QWidget *w, int depth) {
    const QRect g = w->geometry();
    std::fprintf(stderr, "%*s%s @%d,%d %dx%d%s\n", depth * 2, "", w->metaObject()->className(), g.x(), g.y(), g.width(), g.height(), w->isVisible() ? "" : " (hidden)");
    for (QObject *child : w->children())
      if (auto *cw = qobject_cast<QWidget *>(child)) dumpWidget(cw, depth + 1);
  }

  BoundValue readWidget(NativeNode &nn);
  void pollNative();
  void updateMultilineHeight(NativeNode &nn);
  void updatePercentSizes();
  void updateFloating();
  void applyStickyBottom();
  void resolvePendingScrolls();

  QIcon iconFor(const char *name, int pixelSize, QWidget *reference, n8v::Color tint);

  std::unique_ptr<QApplication> app_;
  N8VRootWidget *window_ = nullptr;
  QVBoxLayout *rootLayout_ = nullptr;
  NativeNode root_;
  std::unordered_set<PNode *> live_;
  std::unordered_map<std::string, NativeNode *> byId_;
  std::unordered_map<QObject *, NativeNode *> watched_;
  std::unordered_set<std::string> hovered_;
  std::vector<PendingScroll> pendingScrolls_;
  bool warnedPercent_ = false;
  int frames_ = 0;
};

QBoxLayout *QtTreeBackend::layoutFor(NativeNode &parent) { return parent.kind() == Kind::Sidebar ? parent.contentLayout : parent.layout; }

int QtTreeBackend::layoutIndex(NativeNode &parent, size_t kidIndex) const {
  int index = parent.startStretch ? 1 : 0;
  for (size_t i = 0; i < kidIndex; ++i)
    if (!parent.kids[i]->floating) ++index;
  return index;
}

size_t QtTreeBackend::pageIndex(NativeNode &sidebar, NativeNode &page) const {
  size_t index = 0;
  for (NativeNode *kid : sidebar.kids) {
    if (kid == &page) break;
    if (kid->kind() == Kind::Page) ++index;
  }
  return index;
}

void QtTreeBackend::attach(NativeNode &parent, NativeNode &child, size_t index) {
  if (parent.kind() == Kind::Sidebar && child.kind() == Kind::Page) {
    parent.list->insertItem((int)pageIndex(parent, child), child.item);
    parent.contentLayout->addWidget(child.content);
    return;
  }
  if (child.floating) return;
  layoutFor(parent)->insertWidget(layoutIndex(parent, index), child.outer);
  child.outer->show();
}

void QtTreeBackend::detach(NativeNode &parent, NativeNode &child) {
  if (parent.kind() == Kind::Sidebar && child.kind() == Kind::Page) {
    parent.list->takeItem(parent.list->row(child.item));
    parent.contentLayout->removeWidget(child.content);
    child.content->hide();
    child.content->setParent(nullptr);
    return;
  }
  QWidget *w = child.outer;
  w->hide();
  if (!child.floating) layoutFor(parent)->removeWidget(w);
  w->setParent(nullptr);
}

void QtTreeBackend::refreshChild(NativeNode &parent, NativeNode &child) {
  child.ctx = contextFor(parent);
  if (child.kind() != Kind::Page) layoutChild(child);
}

void QtTreeBackend::layoutChild(NativeNode &c) {
  if (!c.node || !c.outer || c.floating || !c.parentNode) return;
  const bool horizontalMain = c.ctx.orientation == Qt::Horizontal;
  QWidget *w = c.outer;

  struct Axis {
    QSizePolicy::Policy policy = QSizePolicy::Preferred;
    int minimum = 0;
    int maximum = QWIDGETSIZE_MAX;
    bool grow = false;
    double percent = 0.0;
  };
  auto resolve = [&](const n8v_sizing &s) {
    Axis a;
    switch (s.mode) {
    case N8V_SIZING_FIT:
      if (s.min > 0.0f) a.minimum = (int)s.min;
      if (s.max > 0.0f) a.maximum = (int)s.max;
      break;
    case N8V_SIZING_GROW:
      a.policy = QSizePolicy::Expanding;
      a.grow = true;
      if (s.min > 0.0f) a.minimum = (int)s.min;
      if (s.max > 0.0f) a.maximum = (int)s.max;
      break;
    case N8V_SIZING_FIXED:
      a.policy = QSizePolicy::Fixed;
      a.minimum = a.maximum = std::max(0, (int)s.value);
      break;
    case N8V_SIZING_PERCENT:
      a.policy = QSizePolicy::Expanding;
      a.percent = s.value;
      break;
    }
    return a;
  };
  const Axis ax = resolve(sizingFor(c, true));
  const Axis ay = resolve(sizingFor(c, false));

  w->setMinimumWidth(ax.minimum);
  w->setMaximumWidth(ax.maximum);
  w->setMinimumHeight(ay.minimum);
  w->setMaximumHeight(ay.maximum);
  QSizePolicy policy = w->sizePolicy();
  policy.setHorizontalPolicy(ax.policy);
  policy.setVerticalPolicy(ay.policy);
  w->setSizePolicy(policy);
  c.percentW = ax.percent;
  c.percentH = ay.percent;

  QBoxLayout *layout = layoutFor(*c.parentNode);
  if (!layout) return;
  Qt::Alignment flags;
  if (horizontalMain && !ay.grow) flags |= vAlignFlag(c.ctx.vAlign);
  if (!horizontalMain && !ax.grow) flags |= hAlignFlag(c.ctx.hAlign);
  layout->setAlignment(w, flags);
  layout->setStretchFactor(w, (horizontalMain ? ax.grow : ay.grow) ? 1 : 0);

  if (c.scroll) {
    const bool fit = sizingFor(c, true).mode == N8V_SIZING_FIT || sizingFor(c, false).mode == N8V_SIZING_FIT;
    c.scroll->setSizeAdjustPolicy(fit ? QAbstractScrollArea::AdjustToContents : QAbstractScrollArea::AdjustIgnored);
  }
}

void QtTreeBackend::updateStretches(NativeNode &container) {
  const Kind kind = container.kind();
  if (kind != Kind::Root && kind != Kind::Flex && kind != Kind::Panel && kind != Kind::Floating && kind != Kind::Page) return;
  QBoxLayout *layout = container.layout;

  if (container.endStretch) delete layout->takeAt(layout->count() - 1);
  if (container.startStretch) delete layout->takeAt(0);
  container.startStretch = container.endStretch = false;

  const ChildContext cc = contextFor(container);
  const bool horizontal = cc.orientation == Qt::Horizontal;
  const n8v_align mainAlign = horizontal ? cc.hAlign : cc.vAlign;
  bool anyGrow = false;
  for (NativeNode *kid : container.kids) {
    if (kid->floating) continue;
    const n8v_sizing s = sizingFor(*kid, horizontal);
    if (s.mode == N8V_SIZING_GROW || s.mode == N8V_SIZING_PERCENT) anyGrow = true;
  }
  if (anyGrow) return;
  if (mainAlign == N8V_ALIGN_CENTER || mainAlign == N8V_ALIGN_END) {
    layout->insertStretch(0, 1);
    container.startStretch = true;
  }
  if (mainAlign == N8V_ALIGN_START || mainAlign == N8V_ALIGN_CENTER) {
    layout->addStretch(1);
    container.endStretch = true;
  }
}

void QtTreeBackend::afterKidsChanged(NativeNode &container) {
  if (container.kind() == Kind::Sidebar) {
    for (NativeNode *kid : container.kids)
      if (kid->kind() == Kind::Page) kid->content->setVisible(!kid->kids.empty());
    syncSidebarSelection(container);
  } else if (container.kind() == Kind::Page) {
    container.content->setVisible(!container.kids.empty());
  }
}

void QtTreeBackend::syncSidebarSelection(NativeNode &sidebar) {
  if (!sidebar.list) return;
  int selected = 0;
  if (const int *bound = std::get_if<int>(&sidebar.node->bound)) selected = *bound;
  if (selected >= 0 && selected < sidebar.list->count()) sidebar.list->setCurrentRow(selected);
  sidebar.lastValue = readWidget(sidebar);
  for (NativeNode *kid : sidebar.kids)
    if (kid->kind() == Kind::Page) kid->content->setVisible(!kid->kids.empty());
}

void QtTreeBackend::setScrolled(NativeNode &nn, bool want, bool horizontal, bool vertical) {
  auto replaceOuter = [&](QWidget *replacement) {
    QWidget *old = nn.outer;
    if (nn.parentNode && layoutFor(*nn.parentNode)) {
      QBoxLayout *layout = layoutFor(*nn.parentNode);
      const int index = layout->indexOf(old);
      layout->removeWidget(old);
      old->setParent(nullptr);
      layout->insertWidget(index, replacement);
      replacement->show();
    }
    watched_.erase(old);
    nn.outer = replacement;
  };
  if (want && !nn.scroll) {
    auto *scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    QWidget *inner = nn.outer;
    replaceOuter(scroll);
    scroll->setWidget(inner);
    nn.scroll = scroll;
  } else if (!want && nn.scroll) {
    QScrollArea *scroll = nn.scroll;
    QWidget *inner = scroll->takeWidget();
    replaceOuter(inner);
    delete scroll;
    nn.scroll = nullptr;
  }
  if (nn.scroll) {
    nn.scroll->setHorizontalScrollBarPolicy(horizontal ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
    nn.scroll->setVerticalScrollBarPolicy(vertical ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
  }
}

void QtTreeBackend::unregisterId(NativeNode &nn) {
  if (nn.outer) watched_.erase(nn.outer);
  if (nn.registeredId.empty()) return;
  auto it = byId_.find(nn.registeredId);
  if (it != byId_.end() && it->second == &nn) byId_.erase(it);
  hovered_.erase(nn.registeredId);
  nn.registeredId.clear();
}

void QtTreeBackend::registerId(NativeNode &nn) {
  const char *id = tree::idOf(nn.node->current.props);
  const std::string wanted = id ? id : "";
  if (wanted != nn.registeredId) {
    if (!nn.registeredId.empty()) {
      auto it = byId_.find(nn.registeredId);
      if (it != byId_.end() && it->second == &nn) byId_.erase(it);
      hovered_.erase(nn.registeredId);
    }
    nn.registeredId = wanted;
    if (!wanted.empty()) byId_[wanted] = &nn;
  }
  const auto *flex = propsOf<n8v_flex_options>(*nn.node);
  if (nn.outer && (!wanted.empty() || (flex && flex->on_hover))) {
    watched_.erase(nn.outer);
    watched_[nn.outer] = &nn;
    if (nn.content && nn.content != nn.outer) watched_[nn.content] = &nn;
  }
}

void QtTreeBackend::setHover(NativeNode &nn, bool hovered) {
  if (!live_.count(nn.node)) return;
  if (!nn.registeredId.empty()) {
    if (hovered) hovered_.insert(nn.registeredId);
    else hovered_.erase(nn.registeredId);
  }
  dispatch::hover(*nn.node, hovered);
}

BoundValue QtTreeBackend::readWidget(NativeNode &nn) {
  switch (nn.kind()) {
  case Kind::Checkbox:
    return static_cast<QCheckBox *>(nn.widget)->isChecked();
  case Kind::Toggle:
    return static_cast<QPushButton *>(nn.widget)->isChecked();
  case Kind::Radio:
    return static_cast<QRadioButton *>(nn.widget)->isChecked();
  case Kind::Entry:
    if (const auto *entry = propsOf<n8v_entry_options>(*nn.node); entry && entry->multiline) return static_cast<QPlainTextEdit *>(nn.widget)->toPlainText().toStdString();
    return static_cast<QLineEdit *>(nn.widget)->text().toStdString();
  case Kind::Dropdown:
    return static_cast<QComboBox *>(nn.widget)->currentIndex();
  case Kind::Slider:
    return (float)static_cast<QSlider *>(nn.widget)->value();
  case Kind::Sidebar:
    return nn.list ? nn.list->currentRow() : 0;
  default:
    return {};
  }
}

void QtTreeBackend::updateMultilineHeight(NativeNode &nn) {
  const auto *o = propsOf<n8v_entry_options>(*nn.node);
  if (!o || !o->multiline) return;
  auto *edit = static_cast<QPlainTextEdit *>(nn.widget);
  const EntryLines lines = entryLines(*o);
  int count = lines.min;
  if (!lines.fixed) count = std::clamp((int)std::ceil(edit->document()->size().height()), lines.min, lines.max);
  const int chrome = 2 * edit->frameWidth() + (int)std::ceil(2 * edit->document()->documentMargin()) + 2;
  const int height = chrome + count * edit->fontMetrics().lineSpacing();
  const bool overflowing = (int)std::ceil(edit->document()->size().height()) > lines.max;
  edit->setVerticalScrollBarPolicy(lines.fixed || overflowing ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
  if (height == nn.entryHeight) return;
  nn.entryHeight = height;
  if (nn.parentNode) layoutChild(nn);
}

void QtTreeBackend::pollNative() {
  const std::vector<PNode *> nodes(live_.begin(), live_.end());
  for (PNode *pn : nodes) {
    NativeNode &nn = *native(*pn);
    switch (nn.kind()) {
    case Kind::Checkbox:
    case Kind::Toggle:
    case Kind::Radio:
    case Kind::Entry:
    case Kind::Dropdown:
    case Kind::Slider:
    case Kind::Sidebar:
      break;
    default:
      continue;
    }
    if (nn.kind() == Kind::Entry) updateMultilineHeight(nn);
    BoundValue current = readWidget(nn);
    if (current == nn.lastValue) continue;
    nn.lastValue = current;

    switch (nn.kind()) {
    case Kind::Checkbox:
    case Kind::Toggle:
      dispatch::setChecked(*pn, std::get<bool>(current));
      break;
    case Kind::Radio:
      if (std::get<bool>(current)) {
        dispatch::select(*pn, 0);
      } else {
        static_cast<QRadioButton *>(nn.widget)->setChecked(true);
        nn.lastValue = readWidget(nn);
      }
      break;
    case Kind::Entry:
      dispatch::setText(*pn, std::get<std::string>(current));
      break;
    case Kind::Dropdown:
      if (std::get<int>(current) >= 0) dispatch::select(*pn, std::get<int>(current));
      break;
    case Kind::Slider: {
      const auto *o = propsOf<n8v_slider_options>(*pn);
      if (o) dispatch::setSlider(*pn, o->min + (std::get<float>(current) / (float)kSliderSteps) * (o->max - o->min));
      break;
    }
    case Kind::Sidebar: {
      const int row = std::get<int>(current);
      int index = 0;
      for (NativeNode *kid : nn.kids) {
        if (kid->kind() != Kind::Page) continue;
        if (index++ == row) {
          dispatch::selectPage(*kid->node);
          break;
        }
      }
      break;
    }
    default:
      break;
    }
  }
}

void QtTreeBackend::updatePercentSizes() {
  for (PNode *pn : live_) {
    NativeNode &nn = *native(*pn);
    if ((nn.percentW <= 0.0 && nn.percentH <= 0.0) || !nn.outer || !nn.parentNode) continue;
    warnOnce(warnedPercent_, "Sizing::percent is approximated by resizing against the parent each frame");
    QWidget *parentWidget = nn.parentNode->content ? nn.parentNode->content : nn.parentNode->outer;
    if (!parentWidget) continue;
    const QMargins m = nn.parentNode->layout ? nn.parentNode->layout->contentsMargins() : QMargins();
    if (nn.percentW > 0.0) nn.outer->setFixedWidth(std::max(0, (int)((parentWidget->width() - m.left() - m.right()) * nn.percentW)));
    if (nn.percentH > 0.0) nn.outer->setFixedHeight(std::max(0, (int)((parentWidget->height() - m.top() - m.bottom()) * nn.percentH)));
  }
}

void QtTreeBackend::updateFloating() {
  for (PNode *pn : live_) {
    NativeNode &nn = *native(*pn);
    if (!nn.floating || !nn.parentNode || !nn.outer) continue;
    const auto *o = propsOf<n8v_floating_options>(*pn);
    if (!o) continue;

    NativeNode &host = *nn.parentNode;
    QWidget *hostWidget = host.outer ? host.outer : host.content;
    QWidget *container = hostWidget == window_ ? window_ : hostWidget->parentWidget();
    if (!container) container = window_;
    const QRect hostRect = hostWidget == window_ ? window_->rect() : hostWidget->geometry();
    if (nn.outer->parentWidget() != container) {
      nn.outer->setParent(container);
      nn.outer->show();
    }

    const QSize hint = nn.outer->sizeHint();
    int w = o->width.mode == N8V_SIZING_FIXED ? (int)o->width.value : o->width.mode == N8V_SIZING_GROW ? hostRect.width() : hint.width();
    int h = o->height.mode == N8V_SIZING_FIXED ? (int)o->height.value : o->height.mode == N8V_SIZING_GROW ? hostRect.height() : hint.height();
    if (nn.outer->layout() && nn.outer->layout()->hasHeightForWidth() && o->height.mode != N8V_SIZING_FIXED) h = nn.outer->layout()->heightForWidth(w);

    double pfx = 0, pfy = 0, efx = 0, efy = 0;
    attachFraction(o->parent_attach_point, pfx, pfy);
    attachFraction(o->element_attach_point, efx, efy);
    const int x = hostRect.x() + (int)(pfx * hostRect.width() - efx * w + o->offset_x);
    const int y = hostRect.y() + (int)(pfy * hostRect.height() - efy * h + o->offset_y);
    nn.outer->setGeometry(x, y, w, h);
    nn.outer->raise();
  }
}

void QtTreeBackend::applyStickyBottom() {
  for (auto &[id, nn] : byId_) {
    if (!nn->stickyBottom || !nn->scroll) continue;
    QScrollBar *bar = nn->scroll->verticalScrollBar();
    const int max = bar->maximum();
    if (max != nn->stickyLastMax) {
      if (nn->stickyAtBottom) bar->setValue(max);
      nn->stickyLastMax = max;
    }
    nn->stickyAtBottom = bar->value() >= max - kStickyBottomEpsilonPx;
  }
}

void QtTreeBackend::resolvePendingScrolls() {
  const auto now = std::chrono::steady_clock::now();
  for (auto it = pendingScrolls_.begin(); it != pendingScrolls_.end();) {
    const NativeScrollRequest &req = it->request;
    if (now - it->requestedAt < kScrollSettleTime) {
      ++it;
      continue;
    }
    bool resolved = false;
    auto container = byId_.find(req.containerId);
    if (container != byId_.end() && container->second->scroll) {
      NativeNode &c = *container->second;
      QScrollBar *hbar = c.scroll->horizontalScrollBar();
      QScrollBar *vbar = c.scroll->verticalScrollBar();
      resolved = true;
      switch (req.kind) {
      case NativeScrollRequest::Kind::Bottom:
        vbar->setValue(vbar->maximum());
        break;
      case NativeScrollRequest::Kind::Top:
        vbar->setValue(0);
        break;
      case NativeScrollRequest::Kind::Left:
        hbar->setValue(0);
        break;
      case NativeScrollRequest::Kind::Right:
        hbar->setValue(hbar->maximum());
        break;
      case NativeScrollRequest::Kind::Offset:
        hbar->setValue(std::clamp((int)req.x, 0, hbar->maximum()));
        vbar->setValue(std::clamp((int)req.y, 0, vbar->maximum()));
        break;
      case NativeScrollRequest::Kind::Element: {
        auto element = byId_.find(req.elementId);
        if (element == byId_.end() || !element->second->outer) {
          resolved = false;
          break;
        }
        const QPoint p = element->second->outer->mapTo(c.content, QPoint(0, 0));
        vbar->setValue(std::clamp(p.y() - (int)req.paddingTop, 0, vbar->maximum()));
        hbar->setValue(std::clamp(p.x() - (int)req.paddingLeft, 0, hbar->maximum()));
        break;
      }
      }
    }
    if (resolved) {
      it = pendingScrolls_.erase(it);
    } else if (now - it->requestedAt > kScrollRequestTimeout) {
      std::fprintf(stderr, "[n8v] scroll request for container '%s' timed out (container or target element id not found)\n", req.containerId.c_str());
      it = pendingScrolls_.erase(it);
    } else {
      ++it;
    }
  }
}

QIcon QtTreeBackend::iconFor(const char *name, int pixelSize, QWidget *reference, n8v::Color tint) {
  if (!name || !*name) return {};
  const char *freedesktopName = resolveFreedesktopIconName(name);

  const bool explicitTint = tint.a > 0.0f;
  const QColor text = reference->palette().color(reference->foregroundRole());
  const QColor highlighted = reference->palette().color(QPalette::HighlightedText);
  const n8v::Color normalTint = explicitTint ? tint : n8v::Color{(float)text.red(), (float)text.green(), (float)text.blue(), 255.0f};
  const n8v::Color selectedTint = explicitTint ? tint : n8v::Color{(float)highlighted.red(), (float)highlighted.green(), (float)highlighted.blue(), 255.0f};

  QIcon icon;
  if (const DecodedImage *decoded = getOrDecodeIcon(name, (uint16_t)pixelSize, normalTint)) {
    icon.addPixmap(pixmapFromDecoded(decoded), QIcon::Normal);
    if (const DecodedImage *onSelection = getOrDecodeIcon(name, (uint16_t)pixelSize, selectedTint)) icon.addPixmap(pixmapFromDecoded(onSelection), QIcon::Selected);
    return icon;
  }
  if (freedesktopName) {
    icon = QIcon::fromTheme(qstr(freedesktopName));
    if (!icon.isNull()) return icon;
    const std::string path = findFreedesktopIconFile(freedesktopName);
    if (!path.empty()) {
      if (const DecodedImage *themed = getOrDecodeIconFromFile(path, (uint16_t)pixelSize, normalTint)) return QIcon(pixmapFromDecoded(themed));
    }
  }
  QStyle::StandardPixmap standard;
  if (qt_icons::resolveStandardPixmap(name, standard)) return reference->style()->standardIcon(standard);
  return {};
}

void QtTreeBackend::build(NativeNode &nn) {
  PNode &pn = *nn.node;
  switch (nn.kind()) {
  case Kind::Flex:
  case Kind::Floating: {
    auto *box = new QWidget();
    nn.layout = new QBoxLayout(QBoxLayout::TopToBottom, box);
    nn.layout->setContentsMargins(0, 0, 0, 0);
    nn.layout->setSpacing(0);
    nn.outer = nn.content = box;
    nn.floating = nn.kind() == Kind::Floating;
    break;
  }
  case Kind::Panel: {
    auto *frame = new QFrame();
    nn.layout = new QBoxLayout(QBoxLayout::TopToBottom, frame);
    nn.outer = nn.content = frame;
    break;
  }
  case Kind::Text: {
    auto *label = new N8VLabel();
    label->node = &pn;
    label->setTextFormat(Qt::RichText);
    label->setWordWrap(true);
    label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    nn.outer = nn.widget = label;
    break;
  }
  case Kind::Button: {
    auto *button = new N8VButton();
    button->node = &pn;
    button->setAutoDefault(false);
    nn.outer = nn.widget = button;
    break;
  }
  case Kind::Checkbox:
    nn.outer = nn.widget = new QCheckBox();
    break;
  case Kind::Radio: {
    auto *radio = new QRadioButton();
    radio->setAutoExclusive(false);
    nn.outer = nn.widget = radio;
    break;
  }
  case Kind::Toggle: {
    auto *toggle = new QPushButton();
    toggle->setCheckable(true);
    nn.outer = nn.widget = toggle;
    break;
  }
  case Kind::Entry: {
    if (const auto *entry = propsOf<n8v_entry_options>(pn); entry && entry->multiline) {
      auto *edit = new N8VPlainTextEdit();
      edit->node = &pn;
      edit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
      nn.outer = nn.widget = edit;
      break;
    }
    auto *edit = new N8VLineEdit();
    edit->node = &pn;
    nn.outer = nn.widget = edit;
    break;
  }
  case Kind::Dropdown:
    nn.outer = nn.widget = new QComboBox();
    break;
  case Kind::Slider: {
    auto *slider = new QSlider(Qt::Horizontal);
    slider->setRange(0, kSliderSteps);
    nn.outer = nn.widget = slider;
    break;
  }
  case Kind::Image:
  case Kind::Icon: {
    auto *label = new QLabel();
    label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    nn.outer = nn.widget = label;
    break;
  }
  case Kind::Document: {
    auto *label = new QLabel();
    label->setTextFormat(Qt::MarkdownText);
    label->setWordWrap(true);
    label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    label->setOpenExternalLinks(true);
    nn.outer = nn.widget = label;
    break;
  }
  case Kind::Sidebar: {
    nn.outer = new QWidget();
    auto *hbox = new QHBoxLayout(nn.outer);
    hbox->setContentsMargins(0, 0, 0, 0);
    hbox->setSpacing(0);

    nn.panelRoot = new QWidget();
    nn.panelRoot->setAutoFillBackground(true);
    QPalette panelPalette = nn.panelRoot->palette();
    panelPalette.setColor(QPalette::Window, panelPalette.color(QPalette::Base));
    nn.panelRoot->setPalette(panelPalette);
    auto *panelLayout = new QVBoxLayout(nn.panelRoot);
    panelLayout->setContentsMargins(0, 0, 0, 0);
    panelLayout->setSpacing(0);
    nn.titleLabel = new QLabel();
    nn.titleLabel->setAlignment(Qt::AlignCenter);
    nn.titleLabel->setContentsMargins(0, 12, 0, 12);
    QFont titleFont = nn.titleLabel->font();
    titleFont.setBold(true);
    nn.titleLabel->setFont(titleFont);
    nn.list = new QListWidget();
    nn.list->setFrameShape(QFrame::NoFrame);
    nn.list->setSelectionMode(QAbstractItemView::SingleSelection);
    nn.list->setIconSize(QSize(kDefaultIconSize, kDefaultIconSize));
    panelLayout->addWidget(nn.titleLabel);
    panelLayout->addWidget(nn.list, 1);

    nn.contentArea = new QWidget();
    nn.contentLayout = new QVBoxLayout(nn.contentArea);
    nn.contentLayout->setContentsMargins(0, 0, 0, 0);
    nn.contentLayout->setSpacing(0);

    hbox->addWidget(nn.panelRoot, 0);
    hbox->addWidget(nn.contentArea, 1);
    nn.layout = hbox;
    break;
  }
  case Kind::Page: {
    nn.content = new QWidget();
    nn.layout = new QBoxLayout(QBoxLayout::TopToBottom, nn.content);
    nn.layout->setContentsMargins(0, 0, 0, 0);
    nn.layout->setSpacing(0);
    nn.content->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    nn.content->setVisible(false);
    nn.item = new QListWidgetItem();
    break;
  }
  case Kind::Root:
    break;
  }
}

void QtTreeBackend::applyFlex(NativeNode &nn, const n8v_flex_options &o) {
  nn.layout->setDirection(o.direction == N8V_DIRECTION_HORIZONTAL ? QBoxLayout::LeftToRight : QBoxLayout::TopToBottom);
  nn.layout->setSpacing(o.gap);
  nn.layout->setContentsMargins(o.padding.left, o.padding.top, o.padding.right, o.padding.bottom);
  setScrolled(nn, o.clip_horizontal || o.clip_vertical, o.clip_horizontal, o.clip_vertical);
  nn.stickyBottom = o.stick_to_bottom && o.clip_vertical;
}

void QtTreeBackend::applyPanel(NativeNode &nn, const n8v_panel_options &o) {
  auto *frame = static_cast<QFrame *>(nn.outer);
  nn.layout->setDirection(o.direction == N8V_DIRECTION_HORIZONTAL ? QBoxLayout::LeftToRight : QBoxLayout::TopToBottom);
  nn.layout->setSpacing(o.gap);
  const bool card = o.role == N8V_PANEL_ROLE_CARD;
  const int vpad = card ? 10 : 6, hpad = card ? 12 : 10;
  nn.layout->setContentsMargins(hpad, vpad, hpad, vpad);
  frame->setFrameShape(card ? QFrame::StyledPanel : QFrame::NoFrame);
}

void QtTreeBackend::applyButton(NativeNode &nn, const n8v_button_options &o) {
  auto *button = static_cast<N8VButton *>(nn.widget);
  button->setText(qstr(nn.node->current.label));
  const bool hasIcon = o.icon && *o.icon;
  if (hasIcon) {
    const std::string key = std::string(o.icon) + (o.icon_variant == N8V_ICON_VARIANT_FILLED ? "#filled" : "");
    if (key != nn.iconKey) {
      button->setIcon(iconFor(o.icon, kDefaultIconSize, button, n8v::Color{0, 0, 0, 0}));
      button->setIconSize(QSize(kDefaultIconSize, kDefaultIconSize));
      nn.iconKey = key;
    }
  } else if (!nn.iconKey.empty()) {
    button->setIcon(QIcon());
    nn.iconKey.clear();
  }
  button->setLayoutDirection(o.icon_position == N8V_ICON_POSITION_TRAILING ? Qt::RightToLeft : Qt::LeftToRight);
  button->setFlat(o.style == N8V_BUTTON_STYLE_GHOST);
  button->setCheckable(o.checked != nullptr);
  if (o.checked && button->isChecked() != *o.checked) {
    const QSignalBlocker blocker(button);
    button->setChecked(*o.checked);
  }
  QFont font = button->font();
  font.setBold(o.style == N8V_BUTTON_STYLE_PRIMARY);
  button->setFont(font);
}

void QtTreeBackend::applyText(NativeNode &nn, const n8v_text_options &o) {
  auto *label = static_cast<N8VLabel *>(nn.widget);
  QString html = escapeHtml(nn.node->current.label);
  if (o.strikethrough) html = "<s>" + html + "</s>";
  if (o.italic) html = "<i>" + html + "</i>";
  if (o.bold) html = "<b>" + html + "</b>";
  const bool isDefaultColor = o.color.a <= 0.0f || (o.color.r == 0.0f && o.color.g == 0.0f && o.color.b == 0.0f && o.color.a == 255.0f);
  if (!isDefaultColor)
    html =
      QString("<span style=\"color:rgba(%1,%2,%3,%4)\">").arg((int)o.color.r).arg((int)o.color.g).arg((int)o.color.b).arg(o.color.a / 255.0, 0, 'f', 3) + html + "</span>";
  const bool link = (o.url && *o.url) || o.on_click;
  if (link) html = "<a href=\"#\">" + html + "</a>";
  label->setText(html);
  label->invalidateHint();
  label->clickable = link;
  label->setCursor(link ? Qt::PointingHandCursor : Qt::ArrowCursor);
}

void QtTreeBackend::applySidebar(NativeNode &nn, const n8v_sidebar_options &o) {
  const std::string title = o.title ? o.title : "";
  nn.titleLabel->setText(qstr(title));
  nn.titleLabel->setVisible(!title.empty());
  const int width = o.width.mode == N8V_SIZING_FIXED ? (int)o.width.value : 0;
  if (width > 0) nn.panelRoot->setFixedWidth(width);
  else {
    nn.panelRoot->setMinimumWidth(0);
    nn.panelRoot->setMaximumWidth(QWIDGETSIZE_MAX);
  }
  if (o.min_width > 0.0f) nn.panelRoot->setMinimumWidth((int)o.min_width);
  if (o.max_width > 0.0f) nn.panelRoot->setMaximumWidth((int)o.max_width);
}

void QtTreeBackend::applyPage(NativeNode &nn, const n8v_page_options &o) {
  if (!nn.item) return;
  NativeNode *sidebar = nn.node->parent ? native(*nn.node->parent) : nullptr;
  const auto *sidebarProps = sidebar && sidebar->node ? propsOf<n8v_sidebar_options>(*sidebar->node) : nullptr;
  const bool compact = sidebarProps && sidebarProps->compact;
  nn.item->setText(qstr(n8v::detail::pageLabelWithBadges(o.name ? o.name : "", o.notify, o.badge)));
  nn.item->setSizeHint(QSize(0, compact ? 26 : 38));
  if (o.icon && *o.icon) {
    if (nn.iconKey != o.icon) {
      nn.item->setIcon(iconFor(o.icon, kDefaultIconSize, sidebar && sidebar->list ? static_cast<QWidget *>(sidebar->list) : window_, n8v::Color{0, 0, 0, 0}));
      nn.iconKey = o.icon;
    }
  } else if (!nn.iconKey.empty()) {
    nn.item->setIcon(QIcon());
    nn.iconKey.clear();
  }
}

void QtTreeBackend::apply(NativeNode &nn) {
  PNode &pn = *nn.node;
  const std::string &label = pn.current.label;
  std::visit(
    Overloaded{
      [&](const std::monostate &) {},
      [&](const n8v_flex_options &o) { applyFlex(nn, o); },
      [&](const n8v_panel_options &o) { applyPanel(nn, o); },
      [&](const n8v_floating_options &) {},
      [&](const n8v_sidebar_options &o) { applySidebar(nn, o); },
      [&](const n8v_page_options &o) { applyPage(nn, o); },
      [&](const n8v_button_options &o) { applyButton(nn, o); },
      [&](const n8v_text_options &o) { applyText(nn, o); },
      [&](const n8v_checkbox_options &) {
        auto *box = static_cast<QCheckBox *>(nn.widget);
        box->setText(qstr(label));
        if (const bool *checked = std::get_if<bool>(&pn.bound)) box->setChecked(*checked);
      },
      [&](const n8v_toggle_options &) {
        auto *toggle = static_cast<QPushButton *>(nn.widget);
        toggle->setText(qstr(label));
        if (const bool *checked = std::get_if<bool>(&pn.bound)) toggle->setChecked(*checked);
      },
      [&](const n8v_radio_options &o) {
        auto *radio = static_cast<QRadioButton *>(nn.widget);
        radio->setText(qstr(label));
        if (const int *selected = std::get_if<int>(&pn.bound)) radio->setChecked(*selected == o.value);
      },
      [&](const n8v_entry_options &o) {
        if (o.multiline) {
          auto *edit = static_cast<QPlainTextEdit *>(nn.widget);
          edit->setPlaceholderText(qstr(o.placeholder));
          if (const std::string *text = std::get_if<std::string>(&pn.bound)) {
            const QString want = qstr(*text);
            if (edit->toPlainText() != want) edit->setPlainText(want);
          }
          updateMultilineHeight(nn);
          return;
        }
        auto *edit = static_cast<QLineEdit *>(nn.widget);
        edit->setPlaceholderText(qstr(o.placeholder));
        edit->setEchoMode(o.password ? QLineEdit::Password : QLineEdit::Normal);
        if (const std::string *text = std::get_if<std::string>(&pn.bound)) {
          const QString want = qstr(*text);
          if (edit->text() != want) edit->setText(want);
        }
      },
      [&](const n8v_dropdown_options &o) {
        auto *combo = static_cast<QComboBox *>(nn.widget);
        std::string key = std::string(o.placeholder ? o.placeholder : "") + '\n';
        QStringList items;
        for (size_t i = 0; i < o.item_count; ++i) {
          items << qstr(o.items[i]);
          key += std::string(o.items[i] ? o.items[i] : "") + '\n';
        }
        if (key != nn.lastItemsKey) {
          combo->clear();
          combo->addItems(items);
          combo->setPlaceholderText(qstr(o.placeholder));
          nn.lastItemsKey = key;
        }
        const int *selected = std::get_if<int>(&pn.bound);
        const int want = selected && *selected >= 0 && (size_t)*selected < o.item_count ? *selected : -1;
        if (combo->currentIndex() != want) combo->setCurrentIndex(want);
      },
      [&](const n8v_slider_options &o) {
        auto *slider = static_cast<QSlider *>(nn.widget);
        if (const float *value = std::get_if<float>(&pn.bound)) {
          const float span = o.max - o.min;
          const int want = span > 0.0f ? (int)(((*value - o.min) / span) * (float)kSliderSteps + 0.5f) : 0;
          if (slider->value() != want) slider->setValue(want);
        }
      },
      [&](const n8v_image_options &o) {
        auto *labelWidget = static_cast<QLabel *>(nn.widget);
        const DecodedImage *decoded = getOrDecodeImage(o);
        if (!decoded) {
          labelWidget->clear();
          nn.pixmapSource = nullptr;
          return;
        }
        CornerRadius radius;
        switch (o.rounding.mode) {
        case N8V_ROUNDING_STYLE_DEFAULT:
          radius = activePaint().image().cornerRadius;
          break;
        case N8V_ROUNDING_NONE:
          radius = {};
          break;
        case N8V_ROUNDING_FIXED:
          radius = {o.rounding.radius.top_left, o.rounding.radius.top_right, o.rounding.radius.bottom_left, o.rounding.radius.bottom_right};
          break;
        }
        const int targetW = o.width.mode == N8V_SIZING_FIXED ? std::max(1, (int)o.width.value) : decoded->width;
        const int targetH = o.height.mode == N8V_SIZING_FIXED ? std::max(1, (int)o.height.value) : decoded->height;
        const DecodedImage *baked = getOrBakeRoundedImage(decoded, targetW, targetH, radius.topLeft, radius.topRight, radius.bottomLeft, radius.bottomRight);
        if (nn.pixmapSource != baked) {
          labelWidget->setPixmap(pixmapFromDecoded(baked));
          nn.pixmapSource = baked;
        }
      },
      [&](const n8v_icon_options &o) {
        auto *labelWidget = static_cast<QLabel *>(nn.widget);
        const int size = o.width.mode == N8V_SIZING_FIXED ? (int)o.width.value : kDefaultIconSize;
        const std::string key = std::string(o.name ? o.name : "") + "@" + std::to_string(size);
        if (key != nn.iconKey) {
          labelWidget->setPixmap(iconFor(o.name, size, labelWidget, n8v::Color{o.tint.r, o.tint.g, o.tint.b, o.tint.a}).pixmap(size, size));
          nn.iconKey = key;
        }
      },
      [&](const n8v_document_options &o) {
        auto *label = static_cast<QLabel *>(nn.widget);
        if (!o.link_style) {
          label->setText(qstr(o.markdown));
          return;
        }
        const n8v::detail::markdown::CachedDocument cached = n8v::detail::markdown::getOrParse(o.markdown ? o.markdown : "");
        n8v::detail::markdown::HtmlOptions htmlOptions;
        htmlOptions.flavor = n8v::detail::markdown::HtmlFlavor::QtRichText;
        htmlOptions.linkStyler = n8v::detail::markdown::makeLinkStyler(o.link_style, o.link_style_userdata);
        const QColor backdrop = label->palette().color(QPalette::Window);
        htmlOptions.backdrop = {(float)backdrop.red(), (float)backdrop.green(), (float)backdrop.blue(), 255.0f};
        label->setTextFormat(Qt::RichText);
        label->setText(QString::fromStdString(n8v::detail::markdown::toHtml(*cached.document, htmlOptions)));
      },
    },
    pn.current.props
  );
  nn.lastValue = readWidget(nn);
}

} // namespace

std::unique_ptr<Backend> makeQtTreeBackend() { return std::make_unique<QtTreeBackend>(); }

} // namespace n8v::detail
