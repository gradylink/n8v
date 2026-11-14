#include "gtk4_tree_backend.hpp"

#include "gtk4_markup.hpp"

#include "core/entry_lines.hpp"
#include "core/icon_loader.hpp"
#include "core/icon_registry.hpp"
#include "core/image_loader.hpp"
#include "core/markdown/image_state.hpp"
#include "core/open_url.hpp"
#include "core/paste_file.hpp"
#include "core/style.hpp"
#include "core/theme_color.hpp"
#include "core/tree/dispatch.hpp"
#include "core/tree/persistent.hpp"
#include "core/tree/props.hpp"
#include "core/tree/reconcile.hpp"
#include "core/widgets/page_badge.hpp"

#include <gtk/gtk.h>

// fix conflict with function of same name
#undef g_object_ref_sink
#undef g_object_ref

#ifdef N8V_HAVE_ADWAITA
#include <adwaita.h>
#endif

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

extern "C" {
gulong g_signal_connect_data(gpointer instance, const gchar *detailed_signal, GCallback c_handler, gpointer data, GClosureNotify destroy_data, GConnectFlags connect_flags);
gpointer g_object_ref_sink(gpointer object);
gpointer g_object_ref(gpointer object);
void g_object_unref(gpointer object);
}

namespace n8v::detail {
namespace {

using tree::OwnedProps;
using tree::PNode;
namespace dispatch = tree::dispatch;
namespace markup = gtk_markup;

struct NamedColorCss {
  const char *slug;
  const char *foreground;
  const char *fill;
};

#ifdef N8V_HAVE_ADWAITA
constexpr NamedColorCss kNamedColorCss[] = {
  {"red", "@error_color", "@error_bg_color"},
  {"orange", "@orange_3", "@orange_3"},
  {"yellow", "@warning_color", "@warning_bg_color"},
  {"green", "@success_color", "@success_bg_color"},
  {"blue", "@accent_color", "@accent_bg_color"},
  {"purple", "@purple_3", "@purple_3"},
  {"pink", "mix(@purple_3, @red_3, 0.5)", "mix(@purple_3, @red_3, 0.5)"},
};
#else
constexpr NamedColorCss kNamedColorCss[] = {
  {"red", "#e01b24", "#e01b24"},
  {"orange", "#e66100", "#e66100"},
  {"yellow", "#f5c211", "#f5c211"},
  {"green", "#26a269", "#26a269"},
  {"blue", "#3584e4", "#3584e4"},
  {"purple", "#9141ac", "#9141ac"},
  {"pink", "#d6428f", "#d6428f"},
};
#endif

std::string namedColorCss() {
  std::string css;
  for (const NamedColorCss &c : kNamedColorCss) {
    css += std::string(".n8v-fg-") + c.slug + " { color: " + c.foreground + "; }\n";
    css += std::string(".n8v-divider.n8v-fill-") + c.slug + " { background-color: " + c.fill + "; }\n";
    css += std::string(".n8v-accent-") + c.slug + " { border-left: 4px solid " + c.fill + "; }\n";
  }
  return css;
}

std::string themeClass(const char *prefix, const n8v_color &color) {
  if (!isThemeColor(color)) return "";
  const char *slug = themeColorSlug(themeColorName(color));
  return slug ? std::string(prefix) + slug : "";
}

template <typename... Ts> struct Overloaded : Ts... {
  using Ts::operator()...;
};
template <typename... Ts> Overloaded(Ts...) -> Overloaded<Ts...>;

template <typename T> const T *propsOf(const PNode &node) { return std::get_if<T>(&node.current.props); }

constexpr double kStickyBottomEpsilonPx = 24.0;
constexpr int kDefaultIconSize = 16;
constexpr int kMultilineEntryMarginPx = 8;
constexpr int kMultilineSideMarginPx = 9;
constexpr int kMultilineFrameMarginPx = 6;

void warnOnce(bool &flag, const char *message) {
  if (flag) return;
  flag = true;
  std::fprintf(stderr, "[n8v] gtk: %s\n", message);
}

GtkAlign toGtkAlign(n8v_align a) {
  switch (a) {
  case N8V_ALIGN_CENTER:
    return GTK_ALIGN_CENTER;
  case N8V_ALIGN_END:
    return GTK_ALIGN_END;
  case N8V_ALIGN_START:
    break;
  }
  return GTK_ALIGN_START;
}

struct ChildContext {
  GtkOrientation orientation = GTK_ORIENTATION_VERTICAL;
  n8v_align hAlign = N8V_ALIGN_START;
  n8v_align vAlign = N8V_ALIGN_START;
};

enum class NativeKind {
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
  NativeKind kind() const { return node ? (NativeKind)node->current.props.index() : NativeKind::Root; }

  PNode *node = nullptr;
  GtkWidget *outer = nullptr;
  GtkWidget *content = nullptr;
  GtkWidget *scrolled = nullptr;
  GtkWidget *overlay = nullptr;
  GtkWidget *widget = nullptr;
  GtkWidget *label = nullptr;
  GtkWidget *image = nullptr;
  GtkWidget *picture = nullptr;
  GtkWidget *startSpacer = nullptr;
  GtkWidget *endSpacer = nullptr;
  GtkWidget *backdrop = nullptr;
  int entryHeight = 0;

  // sidebar
  GtkWidget *panelRoot = nullptr;
  GtkWidget *listBox = nullptr;
  GtkWidget *titleLabel = nullptr;
  GtkWidget *contentArea = nullptr;
  GtkWidget *rowLabel = nullptr;
  GtkWidget *badge = nullptr;
  GtkWidget *notifyBar = nullptr;
  bool adwRow = false;

  bool floating = false;
  bool hasIconBox = false;
  bool iconTrailing = false;
  bool hoverWatched = false;
  bool stickyAtBottom = true;
  int imageWidth = 0;
  int imageHeight = 0;
  bool stickyBottom = false;
  double stickyLastUpper = 0.0;
  double stickyLastPage = 0.0;
  std::string lastItemsKey;
  std::string accentClass;
  std::string videoSource;
  std::string roundClass;
  std::vector<GtkWidget *> inlinePics;
  size_t dropdownCount = 0;
  const void *textureSource = nullptr;
  std::string registeredId;

  std::vector<NativeNode *> kids;
  ChildContext ctx;
  std::vector<GtkWidget *> owned;
};

struct PendingScroll {
  NativeScrollRequest request;
  std::chrono::steady_clock::time_point requestedAt = std::chrono::steady_clock::now();
};

constexpr std::chrono::milliseconds kScrollSettleTime{50};
constexpr std::chrono::milliseconds kScrollRequestTimeout{2000};

class Gtk4TreeBackend final : public Backend, public tree::TreeOps {
public:
  ~Gtk4TreeBackend() override { shutdown(); }

  bool initialize(int width, int height, std::string_view title) override {
    gtk_init();
#ifdef N8V_HAVE_ADWAITA
    adw_init();
#endif
    self_ = this;

    window_ = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(window_), std::string(title).c_str());
    gtk_window_set_default_size(GTK_WINDOW(window_), width, height);
    g_signal_connect_data(window_, "close-request", G_CALLBACK(&Gtk4TreeBackend::onCloseRequest), this, nullptr, (GConnectFlags)0);

    rootBox_ = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_hexpand(rootBox_, TRUE);
    gtk_widget_set_vexpand(rootBox_, TRUE);
    gtk_window_set_child(GTK_WINDOW(window_), rootBox_);

    root_.outer = rootBox_;
    root_.content = rootBox_;

    GtkGesture *clickAway = gtk_gesture_click_new();
    gtk_event_controller_set_propagation_phase(GTK_EVENT_CONTROLLER(clickAway), GTK_PHASE_CAPTURE);
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(clickAway), 0);
    g_signal_connect_data(clickAway, "pressed", G_CALLBACK(&Gtk4TreeBackend::onClickAway), nullptr, nullptr, (GConnectFlags)0);
    gtk_widget_add_controller(window_, GTK_EVENT_CONTROLLER(clickAway));
    GtkEventController *escapeKey = gtk_event_controller_key_new();
    gtk_event_controller_set_propagation_phase(escapeKey, GTK_PHASE_CAPTURE);
    g_signal_connect_data(escapeKey, "key-pressed", G_CALLBACK(&Gtk4TreeBackend::onEscapeKey), nullptr, nullptr, (GConnectFlags)0);
    gtk_widget_add_controller(window_, escapeKey);

    cardCss_ = gtk_css_provider_new();
    gtk_css_provider_load_from_string(
      cardCss_,
      ".n8v-card { border: 1px solid alpha(currentColor, 0.2); border-radius: 6px; }\n"
      ".n8v-badge { background: alpha(currentColor, 0.18); border-radius: 999px; padding: 0 7px; font-size: smaller; font-weight: bold; min-height: 16px; }\n"
#ifdef N8V_HAVE_ADWAITA
      ".n8v-badge-red { background: @error_bg_color; color: @error_fg_color; }\n"
      ".n8v-badge-orange { background: @orange_3; color: white; }\n"
      ".n8v-badge-yellow { background: @warning_bg_color; color: @warning_fg_color; }\n"
      ".n8v-badge-green { background: @success_bg_color; color: @success_fg_color; }\n"
      ".n8v-badge-blue { background: @accent_bg_color; color: @accent_fg_color; }\n"
      ".n8v-badge-purple { background: @purple_3; color: white; }\n"
      ".n8v-badge-pink { background: mix(@purple_3, @red_3, 0.5); color: white; }\n"
#else
      ".n8v-badge-red, .n8v-badge-orange, .n8v-badge-green, .n8v-badge-blue, .n8v-badge-purple, .n8v-badge-pink { color: white; }\n"
      ".n8v-badge-yellow { color: rgba(0, 0, 0, 0.8); }\n"
      ".n8v-badge-red { background: #e01b24; }\n"
      ".n8v-badge-orange { background: #e66100; }\n"
      ".n8v-badge-yellow { background: #f5c211; }\n"
      ".n8v-badge-green { background: #26a269; }\n"
      ".n8v-badge-blue { background: #3584e4; }\n"
      ".n8v-badge-purple { background: #9141ac; }\n"
      ".n8v-badge-pink { background: #d6428f; }\n"
#endif
      ".n8v-sidebar-decoration, .n8v-sidebar-decoration:hover, .n8v-sidebar-decoration:selected { background: none; box-shadow: none; }\n"
      ".n8v-sidebar-divider { min-height: 0; padding: 0; }\n"
      ".n8v-notify-bar { background: currentColor; border-radius: 2px; min-width: 3px; min-height: 16px; }\n"
      ".n8v-notify-label, .n8v-notify-label label { font-weight: bold; }\n"
    );
    gtk_style_context_add_provider_for_display(gtk_widget_get_display(window_), GTK_STYLE_PROVIDER(cardCss_), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    GtkCssProvider *namedCss = gtk_css_provider_new();
    gtk_css_provider_load_from_string(namedCss, namedColorCss().c_str());
    gtk_style_context_add_provider_for_display(gtk_widget_get_display(window_), GTK_STYLE_PROVIDER(namedCss), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 1);
    g_object_unref(namedCss);

    multilineCss_ = gtk_css_provider_new();
    gtk_css_provider_load_from_string(
      multilineCss_,
      "scrolledwindow.n8v-multiline-scroll, textview.n8v-multiline-text, textview.n8v-multiline-text text { background: none; background-color: transparent; border: none; "
      "box-shadow: none; }\n"
    );
    gtk_style_context_add_provider_for_display(gtk_widget_get_display(window_), GTK_STYLE_PROVIDER(multilineCss_), GTK_STYLE_PROVIDER_PRIORITY_USER + 100);

    gtk_window_present(GTK_WINDOW(window_));
    return true;
  }

  bool pumpEvents() override {
    GMainContext *context = g_main_context_default();
    while (g_main_context_pending(context)) g_main_context_iteration(context, FALSE);
    return !closeRequested_;
  }

  bool pointerDown() const override { return false; }
  Clay_Dimensions windowSize() const override { return {(float)gtk_widget_get_width(window_), (float)gtk_widget_get_height(window_)}; }
  Clay_Dimensions measureText(std::string_view, FontFamily, uint16_t, bool, bool) const override { return {0, 0}; }

  bool rendersFromClay() const override { return false; }
  bool rendersNativeChrome() const override { return true; }
  tree::TreeOps *treeOps() override { return this; }
  void beginFrame() override {}
  void present(Clay_RenderCommandArray) override {}
  void setCursor(CursorKind) override {}

  void commitFrame() override {
    updateMultilineHeights();
    applyStickyBottom();
    resolvePendingScrolls();
    resolvePendingFocus();
    if (++frames_ == 30 && std::getenv("N8V_GTK_DUMP")) dumpWidget(window_, 0);
  }

  bool isHovered(std::string_view id) const override { return hovered_.count(std::string(id)) != 0; }

  bool visibleChildren(std::string_view containerId, float margin, const std::function<void(std::string_view, float, float)> &visit) const override {
    auto container = byId_.find(std::string(containerId));
    if (container == byId_.end() || !container->second->scrolled || !container->second->content) return false;
    const NativeNode &c = *container->second;
    GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(c.scrolled));
    const double viewTop = gtk_adjustment_get_value(vadj);
    const double viewHeight = gtk_adjustment_get_page_size(vadj);
    if (viewHeight <= 0.0) return false;
    for (const NativeNode *kid : c.kids) {
      if (kid->floating || !kid->outer) continue;
      const char *id = tree::idOf(kid->node->current.props);
      if (!id || !*id) continue;
      graphene_point_t origin = GRAPHENE_POINT_INIT(0, 0), inContent;
      if (!gtk_widget_compute_point(kid->outer, c.content, &origin, &inContent)) continue;
      const double top = (double)inContent.y - viewTop;
      const double height = gtk_widget_get_height(kid->outer);
      if (top + height >= -margin && top <= viewHeight + margin) visit(id, (float)top, (float)height);
    }
    return true;
  }

  void requestScroll(const NativeScrollRequest &request) override { pendingScrolls_.push_back(PendingScroll{request, std::chrono::steady_clock::now()}); }

  void requestFocus(std::string_view id) override { pendingFocus_.push_back(PendingFocus{std::string(id), 0}); }

#if GTK_CHECK_VERSION(4, 10, 0)
  bool showMessageDialog(const MessageDialogRequest &request, MessageDialogDone done) override {
    if (!window_) return false;
    const char *heading = request.title.empty() ? request.message.c_str() : request.title.c_str();
    const char *body = request.title.empty() ? nullptr : request.message.c_str();
#if defined(N8V_HAVE_ADWAITA) && ADW_CHECK_VERSION(1, 5, 0)
    AdwDialog *dialog = adw_alert_dialog_new(heading, body);
    for (size_t i = 0; i < request.buttons.size(); ++i) {
      const std::string id = std::to_string(i);
      adw_alert_dialog_add_response(ADW_ALERT_DIALOG(dialog), id.c_str(), request.buttons[i].label.c_str());
      if (request.buttons[i].destructive) adw_alert_dialog_set_response_appearance(ADW_ALERT_DIALOG(dialog), id.c_str(), ADW_RESPONSE_DESTRUCTIVE);
    }
    adw_alert_dialog_set_default_response(ADW_ALERT_DIALOG(dialog), "0");
    adw_alert_dialog_set_close_response(ADW_ALERT_DIALOG(dialog), "dismissed");
    adw_alert_dialog_choose(ADW_ALERT_DIALOG(dialog), window_, nullptr, &Gtk4TreeBackend::onAdwMessageDialogDone, new MessageDialogDone(std::move(done)));
    return true;
#else
    GtkAlertDialog *dialog = gtk_alert_dialog_new("%s", heading);
    if (body) gtk_alert_dialog_set_detail(dialog, body);
    gtk_alert_dialog_set_modal(dialog, TRUE);

    std::vector<const char *> labels;
    for (const MessageDialogButton &b : request.buttons) labels.push_back(b.label.c_str());
    labels.push_back(nullptr);
    gtk_alert_dialog_set_buttons(dialog, labels.data());
    gtk_alert_dialog_set_default_button(dialog, 0);
    gtk_alert_dialog_set_cancel_button(dialog, -1);

    gtk_alert_dialog_choose(dialog, GTK_WINDOW(window_), nullptr, &Gtk4TreeBackend::onMessageDialogDone, new MessageDialogDone(std::move(done)));
    g_object_unref(dialog);
    return true;
#endif
  }

#if defined(N8V_HAVE_ADWAITA) && ADW_CHECK_VERSION(1, 5, 0)
  static void onAdwMessageDialogDone(GObject *source, GAsyncResult *res, gpointer data) {
    std::unique_ptr<MessageDialogDone> done(static_cast<MessageDialogDone *>(data));
    const char *response = adw_alert_dialog_choose_finish(ADW_ALERT_DIALOG(source), res);
    char *end = nullptr;
    long button = response ? std::strtol(response, &end, 10) : -1;
    (*done)(response &&end != response && *end == '\0' ? static_cast<int>(button) : -1);
  }
#endif

  static void onMessageDialogDone(GObject *source, GAsyncResult *res, gpointer data) {
    std::unique_ptr<MessageDialogDone> done(static_cast<MessageDialogDone *>(data));
    GError *error = nullptr;
    int button = gtk_alert_dialog_choose_finish(GTK_ALERT_DIALOG(source), res, &error);
    if (error) {
      g_error_free(error);
      button = -1;
    }
    (*done)(button);
  }

  bool showFileDialog(const FileDialogRequest &request, FileDialogDone done) override {
    if (!window_) return false;
    GtkFileDialog *dialog = gtk_file_dialog_new();
    const char *defaultTitle = request.kind == N8V_FILE_DIALOG_SAVE_FILE ? "Save File" : request.kind == N8V_FILE_DIALOG_OPEN_FOLDER ? "Select Folder" : "Open File";
    gtk_file_dialog_set_title(dialog, request.title.empty() ? defaultTitle : request.title.c_str());
    gtk_file_dialog_set_modal(dialog, TRUE);

    if (!request.filters.empty() && request.kind != N8V_FILE_DIALOG_OPEN_FOLDER) {
      GListStore *filters = g_list_store_new(gtk_file_filter_get_type());
      for (size_t i = 0; i < request.filters.size(); ++i) {
        GtkFileFilter *filter = gtk_file_filter_new();
        gtk_file_filter_set_name(filter, request.filters[i].name.c_str());
        for (const std::string &glob : request.filters[i].patterns) {
          const std::string ext = globExtension(glob);
          if (!ext.empty()) {
            gtk_file_filter_add_suffix(filter, ext.c_str());
          } else {
            gtk_file_filter_add_pattern(filter, glob.c_str());
          }
        }
        g_list_store_append(filters, filter);
        if (i == 0) gtk_file_dialog_set_default_filter(dialog, filter);
        g_object_unref(filter);
      }
      gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(filters));
      g_object_unref(filters);
    }
    if (!request.initialDir.empty()) {
      GFile *folder = g_file_new_for_path(request.initialDir.c_str());
      gtk_file_dialog_set_initial_folder(dialog, folder);
      g_object_unref(folder);
    }
    if (request.kind == N8V_FILE_DIALOG_SAVE_FILE && !request.defaultName.empty()) gtk_file_dialog_set_initial_name(dialog, request.defaultName.c_str());

    auto *ctx = new FileDialogCtx{request.kind, request.multiple, std::move(done)};
    GtkWindow *parent = GTK_WINDOW(window_);
    switch (request.kind) {
    case N8V_FILE_DIALOG_OPEN_FILE:
      if (request.multiple) {
        gtk_file_dialog_open_multiple(dialog, parent, nullptr, &Gtk4TreeBackend::onFileDialogDone, ctx);
      } else {
        gtk_file_dialog_open(dialog, parent, nullptr, &Gtk4TreeBackend::onFileDialogDone, ctx);
      }
      break;
    case N8V_FILE_DIALOG_SAVE_FILE:
      gtk_file_dialog_save(dialog, parent, nullptr, &Gtk4TreeBackend::onFileDialogDone, ctx);
      break;
    case N8V_FILE_DIALOG_OPEN_FOLDER:
      if (request.multiple) {
        gtk_file_dialog_select_multiple_folders(dialog, parent, nullptr, &Gtk4TreeBackend::onFileDialogDone, ctx);
      } else {
        gtk_file_dialog_select_folder(dialog, parent, nullptr, &Gtk4TreeBackend::onFileDialogDone, ctx);
      }
      break;
    }
    g_object_unref(dialog);
    return true;
  }
#endif

  void shutdown() override {
    if (!window_) return;
    gtk_window_destroy(GTK_WINDOW(window_));
    window_ = nullptr;
    rootBox_ = nullptr;
    self_ = nullptr;
  }

  void create(PNode &node) override {
    auto *nn = new NativeNode;
    nn->node = &node;
    node.handle = nn;
    live_.insert(&node);
    build(*nn);
    g_object_ref_sink(nn->outer);
    nn->owned.push_back(nn->outer);
    if (nn->kind() == Kind::Page) {
      g_object_ref_sink(nn->content);
      nn->owned.push_back(nn->content);
    }
    g_object_set_data(G_OBJECT(nn->outer), "n8v-node", nn);
    apply(*nn);
    registerId(*nn);
  }

  void update(PNode &node, const OwnedProps &, unsigned) override {
    NativeNode &nn = *native(node);
    apply(nn);
    registerId(nn);
    if (nn.node->parent && nn.kind() != Kind::Page && !isSidebarDecoration(nn.kind())) layoutChild(nn);
    if (!nn.kids.empty() || nn.kind() == Kind::Flex || nn.kind() == Kind::Panel) {
      for (NativeNode *kid : nn.kids) refreshChild(nn, *kid);
      updateSpacers(nn);
    }
    if (nn.kind() == Kind::Sidebar) {
      for (NativeNode *kid : nn.kids)
        if (kid->kind() == Kind::Page) apply(*kid);
      syncSidebarSelection(nn);
    }
  }

  void insert(PNode &parent, PNode &child, size_t index) override {
    NativeNode &p = nativeOrRoot(parent);
    NativeNode &c = *native(child);
    p.kids.insert(p.kids.begin() + (ptrdiff_t)index, &c);
    attach(p, c, index);
    refreshChild(p, c);
    updateSpacers(p);
    afterKidsChanged(p);
  }

  void move(PNode &parent, PNode &child, size_t index) override {
    NativeNode &p = nativeOrRoot(parent);
    NativeNode &c = *native(child);
    p.kids.erase(std::find(p.kids.begin(), p.kids.end(), &c));
    p.kids.insert(p.kids.begin() + (ptrdiff_t)index, &c);
    if (p.kind() == Kind::Sidebar && (c.kind() == Kind::Page || isSidebarDecoration(c.kind()))) {
      g_object_ref(c.outer);
      gtk_list_box_remove(GTK_LIST_BOX(p.listBox), c.outer);
      gtk_list_box_insert(GTK_LIST_BOX(p.listBox), c.outer, (int)rowIndex(p, c));
      g_object_unref(c.outer);
    } else if (!c.floating) {
      gtk_box_reorder_child_after(GTK_BOX(p.content), c.outer, siblingBefore(p, index));
    }
    updateSpacers(p);
    afterKidsChanged(p);
  }

  void remove(PNode &parent, PNode &child) override {
    NativeNode &p = nativeOrRoot(parent);
    NativeNode &c = *native(child);
    detach(p, c);
    p.kids.erase(std::find(p.kids.begin(), p.kids.end(), &c));
    updateSpacers(p);
    afterKidsChanged(p);
  }

  void destroy(PNode &node) override {
    NativeNode *nn = native(node);
    if (!nn) return;
    unregisterId(*nn);
    live_.erase(&node);
    if (nn->kind() == Kind::Radio && nn->widget) gtk_check_button_set_group(GTK_CHECK_BUTTON(nn->widget), nullptr);
    for (GtkWidget *w : nn->owned) g_object_unref(w);
    delete nn;
    node.handle = nullptr;
  }

private:
  using Kind = NativeKind;

  /** Sections and dividers are rows of the sidebar's list like pages are, but have no content and can't be selected. */
  static bool isSidebarDecoration(Kind kind) { return kind == Kind::SidebarSection || kind == Kind::SidebarDivider; }

  NativeNode *native(PNode &node) const { return static_cast<NativeNode *>(node.handle); }
  NativeNode &nativeOrRoot(PNode &node) { return node.parent ? *native(node) : root_; }

  static PNode *liveNode(gpointer userData) {
    auto *node = static_cast<PNode *>(userData);
    return self_ && self_->live_.count(node) ? node : nullptr;
  }

  template <typename Fn> void connect(gpointer instance, const char *signal, Fn handler, PNode &node) {
    g_signal_connect_data(instance, signal, G_CALLBACK(handler), &node, nullptr, (GConnectFlags)0);
  }

  void build(NativeNode &nn);
  void apply(NativeNode &nn);

  void attach(NativeNode &parent, NativeNode &child, size_t index);
  void detach(NativeNode &parent, NativeNode &child);
  GtkWidget *siblingBefore(NativeNode &parent, size_t index);
  size_t pageIndex(NativeNode &sidebar, NativeNode &page);
  size_t rowIndex(NativeNode &sidebar, NativeNode &child);
  void refreshChild(NativeNode &parent, NativeNode &child);
  void layoutChild(NativeNode &child);
  void updateSpacers(NativeNode &container);
  void afterKidsChanged(NativeNode &container);
  void syncSidebarSelection(NativeNode &sidebar);
  void ensureOverlay(NativeNode &host);
  void swapOuter(NativeNode &nn, GtkWidget *replacement);
  void setScrolled(NativeNode &nn, bool wantScrolled, bool horizontal, bool vertical);
  void registerId(NativeNode &nn);
  void unregisterId(NativeNode &nn);
  void watchHover(NativeNode &nn);
  void resolvePendingScrolls();
  void resolvePendingFocus();
  void applyStickyBottom();
  void setIconImage(NativeNode &nn, GtkWidget *image, const char *name, int pixelSize);
  void applyVideo(NativeNode &nn, const n8v_video_options &o);
  void applyAudio(NativeNode &nn, const n8v_audio_options &o);
  void setLabelFromText(GtkWidget *label, const n8v_text_options &o, std::string_view text);

  void applyFlex(NativeNode &nn, const n8v_flex_options &o);
  void applyPanel(NativeNode &nn, const n8v_panel_options &o);
  void applyButton(NativeNode &nn, const n8v_button_options &o);
  void applyMultilineEntry(NativeNode &nn, const n8v_entry_options &o);
  void updateMultilineHeight(NativeNode &nn);
  void updateMultilineHeights();
  void applySidebar(NativeNode &nn, const n8v_sidebar_options &o);
  void applyPage(NativeNode &nn, const n8v_page_options &o);

  static gboolean onCloseRequest(GtkWindow *, gpointer userData) {
    static_cast<Gtk4TreeBackend *>(userData)->closeRequested_ = true;
    return TRUE;
  }

  static void onClickAway(GtkGestureClick *gesture, int, double x, double y, gpointer) {
    if (!self_) return;
    GtkWidget *picked = gtk_widget_pick(self_->window_, x, y, GTK_PICK_DEFAULT);
    self_->dismissFloating(picked);
    (void)gesture;
  }
  static gboolean onEscapeKey(GtkEventControllerKey *, guint keyval, guint, GdkModifierType, gpointer) {
    if (keyval == GDK_KEY_Escape && self_) self_->dismissFloating(nullptr);
    return FALSE;
  }
  void dismissFloating(GtkWidget *picked) {
    std::vector<PNode *> dismissed;
    for (PNode *pn : live_) {
      const auto *o = propsOf<n8v_floating_options>(*pn);
      if (!o || !o->on_dismiss) continue;
      NativeNode *nn = native(*pn);
      if (!nn || !nn->outer) continue;
      if (picked) {
        auto inside = [&](GtkWidget *w) { return w && (picked == w || gtk_widget_is_ancestor(picked, w)); };
        NativeNode *host = pn->parent ? native(*pn->parent) : nullptr;
        if (inside(nn->outer) || (host && inside(host->outer))) continue;
      }
      dismissed.push_back(pn);
    }
    for (PNode *pn : dismissed)
      if (live_.count(pn)) dispatch::dismiss(*pn);
  }
  static void onButtonClicked(GtkButton *, gpointer userData) {
    if (PNode *node = liveNode(userData)) dispatch::activate(*node);
  }
  static gboolean onLinkActivated(GtkLabel *, const char *, gpointer userData) {
    if (PNode *node = liveNode(userData)) dispatch::activate(*node);
    return TRUE;
  }
  static gboolean onDocumentLinkActivated(GtkLabel *, const char *uri, gpointer) {
    if (uri) openUrl(uri);
    return TRUE;
  }
  static void onCheckToggled(GtkCheckButton *button, gpointer userData) {
    PNode *node = liveNode(userData);
    if (!node || self_->updating_) return;
    if (std::holds_alternative<n8v_radio_options>(node->current.props)) {
      if (gtk_check_button_get_active(button)) dispatch::select(*node, 0);
    } else {
      dispatch::setChecked(*node, gtk_check_button_get_active(button));
    }
  }
  static void onSwitchActive(GObject *object, GParamSpec *, gpointer userData) {
    PNode *node = liveNode(userData);
    if (node && !self_->updating_) dispatch::setChecked(*node, gtk_switch_get_active(GTK_SWITCH(object)));
  }
  static void onEntryChanged(GtkEditable *editable, gpointer userData) {
    PNode *node = liveNode(userData);
    if (node && !self_->updating_) dispatch::setText(*node, gtk_editable_get_text(editable));
  }
  static void onEntryActivate(GtkEntry *, gpointer userData) {
    if (PNode *node = liveNode(userData)) dispatch::submit(*node);
  }
  static std::string textBufferText(GtkTextBuffer *buffer) {
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    char *raw = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
    std::string text = raw ? raw : "";
    g_free(raw);
    return text;
  }
  static constexpr GtkStateFlags kFocusFlags = (GtkStateFlags)(GTK_STATE_FLAG_FOCUSED | GTK_STATE_FLAG_FOCUS_WITHIN | GTK_STATE_FLAG_FOCUS_VISIBLE);
  static void onMultilineFocusEnter(GtkEventControllerFocus *, gpointer backdrop) { gtk_widget_set_state_flags(GTK_WIDGET(backdrop), kFocusFlags, FALSE); }
  static void onMultilineFocusLeave(GtkEventControllerFocus *, gpointer backdrop) { gtk_widget_unset_state_flags(GTK_WIDGET(backdrop), kFocusFlags); }
  static void onTextBufferChanged(GtkTextBuffer *buffer, gpointer userData) {
    PNode *node = liveNode(userData);
    if (node && !self_->updating_) dispatch::setText(*node, textBufferText(buffer));
  }
  static void onTextBufferPlaceholder(GtkTextBuffer *buffer, gpointer placeholder) {
    gtk_widget_set_visible(GTK_WIDGET(placeholder), gtk_text_buffer_get_char_count(buffer) == 0);
  }
  static void onClipboardTexture(GObject *source, GAsyncResult *result, gpointer userData) {
    GError *error = nullptr;
    GdkTexture *texture = gdk_clipboard_read_texture_finish(GDK_CLIPBOARD(source), result, &error);
    if (error) g_error_free(error);
    if (!texture) return;
    const std::string path = newPastedImagePath();
    const bool saved = gdk_texture_save_to_png(texture, path.c_str());
    g_object_unref(texture);
    PNode *node = liveNode(userData);
    if (saved && node) dispatch::pasteFiles(*node, {path});
  }
  static void onClipboardFiles(GObject *source, GAsyncResult *result, gpointer userData) {
    GError *error = nullptr;
    const GValue *value = gdk_clipboard_read_value_finish(GDK_CLIPBOARD(source), result, &error);
    if (error) g_error_free(error);
    if (!value) return;
    std::vector<std::string> paths;
    if (auto *list = static_cast<GdkFileList *>(g_value_get_boxed(value))) {
      for (GSList *item = gdk_file_list_get_files(list); item; item = item->next) {
        char *path = g_file_get_path(G_FILE(item->data));
        if (path) {
          paths.emplace_back(path);
          g_free(path);
        }
      }
    }
    PNode *node = liveNode(userData);
    if (node && !paths.empty()) dispatch::pasteFiles(*node, paths);
  }
  static void onMultilinePaste(GtkTextView *view, gpointer userData) {
    PNode *node = liveNode(userData);
    const auto *o = node ? propsOf<n8v_entry_options>(*node) : nullptr;
    if (!o || !o->on_paste_files) return;
    GdkClipboard *clipboard = gtk_widget_get_clipboard(GTK_WIDGET(view));
    GdkContentFormats *formats = gdk_clipboard_get_formats(clipboard);
    if (gdk_content_formats_contain_gtype(formats, GDK_TYPE_FILE_LIST)) {
      g_signal_stop_emission_by_name(view, "paste-clipboard");
      gdk_clipboard_read_value_async(clipboard, GDK_TYPE_FILE_LIST, G_PRIORITY_DEFAULT, nullptr, &Gtk4TreeBackend::onClipboardFiles, node);
    } else if (!gdk_content_formats_contain_gtype(formats, G_TYPE_STRING) && gdk_content_formats_contain_gtype(formats, GDK_TYPE_TEXTURE)) {
      g_signal_stop_emission_by_name(view, "paste-clipboard");
      gdk_clipboard_read_texture_async(clipboard, nullptr, &Gtk4TreeBackend::onClipboardTexture, node);
    }
  }
  static gboolean onMultilineKey(GtkEventControllerKey *, guint keyval, guint, GdkModifierType state, gpointer userData) {
    PNode *node = liveNode(userData);
    if (!node) return FALSE;
    int key = -1;
    if (keyval == GDK_KEY_Up || keyval == GDK_KEY_KP_Up) key = N8V_KEY_UP;
    else if (keyval == GDK_KEY_Down || keyval == GDK_KEY_KP_Down) key = N8V_KEY_DOWN;
    else if (keyval == GDK_KEY_Escape) key = N8V_KEY_ESCAPE;
    if (key >= 0) {
      const unsigned mods = ((state & GDK_SHIFT_MASK) ? N8V_MOD_SHIFT : 0u) | ((state & GDK_CONTROL_MASK) ? N8V_MOD_CTRL : 0u) | ((state & GDK_ALT_MASK) ? N8V_MOD_ALT : 0u);
      return dispatch::key(*node, (n8v_key)key, mods) ? TRUE : FALSE;
    }
    if (keyval != GDK_KEY_Return && keyval != GDK_KEY_KP_Enter && keyval != GDK_KEY_ISO_Enter) return FALSE;
    const auto *o = node ? propsOf<n8v_entry_options>(*node) : nullptr;
    if (!o || !enterSubmits(o->submit_mode, (state & GDK_CONTROL_MASK) != 0, (state & GDK_SHIFT_MASK) != 0)) return FALSE;
    dispatch::submit(*node);
    return TRUE;
  }
  static void onDropdownSelected(GObject *object, GParamSpec *, gpointer userData) {
    PNode *node = liveNode(userData);
    if (!node || self_->updating_) return;
    guint selected = gtk_drop_down_get_selected(GTK_DROP_DOWN(object));
    const auto *o = propsOf<n8v_dropdown_options>(*node);
    const guint offset = o && o->placeholder && *o->placeholder ? 1 : 0;
    if (selected != GTK_INVALID_LIST_POSITION && selected >= offset) dispatch::select(*node, (int)(selected - offset));
  }
  static void onSliderChanged(GtkRange *range, gpointer userData) {
    PNode *node = liveNode(userData);
    if (node && !self_->updating_) dispatch::setSlider(*node, (float)gtk_range_get_value(range));
  }
  static void onRowActivated(GtkListBox *, GtkListBoxRow *row, gpointer) {
    auto *nn = static_cast<NativeNode *>(g_object_get_data(G_OBJECT(row), "n8v-node"));
    if (nn && self_ && self_->live_.count(nn->node)) dispatch::selectPage(*nn->node);
  }
  static void onMotionEnter(GtkEventControllerMotion *, gdouble, gdouble, gpointer userData) { self_->setHover(static_cast<PNode *>(userData), true); }
  static void onMotionLeave(GtkEventControllerMotion *, gpointer userData) { self_->setHover(static_cast<PNode *>(userData), false); }
  static gboolean onOverlayChildPosition(GtkOverlay *overlay, GtkWidget *child, GdkRectangle *allocation, gpointer);
  static gboolean onInlineImagePosition(GtkOverlay *overlay, GtkWidget *child, GdkRectangle *allocation, gpointer);
  void applyDocument(NativeNode &nn, const n8v_document_options &o);

  static void dumpWidget(GtkWidget *w, int depth) {
    graphene_rect_t bounds = GRAPHENE_RECT_INIT(0, 0, 0, 0);
    GtkWidget *root = GTK_WIDGET(gtk_widget_get_root(w));
    if (root) gtk_widget_compute_bounds(w, root, &bounds);
    std::fprintf(
      stderr,
      "%*s%s @%.0f,%.0f %dx%d%s%s\n",
      depth * 2,
      "",
      gtk_widget_get_css_name(w),
      bounds.origin.x,
      bounds.origin.y,
      gtk_widget_get_width(w),
      gtk_widget_get_height(w),
      gtk_widget_compute_expand(w, GTK_ORIENTATION_HORIZONTAL) ? " hexpand" : "",
      gtk_widget_compute_expand(w, GTK_ORIENTATION_VERTICAL) ? " vexpand" : ""
    );
    int reqW = 0, reqH = 0;
    gtk_widget_get_size_request(w, &reqW, &reqH);
    if (gtk_widget_get_vexpand_set(w) || reqH >= 0 || gtk_widget_get_valign(w) != GTK_ALIGN_FILL)
      std::fprintf(stderr, "%*s  [vexpand=%d valign=%d reqH=%d]\n", depth * 2, "", gtk_widget_get_vexpand(w), (int)gtk_widget_get_valign(w), reqH);
    for (GtkWidget *child = gtk_widget_get_first_child(w); child; child = gtk_widget_get_next_sibling(child)) dumpWidget(child, depth + 1);
  }

  void setHover(PNode *raw, bool hovered) {
    PNode *node = liveNode(raw);
    if (!node) return;
    if (const char *id = tree::idOf(node->current.props)) {
      if (hovered) hovered_.insert(id);
      else hovered_.erase(id);
    }
    dispatch::hover(*node, hovered);
  }

#if GTK_CHECK_VERSION(4, 10, 0)
  struct FileDialogCtx {
    n8v_file_dialog_kind kind;
    bool multiple;
    FileDialogDone done;
  };

  static void onFileDialogDone(GObject *source, GAsyncResult *res, gpointer data) {
    std::unique_ptr<FileDialogCtx> ctx(static_cast<FileDialogCtx *>(data));
    GtkFileDialog *dialog = GTK_FILE_DIALOG(source);
    GError *error = nullptr;
    FileDialogResult result;

    auto addFile = [&](GFile *file) {
      if (!file) return;
      if (char *path = g_file_get_path(file)) {
        result.paths.emplace_back(path);
        g_free(path);
      }
    };
    auto addAll = [&](GListModel *list) {
      if (!list) return;
      for (guint i = 0, n = g_list_model_get_n_items(list); i < n; ++i) {
        GFile *file = G_FILE(g_list_model_get_item(list, i));
        addFile(file);
        if (file) g_object_unref(file);
      }
      g_object_unref(list);
    };
    auto single = [&](GFile *file) {
      addFile(file);
      if (file) g_object_unref(file);
    };

    switch (ctx->kind) {
    case N8V_FILE_DIALOG_OPEN_FILE:
      if (ctx->multiple) addAll(gtk_file_dialog_open_multiple_finish(dialog, res, &error));
      else single(gtk_file_dialog_open_finish(dialog, res, &error));
      break;
    case N8V_FILE_DIALOG_SAVE_FILE:
      single(gtk_file_dialog_save_finish(dialog, res, &error));
      break;
    case N8V_FILE_DIALOG_OPEN_FOLDER:
      if (ctx->multiple) addAll(gtk_file_dialog_select_multiple_folders_finish(dialog, res, &error));
      else single(gtk_file_dialog_select_folder_finish(dialog, res, &error));
      break;
    }
    if (error) g_error_free(error);
    result.status = result.paths.empty() ? FileDialogResult::Status::Cancelled : FileDialogResult::Status::Done;
    ctx->done(std::move(result));
  }
#endif

  GtkWidget *window_ = nullptr;
  GtkWidget *rootBox_ = nullptr;
  GtkCssProvider *cardCss_ = nullptr;
  GtkCssProvider *multilineCss_ = nullptr;
  NativeNode root_;
  std::unordered_set<PNode *> live_;
  std::unordered_map<std::string, NativeNode *> byId_;
  struct PendingFocus {
    std::string id;
    int framesWaited;
  };
  std::vector<PendingFocus> pendingFocus_;
  std::unordered_set<std::string> hovered_;
  std::vector<PendingScroll> pendingScrolls_;
  bool closeRequested_ = false;
  bool updating_ = false;
  int lineHeight_ = 0;
  int frames_ = 0;

  bool warnedPercent_ = false;
  bool warnedMax_ = false;

  static inline Gtk4TreeBackend *self_ = nullptr;

  struct Guard {
    bool previous;
    Guard() : previous(self_->updating_) { self_->updating_ = true; }
    ~Guard() { self_->updating_ = previous; }
  };
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
      [&](const n8v_image_options &o) {
        n8v_sizing s = horizontal ? o.width : o.height;
        if (s.mode == N8V_SIZING_FIT && s.min <= 0.0f && s.max <= 0.0f) return n8v_sizing_fixed((float)(horizontal ? nn.imageWidth : nn.imageHeight));
        return s;
      },
      [&](const n8v_video_options &o) {
        n8v_sizing s = horizontal ? o.width : o.height;
        if (s.mode == N8V_SIZING_FIT && s.min <= 0.0f && s.max <= 0.0f) return n8v_sizing_fixed(horizontal ? 480.0f : 270.0f);
        return s;
      },
      [&](const n8v_audio_options &o) {
        if (!horizontal) return fitSizing();
        if (o.width.mode == N8V_SIZING_FIT && o.width.min <= 0.0f && o.width.max <= 0.0f) return n8v_sizing_fixed(320.0f);
        return o.width;
      },
      [&](const n8v_divider_options &) { return horizontal ? growSizing() : fitSizing(); },
      [&](const n8v_sidebar_options &) { return growSizing(); },
      [&](const n8v_entry_options &) { return horizontal ? growSizing() : fitSizing(); },
      [&](const n8v_dropdown_options &) { return horizontal ? growSizing() : fitSizing(); },
      [&](const n8v_slider_options &) { return horizontal ? growSizing() : fitSizing(); },
      [&](const auto &) { return fitSizing(); },
    },
    nn.node->current.props
  );
}

struct Axis {
  GtkAlign align = GTK_ALIGN_START;
  bool expand = false;
  int request = -1;
};

Axis axisFor(const n8v_sizing &s, bool mainAxis, n8v_align alignment, int paddingSum, bool &warnedPercent, bool &warnedMax) {
  Axis a;
  a.align = mainAxis ? GTK_ALIGN_START : toGtkAlign(alignment);
  switch (s.mode) {
  case N8V_SIZING_FIT:
    if (s.min > 0.0f) a.request = (int)s.min;
    break;
  case N8V_SIZING_GROW:
    a.expand = true;
    a.align = GTK_ALIGN_FILL;
    if (s.min > 0.0f) a.request = (int)s.min;
    break;
  case N8V_SIZING_FIXED:
    a.request = std::max(0, (int)s.value - paddingSum);
    break;
  case N8V_SIZING_PERCENT:
    warnOnce(warnedPercent, "Sizing::percent is not supported by the GTK tree backend yet; treating it as grow");
    a.expand = true;
    a.align = GTK_ALIGN_FILL;
    break;
  }
  if (s.max > 0.0f) warnOnce(warnedMax, "Sizing max is not supported by the GTK tree backend yet; ignoring it");
  return a;
}

ChildContext contextFor(const NativeNode &parent) {
  ChildContext c;
  if (!parent.node) return c;
  std::visit(
    Overloaded{
      [&](const n8v_flex_options &o) {
        c.orientation = o.direction == N8V_DIRECTION_HORIZONTAL ? GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL;
        c.hAlign = o.h_align;
        c.vAlign = o.v_align;
      },
      [&](const n8v_panel_options &o) {
        c.orientation = o.direction == N8V_DIRECTION_HORIZONTAL ? GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL;
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

GdkTexture *makeTexture(const DecodedImage *image) {
  gsize size = (gsize)image->width * (gsize)image->height * 4;
  GBytes *bytes = g_bytes_new(image->rgba, size);
  GdkTexture *texture = gdk_memory_texture_new(image->width, image->height, GDK_MEMORY_R8G8B8A8, bytes, (gsize)image->width * 4);
  g_bytes_unref(bytes);
  return texture;
}

void replaceInParent(GtkWidget *old, GtkWidget *replacement) {
  GtkWidget *parent = gtk_widget_get_parent(old);
  if (!parent) return;
  g_object_ref(old);
  if (GTK_IS_BOX(parent)) {
    GtkWidget *previous = gtk_widget_get_prev_sibling(old);
    gtk_box_remove(GTK_BOX(parent), old);
    gtk_box_insert_child_after(GTK_BOX(parent), replacement, previous);
  } else if (GTK_IS_WINDOW(parent)) {
    gtk_window_set_child(GTK_WINDOW(parent), replacement);
  } else if (GTK_IS_OVERLAY(parent)) {
    if (gtk_overlay_get_child(GTK_OVERLAY(parent)) == old) {
      gtk_overlay_set_child(GTK_OVERLAY(parent), replacement);
    } else {
      gtk_overlay_remove_overlay(GTK_OVERLAY(parent), old);
      gtk_overlay_add_overlay(GTK_OVERLAY(parent), replacement);
    }
  }
  g_object_unref(old);
}

void Gtk4TreeBackend::swapOuter(NativeNode &nn, GtkWidget *replacement) {
  GtkWidget *old = nn.outer;
  g_object_ref(old);
  replaceInParent(old, replacement);
  nn.outer = replacement;
  g_object_ref_sink(replacement);
  nn.owned.push_back(replacement);
  g_object_set_data(G_OBJECT(replacement), "n8v-node", &nn);
  g_object_unref(old);
}

void Gtk4TreeBackend::ensureOverlay(NativeNode &host) {
  if (host.overlay) return;
  GtkWidget *inner = host.outer;
  GtkWidget *overlay = gtk_overlay_new();
  swapOuter(host, overlay);
  gtk_overlay_set_child(GTK_OVERLAY(overlay), inner);
  gtk_widget_set_hexpand(inner, TRUE);
  gtk_widget_set_vexpand(inner, TRUE);
  gtk_widget_set_halign(inner, GTK_ALIGN_FILL);
  gtk_widget_set_valign(inner, GTK_ALIGN_FILL);
  gtk_widget_set_size_request(inner, -1, -1);
  host.overlay = overlay;
  g_signal_connect_data(overlay, "get-child-position", G_CALLBACK(&Gtk4TreeBackend::onOverlayChildPosition), nullptr, nullptr, (GConnectFlags)0);
  if (host.node) layoutChild(host);
  else {
    gtk_widget_set_hexpand(overlay, TRUE);
    gtk_widget_set_vexpand(overlay, TRUE);
  }
}

gboolean Gtk4TreeBackend::onOverlayChildPosition(GtkOverlay *overlay, GtkWidget *child, GdkRectangle *allocation, gpointer) {
  auto *nn = static_cast<NativeNode *>(g_object_get_data(G_OBJECT(child), "n8v-node"));
  if (!nn || !nn->floating) return FALSE;
  const auto *o = propsOf<n8v_floating_options>(*nn->node);
  if (!o) return FALSE;

  const int parentW = gtk_widget_get_width(GTK_WIDGET(overlay));
  const int parentH = gtk_widget_get_height(GTK_WIDGET(overlay));

  int minW = 0, natW = 0;
  gtk_widget_measure(child, GTK_ORIENTATION_HORIZONTAL, -1, &minW, &natW, nullptr, nullptr);
  int w = o->width.mode == N8V_SIZING_FIXED     ? (int)o->width.value
          : o->width.mode == N8V_SIZING_GROW    ? parentW
          : o->width.mode == N8V_SIZING_PERCENT ? (int)(o->width.value * parentW)
                                                : natW;
  if (o->width.mode == N8V_SIZING_GROW) {
    if (o->width.max > 0.0f) w = std::min(w, (int)o->width.max);
    if (o->width.min > 0.0f) w = std::max(w, (int)o->width.min);
  }
  int minH = 0, natH = 0;
  gtk_widget_measure(child, GTK_ORIENTATION_VERTICAL, w, &minH, &natH, nullptr, nullptr);
  int h = o->height.mode == N8V_SIZING_FIXED     ? (int)o->height.value
          : o->height.mode == N8V_SIZING_GROW    ? parentH
          : o->height.mode == N8V_SIZING_PERCENT ? (int)(o->height.value * parentH)
                                                 : natH;

  double pfx = 0, pfy = 0, efx = 0, efy = 0;
  attachFraction(o->parent_attach_point, pfx, pfy);
  attachFraction(o->element_attach_point, efx, efy);
  allocation->x = (int)(pfx * parentW - efx * w + o->offset_x);
  allocation->y = (int)(pfy * parentH - efy * h + o->offset_y);
  if (o->keep_on_screen) {
    GtkWidget *window = GTK_WIDGET(gtk_widget_get_root(GTK_WIDGET(overlay)));
    graphene_point_t origin{0, 0}, inWindow{0, 0};
    if (window && gtk_widget_compute_point(GTK_WIDGET(overlay), window, &origin, &inWindow)) {
      const int windowW = gtk_widget_get_width(window), windowH = gtk_widget_get_height(window);
      const int margin = (int)o->screen_margin;
      const int x = std::max(margin, std::min((int)inWindow.x + allocation->x, windowW - w - margin));
      const int y = std::max(margin, std::min((int)inWindow.y + allocation->y, windowH - h - margin));
      allocation->x = x - (int)inWindow.x;
      allocation->y = y - (int)inWindow.y;
    }
  }
  allocation->width = w;
  allocation->height = h;
  return TRUE;
}

gboolean Gtk4TreeBackend::onInlineImagePosition(GtkOverlay *overlay, GtkWidget *child, GdkRectangle *allocation, gpointer) {
  if (!g_object_get_data(G_OBJECT(child), "n8v-inline-pic")) return FALSE;
  GtkWidget *label = gtk_overlay_get_child(overlay);
  if (!label || !GTK_IS_LABEL(label)) return FALSE;

  PangoRectangle spot{};
  pango_layout_index_to_pos(gtk_label_get_layout(GTK_LABEL(label)), GPOINTER_TO_INT(g_object_get_data(G_OBJECT(child), "n8v-inline-index")), &spot);
  int offsetX = 0, offsetY = 0;
  gtk_label_get_layout_offsets(GTK_LABEL(label), &offsetX, &offsetY);
  const int w = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(child), "n8v-inline-width"));
  const int h = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(child), "n8v-inline-height"));
  allocation->x = offsetX + PANGO_PIXELS(spot.x);
  allocation->y = offsetY + PANGO_PIXELS(spot.y + spot.height / 2) - h / 2;
  allocation->width = w;
  allocation->height = h;
  return TRUE;
}

void Gtk4TreeBackend::applyDocument(NativeNode &nn, const n8v_document_options &o) {
  GtkLabel *label = GTK_LABEL(nn.widget);
  const markdown::Document doc = markdown::parse(o.markdown ? o.markdown : "");

  markup::LinkStyler styler;
  if (o.link_style) {
    styler = [&o](const std::string &url, n8v_link_style &style) { return o.link_style(url.c_str(), &style, o.link_style_userdata); };
  }

  PangoFontMetrics *metrics = pango_context_get_metrics(gtk_widget_get_pango_context(nn.widget), nullptr, nullptr);
  const int lineHeight = (pango_font_metrics_get_ascent(metrics) + pango_font_metrics_get_descent(metrics)) / PANGO_SCALE;
  const int charWidth = std::max(1, std::max(pango_font_metrics_get_approximate_char_width(metrics), pango_font_metrics_get_approximate_digit_width(metrics)) / PANGO_SCALE);
  pango_font_metrics_unref(metrics);

  if (o.width.mode == N8V_SIZING_FIXED && o.width.value > 0.0f) {
    const int chars = std::max(1, (int)(o.width.value / (float)charWidth));
    gtk_label_set_width_chars(label, chars);
    gtk_label_set_max_width_chars(label, chars);
  } else {
    gtk_label_set_width_chars(label, -1);
    gtk_label_set_max_width_chars(label, -1);
  }
  struct Placed {
    const DecodedImage *image;
    int width, height;
  };
  std::vector<Placed> placed;
  markup::ImagePlacer placer = [&](const markdown::InlineRun &run) {
    const std::string path = markdown::lookupImage(o.image_state, run.imageUrl);
    if (path.empty()) return false;
    n8v_image_options imageOptions{};
    imageOptions.source_kind = N8V_IMAGE_SOURCE_PATH;
    imageOptions.path = path.c_str();
    const DecodedImage *image = getOrDecodeImage(imageOptions);
    if (!image || image->width <= 0 || image->height <= 0) return false;
    const int height = run.imageSize > 0 ? run.imageSize : std::max(8, (int)(lineHeight * 1.15f));
    placed.push_back({image, std::max(1, (int)std::lround((double)image->width * height / image->height)), height});
    return true;
  };

  markup::MarkupHooks hooks;
  hooks.links = o.link_style ? &styler : nullptr;
  hooks.images = o.image_resolver ? &placer : nullptr;
  const std::string markupText = markup::documentToPangoMarkup(doc, hooks);

  const std::string key = markupText + '\x01' + (o.image_state ? o.image_state : "");
  if (key != nn.lastItemsKey) {
    nn.lastItemsKey = key;
    gtk_label_set_markup(label, markupText.c_str());

    for (GtkWidget *picture : nn.inlinePics) gtk_overlay_remove_overlay(GTK_OVERLAY(nn.outer), picture);
    nn.inlinePics.clear();

    const char *text = gtk_label_get_text(label);
    PangoAttrList *attributes = pango_attr_list_new();
    size_t next = 0;
    for (const char *at = text; (at = strstr(at, "\xEF\xBF\xBC")) && next < placed.size(); at += 3, ++next) {
      const int index = (int)(at - text);
      const Placed &image = placed[next];
      PangoRectangle logical{0, -(int)(image.height * 0.85 * PANGO_SCALE), image.width * PANGO_SCALE, image.height * PANGO_SCALE};
      PangoAttribute *shape = pango_attr_shape_new(&logical, &logical);
      shape->start_index = (guint)index;
      shape->end_index = (guint)index + 3;
      pango_attr_list_insert(attributes, shape);

      GdkTexture *texture = makeTexture(image.image);
      GtkWidget *picture = gtk_picture_new_for_paintable(GDK_PAINTABLE(texture));
      g_object_unref(texture);
      gtk_picture_set_can_shrink(GTK_PICTURE(picture), FALSE);
      gtk_widget_set_size_request(picture, image.width, image.height);
      gtk_widget_set_can_target(picture, FALSE);
      g_object_set_data(G_OBJECT(picture), "n8v-inline-pic", GINT_TO_POINTER(1));
      g_object_set_data(G_OBJECT(picture), "n8v-inline-index", GINT_TO_POINTER(index));
      g_object_set_data(G_OBJECT(picture), "n8v-inline-width", GINT_TO_POINTER(image.width));
      g_object_set_data(G_OBJECT(picture), "n8v-inline-height", GINT_TO_POINTER(image.height));
      gtk_overlay_add_overlay(GTK_OVERLAY(nn.outer), picture);
      nn.inlinePics.push_back(picture);
    }
    gtk_label_set_attributes(label, attributes);
    pango_attr_list_unref(attributes);
  }
  gtk_widget_set_can_target(nn.widget, markup::documentHasLinks(doc) ? TRUE : FALSE);
}

GtkWidget *Gtk4TreeBackend::siblingBefore(NativeNode &parent, size_t index) {
  for (size_t i = index; i-- > 0;) {
    if (!parent.kids[i]->floating) return parent.kids[i]->outer;
  }
  return parent.startSpacer;
}

size_t Gtk4TreeBackend::pageIndex(NativeNode &sidebar, NativeNode &page) {
  size_t index = 0;
  for (NativeNode *kid : sidebar.kids) {
    if (kid == &page) break;
    if (kid->kind() == Kind::Page) ++index;
  }
  return index;
}

size_t Gtk4TreeBackend::rowIndex(NativeNode &sidebar, NativeNode &child) {
  size_t index = 0;
  for (NativeNode *kid : sidebar.kids) {
    if (kid == &child) break;
    if (kid->kind() == Kind::Page || isSidebarDecoration(kid->kind())) ++index;
  }
  return index;
}

void Gtk4TreeBackend::attach(NativeNode &parent, NativeNode &child, size_t index) {
  if (parent.kind() == Kind::Sidebar && child.kind() == Kind::Page) {
    gtk_list_box_insert(GTK_LIST_BOX(parent.listBox), child.outer, (int)rowIndex(parent, child));
    gtk_box_append(GTK_BOX(parent.contentArea), child.content);
    return;
  }
  if (parent.kind() == Kind::Sidebar && isSidebarDecoration(child.kind())) {
    gtk_list_box_insert(GTK_LIST_BOX(parent.listBox), child.outer, (int)rowIndex(parent, child));
    return;
  }
  if (child.floating && parent.kind() != Kind::Page) {
    ensureOverlay(parent);
    gtk_overlay_add_overlay(GTK_OVERLAY(parent.overlay), child.outer);
    return;
  }
  GtkWidget *container = parent.kind() == Kind::Sidebar ? parent.contentArea : parent.content;
  gtk_box_insert_child_after(GTK_BOX(container), child.outer, siblingBefore(parent, index));
}

void Gtk4TreeBackend::detach(NativeNode &parent, NativeNode &child) {
  if (parent.kind() == Kind::Sidebar && child.kind() == Kind::Page) {
    gtk_list_box_remove(GTK_LIST_BOX(parent.listBox), child.outer);
    gtk_box_remove(GTK_BOX(parent.contentArea), child.content);
    return;
  }
  if (parent.kind() == Kind::Sidebar && isSidebarDecoration(child.kind())) {
    gtk_list_box_remove(GTK_LIST_BOX(parent.listBox), child.outer);
    return;
  }
  if (child.floating && parent.overlay) {
    gtk_overlay_remove_overlay(GTK_OVERLAY(parent.overlay), child.outer);
    return;
  }
  GtkWidget *container = parent.kind() == Kind::Sidebar ? parent.contentArea : parent.content;
  gtk_box_remove(GTK_BOX(container), child.outer);
}

void Gtk4TreeBackend::refreshChild(NativeNode &parent, NativeNode &child) {
  child.ctx = contextFor(parent);
  if (child.kind() != Kind::Page && !isSidebarDecoration(child.kind())) layoutChild(child);
}

void Gtk4TreeBackend::layoutChild(NativeNode &c) {
  if (!c.node || !c.outer) return;
  const bool horizontalMain = c.ctx.orientation == GTK_ORIENTATION_HORIZONTAL;
  int padW = 0, padH = 0;
  if (const auto *flex = propsOf<n8v_flex_options>(*c.node)) {
    if (!c.scrolled && !c.overlay) padW = flex->padding.left + flex->padding.right, padH = flex->padding.top + flex->padding.bottom;
  }
  const Axis ax = axisFor(sizingFor(c, true), horizontalMain, c.ctx.hAlign, padW, warnedPercent_, warnedMax_);
  const Axis ay = axisFor(sizingFor(c, false), !horizontalMain, c.ctx.vAlign, padH, warnedPercent_, warnedMax_);

  GtkWidget *w = c.outer;
  gtk_widget_set_hexpand(w, ax.expand);
  gtk_widget_set_halign(w, ax.align);
  gtk_widget_set_vexpand(w, ay.expand);
  gtk_widget_set_valign(w, ay.align);
  gtk_widget_set_size_request(w, ax.request, ay.request);

  if (c.scrolled) {
    gtk_scrolled_window_set_propagate_natural_width(GTK_SCROLLED_WINDOW(c.scrolled), sizingFor(c, true).mode == N8V_SIZING_FIT);
    gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(c.scrolled), sizingFor(c, false).mode == N8V_SIZING_FIT);
  }
}

void Gtk4TreeBackend::updateSpacers(NativeNode &container) {
  const Kind kind = container.kind();
  if (kind != Kind::Flex && kind != Kind::Panel) return;
  const ChildContext cc = contextFor(container);
  const bool horizontal = cc.orientation == GTK_ORIENTATION_HORIZONTAL;
  const n8v_align mainAlign = horizontal ? cc.hAlign : cc.vAlign;

  bool anyGrow = false;
  for (NativeNode *kid : container.kids) {
    if (kid->floating) continue;
    const n8v_sizing s = sizingFor(*kid, horizontal);
    if (s.mode == N8V_SIZING_GROW || s.mode == N8V_SIZING_PERCENT) anyGrow = true;
  }
  const bool wantStart = !anyGrow && (mainAlign == N8V_ALIGN_CENTER || mainAlign == N8V_ALIGN_END);
  const bool wantEnd = !anyGrow && mainAlign == N8V_ALIGN_CENTER;

  GtkBox *box = GTK_BOX(container.content);
  auto makeSpacer = [&] {
    GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    if (horizontal) gtk_widget_set_hexpand(spacer, TRUE);
    else gtk_widget_set_vexpand(spacer, TRUE);
    return spacer;
  };
  if (wantStart && !container.startSpacer) {
    container.startSpacer = makeSpacer();
    gtk_box_insert_child_after(box, container.startSpacer, nullptr);
  } else if (!wantStart && container.startSpacer) {
    gtk_box_remove(box, container.startSpacer);
    container.startSpacer = nullptr;
  }
  if (wantEnd && !container.endSpacer) {
    container.endSpacer = makeSpacer();
    gtk_box_append(box, container.endSpacer);
  } else if (!wantEnd && container.endSpacer) {
    gtk_box_remove(box, container.endSpacer);
    container.endSpacer = nullptr;
  }
  if (container.endSpacer) {
    GtkWidget *last = container.startSpacer;
    for (NativeNode *kid : container.kids)
      if (!kid->floating) last = kid->outer;
    gtk_box_reorder_child_after(box, container.endSpacer, last);
  }
}

void Gtk4TreeBackend::afterKidsChanged(NativeNode &container) {
  if (container.kind() == Kind::Sidebar) {
    for (NativeNode *kid : container.kids)
      if (kid->kind() == Kind::Page) gtk_widget_set_visible(kid->content, !kid->kids.empty());
    syncSidebarSelection(container);
  } else if (container.kind() == Kind::Page) {
    gtk_widget_set_visible(container.content, !container.kids.empty());
  }
}

void Gtk4TreeBackend::syncSidebarSelection(NativeNode &sidebar) {
  if (!sidebar.listBox) return;
  int selected = 0;
  if (const int *bound = std::get_if<int>(&sidebar.node->bound)) selected = *bound;
  int pageNumber = 0;
  for (NativeNode *kid : sidebar.kids) {
    if (kid->kind() != Kind::Page) continue;
    if (pageNumber++ == selected) {
      gtk_list_box_select_row(GTK_LIST_BOX(sidebar.listBox), GTK_LIST_BOX_ROW(kid->outer));
      break;
    }
  }
  for (NativeNode *kid : sidebar.kids)
    if (kid->kind() == Kind::Page) gtk_widget_set_visible(kid->content, !kid->kids.empty());
}

void Gtk4TreeBackend::setScrolled(NativeNode &nn, bool want, bool horizontal, bool vertical) {
  if (want && !nn.scrolled) {
    GtkWidget *scrolled = gtk_scrolled_window_new();
    GtkWidget *inner = nn.outer;
    g_object_ref(inner);
    swapOuter(nn, scrolled);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), inner);
    g_object_unref(inner);
    gtk_widget_set_hexpand(inner, FALSE);
    gtk_widget_set_vexpand(inner, FALSE);
    gtk_widget_set_halign(inner, GTK_ALIGN_FILL);
    gtk_widget_set_valign(inner, GTK_ALIGN_FILL);
    gtk_widget_set_size_request(inner, -1, -1);
    nn.scrolled = scrolled;
  } else if (!want && nn.scrolled) {
    GtkWidget *scrolled = nn.scrolled;
    GtkWidget *inner = nn.content;
    g_object_ref(inner);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), nullptr);
    swapOuter(nn, inner);
    g_object_unref(inner);
    nn.scrolled = nullptr;
  }
  if (nn.scrolled) {
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(nn.scrolled), horizontal ? GTK_POLICY_AUTOMATIC : GTK_POLICY_NEVER, vertical ? GTK_POLICY_AUTOMATIC : GTK_POLICY_NEVER);
  }
}

void Gtk4TreeBackend::unregisterId(NativeNode &nn) {
  if (nn.registeredId.empty()) return;
  auto it = byId_.find(nn.registeredId);
  if (it != byId_.end() && it->second == &nn) byId_.erase(it);
  hovered_.erase(nn.registeredId);
  nn.registeredId.clear();
}

void Gtk4TreeBackend::registerId(NativeNode &nn) {
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
  if ((!wanted.empty() || (flex && flex->on_hover)) && !nn.hoverWatched) watchHover(nn);
}

void Gtk4TreeBackend::watchHover(NativeNode &nn) {
  nn.hoverWatched = true;
  GtkEventController *motion = gtk_event_controller_motion_new();
  g_signal_connect_data(motion, "enter", G_CALLBACK(&Gtk4TreeBackend::onMotionEnter), nn.node, nullptr, (GConnectFlags)0);
  g_signal_connect_data(motion, "leave", G_CALLBACK(&Gtk4TreeBackend::onMotionLeave), nn.node, nullptr, (GConnectFlags)0);
  gtk_widget_add_controller(nn.outer, motion);
}

void Gtk4TreeBackend::applyStickyBottom() {
  for (auto &[id, nn] : byId_) {
    if (!nn->stickyBottom || !nn->scrolled) continue;
    GtkAdjustment *adj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(nn->scrolled));
    const double upper = gtk_adjustment_get_upper(adj), page = gtk_adjustment_get_page_size(adj);
    if (upper != nn->stickyLastUpper || page != nn->stickyLastPage) {
      if (nn->stickyAtBottom) gtk_adjustment_set_value(adj, std::max(upper - page, 0.0));
      nn->stickyLastUpper = upper;
      nn->stickyLastPage = page;
    }
    nn->stickyAtBottom = gtk_adjustment_get_value(adj) + page >= upper - kStickyBottomEpsilonPx;
  }
}

void Gtk4TreeBackend::resolvePendingFocus() {
  for (auto it = pendingFocus_.begin(); it != pendingFocus_.end();) {
    auto found = byId_.find(it->id);
    if (found != byId_.end() && found->second->widget) {
      gtk_widget_grab_focus(found->second->widget);
      it = pendingFocus_.erase(it);
    } else if (++it->framesWaited > 10) {
      it = pendingFocus_.erase(it); // never appeared
    } else {
      ++it;
    }
  }
}

void Gtk4TreeBackend::resolvePendingScrolls() {
  const auto now = std::chrono::steady_clock::now();
  for (auto it = pendingScrolls_.begin(); it != pendingScrolls_.end();) {
    const NativeScrollRequest &req = it->request;
    if (now - it->requestedAt < kScrollSettleTime) {
      ++it;
      continue;
    }
    bool resolved = false;
    auto container = byId_.find(req.containerId);
    if (container != byId_.end() && container->second->scrolled) {
      NativeNode &c = *container->second;
      GtkAdjustment *hadj = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(c.scrolled));
      GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(c.scrolled));
      const double maxX = std::max(gtk_adjustment_get_upper(hadj) - gtk_adjustment_get_page_size(hadj), 0.0);
      const double maxY = std::max(gtk_adjustment_get_upper(vadj) - gtk_adjustment_get_page_size(vadj), 0.0);
      const bool laidOut = gtk_adjustment_get_page_size(vadj) > 0.0;
      if (laidOut) {
        resolved = true;
        switch (req.kind) {
        case NativeScrollRequest::Kind::Bottom:
          gtk_adjustment_set_value(vadj, maxY);
          break;
        case NativeScrollRequest::Kind::Top:
          gtk_adjustment_set_value(vadj, 0.0);
          break;
        case NativeScrollRequest::Kind::Left:
          gtk_adjustment_set_value(hadj, 0.0);
          break;
        case NativeScrollRequest::Kind::Right:
          gtk_adjustment_set_value(hadj, maxX);
          break;
        case NativeScrollRequest::Kind::Offset:
          gtk_adjustment_set_value(hadj, std::clamp((double)req.x, 0.0, maxX));
          gtk_adjustment_set_value(vadj, std::clamp((double)req.y, 0.0, maxY));
          break;
        case NativeScrollRequest::Kind::Element: {
          auto element = byId_.find(req.elementId);
          graphene_point_t origin = GRAPHENE_POINT_INIT(0, 0), inContent;
          if (element == byId_.end() || !gtk_widget_compute_point(element->second->outer, c.content, &origin, &inContent)) {
            resolved = false;
            break;
          }
          gtk_adjustment_set_value(vadj, std::clamp((double)inContent.y - req.paddingTop, 0.0, maxY));
          gtk_adjustment_set_value(hadj, std::clamp((double)inContent.x - req.paddingLeft, 0.0, maxX));
          break;
        }
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

void setStreamMuted(GtkMediaStream *stream, bool muted) {
  g_object_set_data(G_OBJECT(stream), "n8v-muted", GINT_TO_POINTER(muted ? 1 : 0));
  if (gtk_media_stream_is_prepared(stream)) {
    gtk_media_stream_set_muted(stream, muted);
  } else if (!g_object_get_data(G_OBJECT(stream), "n8v-mute-hooked")) {
    g_object_set_data(G_OBJECT(stream), "n8v-mute-hooked", GINT_TO_POINTER(1));
    g_signal_connect_data(
      stream,
      "notify::prepared",
      G_CALLBACK(+[](GObject *object, GParamSpec *, gpointer) {
        GtkMediaStream *s = GTK_MEDIA_STREAM(object);
        if (gtk_media_stream_is_prepared(s)) gtk_media_stream_set_muted(s, GPOINTER_TO_INT(g_object_get_data(object, "n8v-muted")) != 0);
      }),
      nullptr,
      nullptr,
      (GConnectFlags)0
    );
  }
}

void Gtk4TreeBackend::applyAudio(NativeNode &nn, const n8v_audio_options &o) {
  const std::string source = o.source ? o.source : "";
  if (source != nn.videoSource) {
    nn.videoSource = source;
    GtkMediaStream *stream = nullptr;
    if (!source.empty()) {
      GFile *file = source.find("://") != std::string::npos ? g_file_new_for_uri(source.c_str()) : g_file_new_for_path(source.c_str());
      stream = gtk_media_file_new_for_file(file);
      g_object_unref(file);
    }
    gtk_media_controls_set_media_stream(GTK_MEDIA_CONTROLS(nn.widget), stream);
    // The controls only borrow the stream, so tie its life to the widget's.
    g_object_set_data_full(G_OBJECT(nn.widget), "n8v-audio-stream", stream, g_object_unref);
  }

  GtkMediaStream *stream = GTK_MEDIA_STREAM(g_object_get_data(G_OBJECT(nn.widget), "n8v-audio-stream"));
  if (stream) {
    gtk_media_stream_set_loop(stream, o.loop);
    setStreamMuted(stream, o.muted);
    if (o.autoplay && !g_object_get_data(G_OBJECT(stream), "n8v-autoplayed")) {
      g_object_set_data(G_OBJECT(stream), "n8v-autoplayed", GINT_TO_POINTER(1));
      gtk_media_stream_play(stream);
    }
  }
  gtk_widget_set_size_request(nn.widget, o.width.mode == N8V_SIZING_FIXED ? std::max(1, (int)o.width.value) : 320, -1);
}

void Gtk4TreeBackend::applyVideo(NativeNode &nn, const n8v_video_options &o) {
  const std::string source = o.source ? o.source : "";
  const bool withControls = GTK_IS_VIDEO(nn.widget);

  if (source != nn.videoSource) {
    nn.videoSource = source;
    GFile *file = source.empty() ? nullptr : source.find("://") != std::string::npos ? g_file_new_for_uri(source.c_str()) : g_file_new_for_path(source.c_str());
    if (withControls) {
      gtk_video_set_file(GTK_VIDEO(nn.widget), file);
    } else {
      GtkMediaStream *stream = file ? gtk_media_file_new_for_file(file) : nullptr;
      gtk_picture_set_paintable(GTK_PICTURE(nn.widget), stream ? GDK_PAINTABLE(stream) : nullptr);
      g_object_set_data_full(G_OBJECT(nn.widget), "n8v-video-stream", stream, g_object_unref);
    }
    if (file) g_object_unref(file);
  }

  GtkMediaStream *stream = withControls ? gtk_video_get_media_stream(GTK_VIDEO(nn.widget)) : GTK_MEDIA_STREAM(g_object_get_data(G_OBJECT(nn.widget), "n8v-video-stream"));
  if (withControls) gtk_video_set_loop(GTK_VIDEO(nn.widget), o.loop);
  else if (stream) gtk_media_stream_set_loop(stream, o.loop);
  if (stream) {
    if (o.autoplay && !g_object_get_data(G_OBJECT(stream), "n8v-autoplayed")) {
      g_object_set_data(G_OBJECT(stream), "n8v-autoplayed", GINT_TO_POINTER(1));
      gtk_media_stream_play(stream);
    }
    setStreamMuted(stream, o.muted);
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
  std::string wanted;
  if (radius.topLeft > 0 || radius.topRight > 0 || radius.bottomLeft > 0 || radius.bottomRight > 0) {
    char name[64];
    std::snprintf(name, sizeof(name), "n8v-round-%d-%d-%d-%d", (int)radius.topLeft, (int)radius.topRight, (int)radius.bottomRight, (int)radius.bottomLeft);
    wanted = name;
    static std::unordered_set<std::string> made;
    if (made.insert(wanted).second) {
      char rule[160];
      std::snprintf(
        rule,
        sizeof(rule),
        ".%s { border-radius: %dpx %dpx %dpx %dpx; }",
        name,
        (int)radius.topLeft,
        (int)radius.topRight,
        (int)radius.bottomRight,
        (int)radius.bottomLeft
      );
      GtkCssProvider *provider = gtk_css_provider_new();
      gtk_css_provider_load_from_string(provider, rule);
      gtk_style_context_add_provider_for_display(gtk_widget_get_display(nn.outer), GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
      g_object_unref(provider);
    }
  }
  if (wanted != nn.roundClass) {
    if (!nn.roundClass.empty()) gtk_widget_remove_css_class(nn.outer, nn.roundClass.c_str());
    if (!wanted.empty()) gtk_widget_add_css_class(nn.outer, wanted.c_str());
    nn.roundClass = wanted;
  }

  gtk_widget_set_size_request(
    nn.widget,
    o.width.mode == N8V_SIZING_FIXED ? std::max(1, (int)o.width.value) : 480,
    o.height.mode == N8V_SIZING_FIXED ? std::max(1, (int)o.height.value) : 270
  );
}

void Gtk4TreeBackend::setIconImage(NativeNode &nn, GtkWidget *image, const char *name, int pixelSize) {
  if (!name || !*name) return;
  gtk_image_set_pixel_size(GTK_IMAGE(image), pixelSize);
  const char *freedesktopName = resolveFreedesktopIconName(name);
  GtkIconTheme *theme = gtk_icon_theme_get_for_display(gtk_widget_get_display(window_));
  if (freedesktopName && gtk_icon_theme_has_icon(theme, freedesktopName)) {
    gtk_image_set_from_icon_name(GTK_IMAGE(image), freedesktopName);
    nn.textureSource = nullptr;
    return;
  }
  GdkRGBA fg;
  gtk_widget_get_color(window_, &fg);
  const DecodedImage *icon = getOrDecodeIcon(name, (uint16_t)pixelSize, n8v::Color{fg.red * 255.0f, fg.green * 255.0f, fg.blue * 255.0f, 255.0f});
  if (!icon || nn.textureSource == icon) return;
  GdkTexture *texture = makeTexture(icon);
  gtk_image_set_from_paintable(GTK_IMAGE(image), GDK_PAINTABLE(texture));
  g_object_unref(texture);
  nn.textureSource = icon;
}

void Gtk4TreeBackend::setLabelFromText(GtkWidget *label, const n8v_text_options &o, std::string_view text) {
  std::string m = markup::escapeMarkup(text);
  if (o.strikethrough) m = "<s>" + m + "</s>";
  if (o.italic) m = "<i>" + m + "</i>";
  if (o.bold) m = "<b>" + m + "</b>";
  if (o.small) m = "<span size=\"smaller\">" + m + "</span>";
  if (o.large) m = "<span size=\"260%\">" + m + "</span>";
  if (o.darken) gtk_widget_add_css_class(label, "dim-label");
  else gtk_widget_remove_css_class(label, "dim-label");
  for (const NamedColorCss &c : kNamedColorCss) gtk_widget_remove_css_class(label, (std::string("n8v-fg-") + c.slug).c_str());
  if (const std::string cls = themeClass("n8v-fg-", o.color); !cls.empty()) gtk_widget_add_css_class(label, cls.c_str());
  const bool isDefaultColor = o.color.a <= 0.0f || (o.color.r == 0.0f && o.color.g == 0.0f && o.color.b == 0.0f && o.color.a == 255.0f);
  if (!isDefaultColor) {
    char open[128];
    std::snprintf(open, sizeof open, "<span foreground=\"#%02x%02x%02x\" alpha=\"%d%%\">", (int)o.color.r, (int)o.color.g, (int)o.color.b, (int)(o.color.a / 255.0f * 100.0f));
    m = open + m + "</span>";
  }
  const bool link = (o.url && *o.url) || o.on_click;
  if (link) m = "<a href=\"" + markup::escapeMarkup(o.url && *o.url ? o.url : "about:blank") + "\">" + m + "</a>";
  gtk_label_set_markup(GTK_LABEL(label), m.c_str());
}

void Gtk4TreeBackend::build(NativeNode &nn) {
  PNode &pn = *nn.node;
  switch (nn.kind()) {
  case Kind::Flex: {
    nn.outer = nn.content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    break;
  }
  case Kind::Floating: {
    nn.outer = nn.content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    nn.floating = true;
    break;
  }
  case Kind::Panel: {
    nn.outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    nn.content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_hexpand(nn.content, TRUE);
    gtk_widget_set_vexpand(nn.content, TRUE);
    gtk_box_append(GTK_BOX(nn.outer), nn.content);
    break;
  }
  case Kind::Text: {
    nn.outer = nn.widget = gtk_label_new("");
    gtk_label_set_use_markup(GTK_LABEL(nn.widget), TRUE);
    gtk_label_set_wrap(GTK_LABEL(nn.widget), TRUE);
    gtk_label_set_wrap_mode(GTK_LABEL(nn.widget), PANGO_WRAP_WORD);
    gtk_label_set_natural_wrap_mode(GTK_LABEL(nn.widget), GTK_NATURAL_WRAP_NONE);
    gtk_label_set_xalign(GTK_LABEL(nn.widget), 0.0f);
    connect(nn.widget, "activate-link", &Gtk4TreeBackend::onLinkActivated, pn);
    break;
  }
  case Kind::Button: {
    const auto *buttonProps = propsOf<n8v_button_options>(pn);
    nn.outer = nn.widget = buttonProps && buttonProps->checked ? gtk_toggle_button_new() : gtk_button_new();
    connect(nn.widget, "clicked", &Gtk4TreeBackend::onButtonClicked, pn);
    break;
  }
  case Kind::Checkbox: {
    nn.outer = nn.widget = gtk_check_button_new();
    connect(nn.widget, "toggled", &Gtk4TreeBackend::onCheckToggled, pn);
    break;
  }
  case Kind::Radio: {
    nn.outer = nn.widget = gtk_check_button_new();
    const auto *mine = propsOf<n8v_radio_options>(pn);
    for (PNode *other : live_) {
      if (other == &pn || !other->handle) continue;
      const auto *theirs = propsOf<n8v_radio_options>(*other);
      if (theirs && mine && theirs->selected == mine->selected && native(*other)->widget) {
        gtk_check_button_set_group(GTK_CHECK_BUTTON(nn.widget), GTK_CHECK_BUTTON(native(*other)->widget));
        break;
      }
    }
    connect(nn.widget, "toggled", &Gtk4TreeBackend::onCheckToggled, pn);
    break;
  }
  case Kind::Toggle: {
    nn.outer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    nn.widget = gtk_switch_new();
    nn.label = gtk_label_new("");
    gtk_widget_set_valign(nn.widget, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(nn.outer), nn.widget);
    gtk_box_append(GTK_BOX(nn.outer), nn.label);
    connect(nn.widget, "notify::active", &Gtk4TreeBackend::onSwitchActive, pn);
    break;
  }
  case Kind::Entry: {
    const auto *entry = propsOf<n8v_entry_options>(pn);
    if (entry && entry->multiline) {
      const bool matchStyle = entry->match_entry_style;
      nn.widget = gtk_text_view_new();
      gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(nn.widget), GTK_WRAP_WORD_CHAR);
      gtk_text_view_set_left_margin(GTK_TEXT_VIEW(nn.widget), kMultilineSideMarginPx);
      gtk_text_view_set_right_margin(GTK_TEXT_VIEW(nn.widget), kMultilineSideMarginPx);
      if (!matchStyle) {
        gtk_text_view_set_top_margin(GTK_TEXT_VIEW(nn.widget), kMultilineFrameMarginPx);
        gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(nn.widget), kMultilineFrameMarginPx);
      }

      nn.scrolled = gtk_scrolled_window_new();
      gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(nn.scrolled), GTK_POLICY_NEVER, GTK_POLICY_EXTERNAL);
      gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(nn.scrolled), TRUE);
      gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(nn.scrolled), nn.widget);

      nn.label = gtk_label_new("");
      gtk_widget_set_halign(nn.label, GTK_ALIGN_START);
      gtk_widget_set_valign(nn.label, GTK_ALIGN_START);
      gtk_widget_set_can_target(nn.label, FALSE);
      gtk_widget_add_css_class(nn.label, "dim-label");

      nn.outer = gtk_overlay_new();
      if (matchStyle) {
        gtk_widget_add_css_class(nn.widget, "n8v-multiline-text");
        gtk_widget_add_css_class(nn.scrolled, "n8v-multiline-scroll");
        gtk_scrolled_window_set_has_frame(GTK_SCROLLED_WINDOW(nn.scrolled), FALSE);
        gtk_widget_set_margin_top(nn.scrolled, kMultilineEntryMarginPx);
        gtk_widget_set_margin_bottom(nn.scrolled, kMultilineEntryMarginPx);
        gtk_widget_set_margin_start(nn.label, kMultilineSideMarginPx);
        gtk_widget_set_margin_top(nn.label, kMultilineEntryMarginPx);

        nn.backdrop = gtk_entry_new();
        gtk_widget_set_can_target(nn.backdrop, FALSE);
        gtk_widget_set_focusable(nn.backdrop, FALSE);
        gtk_widget_set_vexpand(nn.backdrop, TRUE);

        gtk_overlay_set_child(GTK_OVERLAY(nn.outer), nn.backdrop);
        gtk_overlay_add_overlay(GTK_OVERLAY(nn.outer), nn.scrolled);
        gtk_overlay_set_measure_overlay(GTK_OVERLAY(nn.outer), nn.scrolled, TRUE);

        GtkEventController *focus = gtk_event_controller_focus_new();
        g_signal_connect_data(focus, "enter", G_CALLBACK(&Gtk4TreeBackend::onMultilineFocusEnter), nn.backdrop, nullptr, (GConnectFlags)0);
        g_signal_connect_data(focus, "leave", G_CALLBACK(&Gtk4TreeBackend::onMultilineFocusLeave), nn.backdrop, nullptr, (GConnectFlags)0);
        gtk_widget_add_controller(nn.widget, focus);
      } else {
        gtk_scrolled_window_set_has_frame(GTK_SCROLLED_WINDOW(nn.scrolled), TRUE);
        gtk_widget_set_margin_start(nn.label, kMultilineSideMarginPx + 1);
        gtk_widget_set_margin_top(nn.label, kMultilineFrameMarginPx + 1);
        gtk_overlay_set_child(GTK_OVERLAY(nn.outer), nn.scrolled);
      }
      gtk_overlay_add_overlay(GTK_OVERLAY(nn.outer), nn.label);

      GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(nn.widget));
      connect(buffer, "changed", &Gtk4TreeBackend::onTextBufferChanged, pn);
      g_signal_connect_data(buffer, "changed", G_CALLBACK(&Gtk4TreeBackend::onTextBufferPlaceholder), nn.label, nullptr, (GConnectFlags)0);
      GtkEventController *keys = gtk_event_controller_key_new();
      gtk_event_controller_set_propagation_phase(keys, GTK_PHASE_CAPTURE);
      connect(keys, "key-pressed", &Gtk4TreeBackend::onMultilineKey, pn);
      gtk_widget_add_controller(nn.widget, keys);
      connect(nn.widget, "paste-clipboard", &Gtk4TreeBackend::onMultilinePaste, pn);
    } else {
      nn.outer = nn.widget = gtk_entry_new();
      connect(nn.widget, "changed", &Gtk4TreeBackend::onEntryChanged, pn);
      connect(nn.widget, "activate", &Gtk4TreeBackend::onEntryActivate, pn);
    }
    break;
  }
  case Kind::Dropdown: {
    const char *const none[] = {nullptr};
    nn.outer = nn.widget = gtk_drop_down_new_from_strings(none);
    connect(nn.widget, "notify::selected", &Gtk4TreeBackend::onDropdownSelected, pn);
    break;
  }
  case Kind::Slider: {
    nn.outer = nn.widget = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 1.0, 0.001);
    gtk_scale_set_draw_value(GTK_SCALE(nn.widget), FALSE);
    connect(nn.widget, "value-changed", &Gtk4TreeBackend::onSliderChanged, pn);
    break;
  }
  case Kind::Image: {
    nn.outer = nn.widget = gtk_picture_new();
    gtk_picture_set_can_shrink(GTK_PICTURE(nn.widget), TRUE);
    gtk_picture_set_content_fit(GTK_PICTURE(nn.widget), GTK_CONTENT_FIT_FILL);
    break;
  }
  case Kind::Icon: {
    nn.outer = nn.widget = gtk_image_new();
    break;
  }
  case Kind::Divider: {
    nn.outer = nn.widget = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    static bool made = false;
    if (!made) {
      made = true;
      GtkCssProvider *provider = gtk_css_provider_new();
      gtk_css_provider_load_from_string(provider, ".n8v-divider { min-height: 1px; background-color: alpha(currentColor, 0.15); }");
      gtk_style_context_add_provider_for_display(gtk_widget_get_display(nn.outer), GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
      g_object_unref(provider);
    }
    gtk_widget_add_css_class(nn.outer, "n8v-divider");
    break;
  }
  case Kind::Audio:
    nn.outer = nn.widget = gtk_media_controls_new(nullptr);
    break;
  case Kind::Video: {
    const n8v_video_options *o = std::get_if<n8v_video_options>(&pn.current.props);
    if (o && !o->controls) {
      nn.outer = nn.widget = gtk_picture_new();
      gtk_picture_set_can_shrink(GTK_PICTURE(nn.widget), TRUE);
      gtk_picture_set_content_fit(GTK_PICTURE(nn.widget), GTK_CONTENT_FIT_COVER);
    } else {
      nn.outer = nn.widget = gtk_video_new();
    }
    gtk_widget_set_overflow(nn.outer, GTK_OVERFLOW_HIDDEN);
    break;
  }
  case Kind::Document: {
    nn.widget = gtk_label_new("");
    nn.outer = gtk_overlay_new();
    gtk_overlay_set_child(GTK_OVERLAY(nn.outer), nn.widget);
    g_signal_connect_data(nn.outer, "get-child-position", G_CALLBACK(&Gtk4TreeBackend::onInlineImagePosition), nullptr, nullptr, (GConnectFlags)0);
    gtk_label_set_use_markup(GTK_LABEL(nn.widget), TRUE);
    gtk_label_set_wrap(GTK_LABEL(nn.widget), TRUE);
    gtk_label_set_wrap_mode(GTK_LABEL(nn.widget), PANGO_WRAP_WORD_CHAR);
    gtk_label_set_xalign(GTK_LABEL(nn.widget), 0.0f);
    gtk_label_set_yalign(GTK_LABEL(nn.widget), 0.0f);
    g_signal_connect_data(nn.widget, "activate-link", G_CALLBACK(&Gtk4TreeBackend::onDocumentLinkActivated), nullptr, nullptr, (GConnectFlags)0);
    break;
  }
  case Kind::Sidebar: {
    nn.titleLabel = gtk_label_new("");
    gtk_widget_add_css_class(nn.titleLabel, "heading");
    gtk_widget_set_margin_top(nn.titleLabel, 12);
    gtk_widget_set_margin_bottom(nn.titleLabel, 12);

    nn.listBox = gtk_list_box_new();
    gtk_widget_add_css_class(nn.listBox, "navigation-sidebar");
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(nn.listBox), GTK_SELECTION_SINGLE);
    g_signal_connect_data(nn.listBox, "row-activated", G_CALLBACK(&Gtk4TreeBackend::onRowActivated), nullptr, nullptr, (GConnectFlags)0);

    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), nn.listBox);
    gtk_widget_set_vexpand(scroll, TRUE);

    nn.panelRoot = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(nn.panelRoot), nn.titleLabel);
    gtk_box_append(GTK_BOX(nn.panelRoot), scroll);

    nn.contentArea = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_hexpand(nn.contentArea, TRUE);
    gtk_widget_set_vexpand(nn.contentArea, TRUE);

#ifdef N8V_HAVE_ADWAITA
    if (!pn.parent->parent) {
      nn.outer = adw_navigation_split_view_new();
      AdwNavigationPage *sidebarPage = adw_navigation_page_new(nn.panelRoot, "Sidebar");
      AdwNavigationPage *contentPage = adw_navigation_page_new(nn.contentArea, "Content");
      adw_navigation_split_view_set_sidebar(ADW_NAVIGATION_SPLIT_VIEW(nn.outer), sidebarPage);
      adw_navigation_split_view_set_content(ADW_NAVIGATION_SPLIT_VIEW(nn.outer), contentPage);
      break;
    }
#endif
    nn.outer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(nn.panelRoot, "sidebar");
    gtk_box_append(GTK_BOX(nn.outer), nn.panelRoot);
    gtk_box_append(GTK_BOX(nn.outer), nn.contentArea);
    break;
  }
  case Kind::Page: {
    nn.content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_hexpand(nn.content, TRUE);
    gtk_widget_set_vexpand(nn.content, TRUE);
    gtk_widget_set_visible(nn.content, FALSE);
    nn.outer = gtk_list_box_row_new();
    break;
  }
  case Kind::SidebarSection: {
    GtkWidget *row = gtk_list_box_row_new();
    gtk_list_box_row_set_selectable(GTK_LIST_BOX_ROW(row), FALSE);
    gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(row), FALSE);
    gtk_widget_add_css_class(row, "n8v-sidebar-decoration");
    nn.label = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(nn.label), 0.0f);
    gtk_widget_add_css_class(nn.label, "heading");
    gtk_widget_set_margin_start(nn.label, 12);
    gtk_widget_set_margin_end(nn.label, 12);
    gtk_widget_set_margin_top(nn.label, 12);
    gtk_widget_set_margin_bottom(nn.label, 4);
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), nn.label);
    nn.outer = nn.widget = row;
    break;
  }
  case Kind::SidebarDivider: {
    GtkWidget *row = gtk_list_box_row_new();
    gtk_list_box_row_set_selectable(GTK_LIST_BOX_ROW(row), FALSE);
    gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(row), FALSE);
    gtk_widget_add_css_class(row, "n8v-sidebar-decoration");
    gtk_widget_add_css_class(row, "n8v-sidebar-divider");
    GtkWidget *line = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_valign(line, GTK_ALIGN_CENTER); // otherwise it stretches to fill the whole row
    gtk_widget_set_margin_start(line, 12);
    gtk_widget_set_margin_end(line, 12);
    gtk_widget_set_margin_top(line, 6);
    gtk_widget_set_margin_bottom(line, 6);
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), line);
    nn.outer = nn.widget = row;
    break;
  }
  case Kind::Root:
    break;
  }
}

void Gtk4TreeBackend::applyFlex(NativeNode &nn, const n8v_flex_options &o) {
  gtk_orientable_set_orientation(GTK_ORIENTABLE(nn.content), o.direction == N8V_DIRECTION_HORIZONTAL ? GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL);
  gtk_box_set_spacing(GTK_BOX(nn.content), o.gap);
  gtk_widget_set_margin_start(nn.content, o.padding.left);
  gtk_widget_set_margin_end(nn.content, o.padding.right);
  gtk_widget_set_margin_top(nn.content, o.padding.top);
  gtk_widget_set_margin_bottom(nn.content, o.padding.bottom);
  setScrolled(nn, o.clip_horizontal || o.clip_vertical, o.clip_horizontal, o.clip_vertical);

  nn.stickyBottom = o.stick_to_bottom && o.clip_vertical;
}

void Gtk4TreeBackend::applyPanel(NativeNode &nn, const n8v_panel_options &o) {
  gtk_orientable_set_orientation(GTK_ORIENTABLE(nn.content), o.direction == N8V_DIRECTION_HORIZONTAL ? GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL);
  gtk_box_set_spacing(GTK_BOX(nn.content), o.gap);
  const bool card = o.role == N8V_PANEL_ROLE_CARD;
  const int vpad = card ? 10 : 6, hpad = card ? 12 : 10;
  gtk_widget_set_margin_start(nn.content, o.has_padding ? o.padding.left : hpad);
  gtk_widget_set_margin_end(nn.content, o.has_padding ? o.padding.right : hpad);
  gtk_widget_set_margin_top(nn.content, o.has_padding ? o.padding.top : vpad);
  gtk_widget_set_margin_bottom(nn.content, o.has_padding ? o.padding.bottom : vpad);
#ifdef N8V_HAVE_ADWAITA
  if (card) gtk_widget_add_css_class(nn.outer, "card");
  else gtk_widget_remove_css_class(nn.outer, "card");
#else
  if (card) gtk_widget_add_css_class(nn.outer, "n8v-card");
  else gtk_widget_remove_css_class(nn.outer, "n8v-card");
#endif

  std::string wanted = themeClass("n8v-accent-", o.accent);
  if (wanted.empty() && o.accent.a > 0.0f) {
    char name[40];
    std::snprintf(name, sizeof(name), "n8v-accent-%02x%02x%02x", (int)o.accent.r & 255, (int)o.accent.g & 255, (int)o.accent.b & 255);
    wanted = name;
    static std::unordered_set<std::string> made;
    if (made.insert(wanted).second) {
      char rule[160];
      std::snprintf(rule, sizeof(rule), ".%s { border-left: 4px solid #%02x%02x%02x; }", name, (int)o.accent.r & 255, (int)o.accent.g & 255, (int)o.accent.b & 255);
      GtkCssProvider *provider = gtk_css_provider_new();
      gtk_css_provider_load_from_string(provider, rule);
      gtk_style_context_add_provider_for_display(gtk_widget_get_display(nn.outer), GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
      g_object_unref(provider);
    }
  }
  if (wanted != nn.accentClass) {
    if (!nn.accentClass.empty()) gtk_widget_remove_css_class(nn.outer, nn.accentClass.c_str());
    if (!wanted.empty()) gtk_widget_add_css_class(nn.outer, wanted.c_str());
    nn.accentClass = wanted;
  }
}

void Gtk4TreeBackend::applyButton(NativeNode &nn, const n8v_button_options &o) {
  GtkButton *button = GTK_BUTTON(nn.widget);
  const std::string &label = nn.node->current.label;
  const bool hasIcon = o.icon && *o.icon;
  const bool hasImage = !hasIcon && o.image && *o.image;
  const bool trailing = o.icon_position == N8V_ICON_POSITION_TRAILING;
  if (hasIcon || hasImage) {
    if (!nn.hasIconBox || nn.iconTrailing != trailing) {
      GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
      gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
      nn.image = gtk_image_new();
      nn.picture = gtk_picture_new();
      gtk_picture_set_can_shrink(GTK_PICTURE(nn.picture), TRUE);
      gtk_picture_set_content_fit(GTK_PICTURE(nn.picture), GTK_CONTENT_FIT_FILL);
      gtk_widget_set_halign(nn.picture, GTK_ALIGN_CENTER);
      gtk_widget_set_valign(nn.picture, GTK_ALIGN_CENTER);
      nn.label = gtk_label_new("");
      if (trailing) {
        gtk_box_append(GTK_BOX(box), nn.label);
        gtk_box_append(GTK_BOX(box), nn.image);
        gtk_box_append(GTK_BOX(box), nn.picture);
      } else {
        gtk_box_append(GTK_BOX(box), nn.image);
        gtk_box_append(GTK_BOX(box), nn.picture);
        gtk_box_append(GTK_BOX(box), nn.label);
      }
      gtk_button_set_child(button, box);
      nn.hasIconBox = true;
      nn.iconTrailing = trailing;
      nn.textureSource = nullptr;
    }
    gtk_label_set_text(GTK_LABEL(nn.label), label.c_str());
    gtk_widget_set_visible(nn.label, !label.empty());
    gtk_widget_set_visible(nn.image, hasIcon);
    gtk_widget_set_visible(nn.picture, hasImage);
    if (hasIcon) setIconImage(nn, nn.image, o.icon, kDefaultIconSize);
    if (hasImage) {
      n8v_image_options imageOptions{};
      imageOptions.source_kind = N8V_IMAGE_SOURCE_PATH;
      imageOptions.path = o.image;
      const DecodedImage *decoded = getOrDecodeImage(imageOptions);
      if (decoded && decoded->width > 0 && decoded->height > 0) {
        const int height = o.image_height > 0 ? o.image_height : (int)(kDefaultIconSize * 1.15f);
        const int width = std::max(1, (int)std::lround((double)decoded->width * height / decoded->height));
        const DecodedImage *scaled = getOrScaleImage(decoded, width, height);
        if (nn.textureSource != scaled) {
          GdkTexture *texture = makeTexture(scaled);
          gtk_picture_set_paintable(GTK_PICTURE(nn.picture), GDK_PAINTABLE(texture));
          g_object_unref(texture);
          nn.textureSource = scaled;
        }
        gtk_widget_set_size_request(nn.picture, width, height);
      } else {
        gtk_widget_set_visible(nn.picture, FALSE); // not loaded
      }
    }
  } else {
    if (nn.hasIconBox) {
      gtk_button_set_child(button, nullptr);
      nn.hasIconBox = false;
    }
    gtk_button_set_label(button, label.c_str());
  }
  const bool flat = o.style == N8V_BUTTON_STYLE_GHOST;
  if (flat) gtk_widget_add_css_class(nn.widget, "flat");
  else gtk_widget_remove_css_class(nn.widget, "flat");
  if (o.style == N8V_BUTTON_STYLE_PRIMARY) gtk_widget_add_css_class(nn.widget, "suggested-action");
  else gtk_widget_remove_css_class(nn.widget, "suggested-action");
  if (o.checked && GTK_IS_TOGGLE_BUTTON(nn.widget)) {
    GtkToggleButton *toggle = GTK_TOGGLE_BUTTON(nn.widget);
    if ((gtk_toggle_button_get_active(toggle) != FALSE) != *o.checked) gtk_toggle_button_set_active(toggle, *o.checked);
  }
}

void Gtk4TreeBackend::applySidebar(NativeNode &nn, const n8v_sidebar_options &o) {
  const std::string title = o.title ? o.title : "";
  gtk_label_set_text(GTK_LABEL(nn.titleLabel), title.c_str());
  gtk_widget_set_visible(nn.titleLabel, !title.empty());
  const float width = o.width.mode == N8V_SIZING_FIXED ? o.width.value : 0.0f;
#ifdef N8V_HAVE_ADWAITA
  if (ADW_IS_NAVIGATION_SPLIT_VIEW(nn.outer)) {
    if (width > 0.0f) {
      adw_navigation_split_view_set_min_sidebar_width(ADW_NAVIGATION_SPLIT_VIEW(nn.outer), width);
      adw_navigation_split_view_set_max_sidebar_width(ADW_NAVIGATION_SPLIT_VIEW(nn.outer), width);
    }
    return;
  }
#endif
  gtk_widget_set_size_request(nn.panelRoot, width > 0.0f ? (int)width : -1, -1);
}

void Gtk4TreeBackend::applyPage(NativeNode &nn, const n8v_page_options &o) {
  NativeNode &sidebar = *native(*nn.node->parent);
  const auto *sidebarProps = propsOf<n8v_sidebar_options>(*sidebar.node);
  const bool compact = sidebarProps && sidebarProps->compact;
#ifdef N8V_HAVE_ADWAITA
  const bool wantAdw = !compact;
#else
  const bool wantAdw = false;
#endif

  const bool needsBuild = !nn.widget || nn.adwRow != wantAdw;
  if (needsBuild) {
    GtkWidget *oldRow = nn.outer;
    const bool attached = gtk_widget_get_parent(oldRow) != nullptr;
    const int position = attached ? gtk_list_box_row_get_index(GTK_LIST_BOX_ROW(oldRow)) : -1;
    if (attached) gtk_list_box_remove(GTK_LIST_BOX(sidebar.listBox), oldRow);

    GtkWidget *row = nullptr;
    nn.image = nullptr;
    nn.rowLabel = nullptr;
    nn.notifyBar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(nn.notifyBar, "n8v-notify-bar");
    gtk_widget_set_valign(nn.notifyBar, GTK_ALIGN_CENTER);
    gtk_widget_set_opacity(nn.notifyBar, 0.0);
    nn.badge = gtk_label_new("");
    gtk_widget_add_css_class(nn.badge, "n8v-badge");
    gtk_widget_set_valign(nn.badge, GTK_ALIGN_CENTER);
    gtk_widget_set_visible(nn.badge, FALSE);
#ifdef N8V_HAVE_ADWAITA
    if (wantAdw) {
      row = adw_action_row_new();
      nn.image = gtk_image_new();
      adw_action_row_add_prefix(ADW_ACTION_ROW(row), nn.image);
      adw_action_row_add_prefix(ADW_ACTION_ROW(row), nn.notifyBar);
      adw_action_row_add_suffix(ADW_ACTION_ROW(row), nn.badge);
    } else
#endif
    {
      row = gtk_list_box_row_new();
      GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
      const int vmargin = compact ? 2 : 8;
      gtk_widget_set_margin_start(hbox, 12);
      gtk_widget_set_margin_end(hbox, 12);
      gtk_widget_set_margin_top(hbox, vmargin);
      gtk_widget_set_margin_bottom(hbox, vmargin);
      nn.image = gtk_image_new();
      nn.rowLabel = gtk_label_new("");
      gtk_widget_set_halign(nn.rowLabel, GTK_ALIGN_START);
      gtk_box_append(GTK_BOX(hbox), nn.notifyBar);
      gtk_box_append(GTK_BOX(hbox), nn.image);
      gtk_widget_set_hexpand(nn.rowLabel, TRUE);
      gtk_box_append(GTK_BOX(hbox), nn.rowLabel);
      gtk_box_append(GTK_BOX(hbox), nn.badge);
      gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), hbox);
    }
    gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(row), TRUE);
    g_object_set_data(G_OBJECT(row), "n8v-node", &nn);
    g_object_ref_sink(row);
    nn.owned.push_back(row);
    nn.outer = row;
    nn.widget = row;
    nn.adwRow = wantAdw;
    nn.textureSource = nullptr;
    if (attached) gtk_list_box_insert(GTK_LIST_BOX(sidebar.listBox), row, position);
    g_object_unref(oldRow);
    nn.owned.erase(std::remove(nn.owned.begin(), nn.owned.end(), oldRow), nn.owned.end());
  }

  const std::string name = o.name ? o.name : "";
#ifdef N8V_HAVE_ADWAITA
  if (nn.adwRow) adw_preferences_row_set_title(ADW_PREFERENCES_ROW(nn.outer), name.c_str());
#endif
  if (nn.rowLabel) gtk_label_set_text(GTK_LABEL(nn.rowLabel), name.c_str());

  const bool hasIcon = o.icon && *o.icon;
  const bool hasImage = !hasIcon && o.image && *o.image;
  gtk_widget_set_visible(nn.image, hasIcon || hasImage);
  if (hasIcon) setIconImage(nn, nn.image, o.icon, kDefaultIconSize);
  if (hasImage) {
    constexpr int kPageImageSize = 28;
    n8v_image_options imageOptions{};
    imageOptions.source_kind = N8V_IMAGE_SOURCE_PATH;
    imageOptions.path = o.image;
    const DecodedImage *decoded = getOrFitImageInSquare(getOrDecodeImage(imageOptions), kPageImageSize);
    if (decoded) {
      CornerRadius radius;
      switch (o.image_rounding.mode) {
      case N8V_ROUNDING_STYLE_DEFAULT:
        radius = activePaint().image().cornerRadius;
        break;
      case N8V_ROUNDING_NONE:
        radius = {};
        break;
      case N8V_ROUNDING_FIXED:
        radius = {o.image_rounding.radius.top_left, o.image_rounding.radius.top_right, o.image_rounding.radius.bottom_left, o.image_rounding.radius.bottom_right};
        break;
      }
      const DecodedImage *baked = getOrBakeRoundedImage(decoded, decoded->width, decoded->height, radius.topLeft, radius.topRight, radius.bottomLeft, radius.bottomRight);
      if (nn.textureSource != baked) {
        GdkTexture *texture = makeTexture(baked);
        gtk_image_set_from_paintable(GTK_IMAGE(nn.image), GDK_PAINTABLE(texture));
        gtk_image_set_pixel_size(GTK_IMAGE(nn.image), kPageImageSize);
        g_object_unref(texture);
        nn.textureSource = baked;
      }
    } else {
      gtk_widget_set_visible(nn.image, FALSE);
    }
  }

  gtk_widget_set_opacity(nn.notifyBar, o.notify ? 1.0 : 0.0);
  const std::string notifyLabelClass = "n8v-notify-label";
  GtkWidget *nameWidget = nn.rowLabel;
#ifdef N8V_HAVE_ADWAITA
  if (nn.adwRow) nameWidget = nn.outer;
#endif
  if (o.notify) gtk_widget_add_css_class(nameWidget, notifyLabelClass.c_str());
  else gtk_widget_remove_css_class(nameWidget, notifyLabelClass.c_str());

  const bool hasBadge = o.badge && *o.badge;
  gtk_widget_set_visible(nn.badge, hasBadge);
  gtk_label_set_text(GTK_LABEL(nn.badge), hasBadge ? o.badge : "");
  static const char *const colorClasses[] =
    {nullptr, "n8v-badge-red", "n8v-badge-orange", "n8v-badge-yellow", "n8v-badge-green", "n8v-badge-blue", "n8v-badge-purple", "n8v-badge-pink"};
  for (const char *cls : colorClasses) {
    if (cls) gtk_widget_remove_css_class(nn.badge, cls);
  }
  if (o.badge_color > 0 && o.badge_color < (int)(sizeof(colorClasses) / sizeof(colorClasses[0]))) gtk_widget_add_css_class(nn.badge, colorClasses[o.badge_color]);
}

void Gtk4TreeBackend::applyMultilineEntry(NativeNode &nn, const n8v_entry_options &o) {
  gtk_label_set_text(GTK_LABEL(nn.label), o.placeholder ? o.placeholder : "");

  GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(nn.widget));
  if (const std::string *text = std::get_if<std::string>(&nn.node->bound)) {
    if (*text != textBufferText(buffer)) gtk_text_buffer_set_text(buffer, text->c_str(), -1);
  }
  gtk_widget_set_visible(nn.label, gtk_text_buffer_get_char_count(buffer) == 0);
  nn.entryHeight = 0;
  updateMultilineHeight(nn);
}

void Gtk4TreeBackend::updateMultilineHeight(NativeNode &nn) {
  const auto *o = propsOf<n8v_entry_options>(*nn.node);
  if (!o || !o->multiline || !nn.scrolled) return;
  GtkTextView *view = GTK_TEXT_VIEW(nn.widget);
  GtkTextBuffer *buffer = gtk_text_view_get_buffer(view);
  GtkTextIter start, end;
  gtk_text_buffer_get_start_iter(buffer, &start);
  gtk_text_buffer_get_end_iter(buffer, &end);
  GdkRectangle first{}, last{};
  gtk_text_view_get_iter_location(view, &start, &first);
  gtk_text_view_get_iter_location(view, &end, &last);
  if (first.height > 0) lineHeight_ = first.height;
  const int lineHeight = lineHeight_ > 0 ? lineHeight_ : 18;

  const EntryLines lines = entryLines(*o);
  const int chrome = o->match_entry_style ? 0 : 2 * kMultilineFrameMarginPx;
  const int minHeight = chrome + lines.min * lineHeight;
  const int maxHeight = chrome + lines.max * lineHeight;
  const int height = lines.fixed ? minHeight : std::clamp(chrome + last.y + std::max(last.height, lineHeight), minHeight, maxHeight);
  const bool overflowing = chrome + last.y + std::max(last.height, lineHeight) > height;
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(nn.scrolled), GTK_POLICY_NEVER, overflowing && height >= 56 ? GTK_POLICY_AUTOMATIC : GTK_POLICY_EXTERNAL);
  if (!overflowing) {
    GtkAdjustment *adjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(nn.scrolled));
    if (gtk_adjustment_get_value(adjustment) != 0.0) gtk_adjustment_set_value(adjustment, 0.0);
  }
  if (height == nn.entryHeight) return;
  nn.entryHeight = height;
  gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(nn.scrolled), height);
  gtk_scrolled_window_set_max_content_height(GTK_SCROLLED_WINDOW(nn.scrolled), height);
}

void Gtk4TreeBackend::updateMultilineHeights() {
  for (PNode *pn : live_) {
    if (pn->handle && propsOf<n8v_entry_options>(*pn)) updateMultilineHeight(*native(*pn));
  }
}

void Gtk4TreeBackend::apply(NativeNode &nn) {
  Guard guard;
  PNode &pn = *nn.node;
  const std::string &label = pn.current.label;
  std::visit(
    Overloaded{
      [&](const std::monostate &) {},
      [&](const n8v_flex_options &o) { applyFlex(nn, o); },
      [&](const n8v_panel_options &o) { applyPanel(nn, o); },
      [&](const n8v_floating_options &o) {
        static bool made = false;
        if (o.opaque && !made) {
          made = true;
          GtkCssProvider *provider = gtk_css_provider_new();
          gtk_css_provider_load_from_string(provider, ".n8v-opaque { background-color: @theme_bg_color; background-color: @window_bg_color; }");
          gtk_style_context_add_provider_for_display(gtk_widget_get_display(nn.outer), GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
          g_object_unref(provider);
        }
        if (o.opaque) gtk_widget_add_css_class(nn.outer, "n8v-opaque");
        else gtk_widget_remove_css_class(nn.outer, "n8v-opaque");
      },
      [&](const n8v_sidebar_options &o) { applySidebar(nn, o); },
      [&](const n8v_page_options &o) { applyPage(nn, o); },
      [&](const n8v_button_options &o) { applyButton(nn, o); },
      [&](const n8v_text_options &o) { setLabelFromText(nn.widget, o, label); },
      [&](const n8v_checkbox_options &) {
        gtk_check_button_set_label(GTK_CHECK_BUTTON(nn.widget), label.c_str());
        if (const bool *checked = std::get_if<bool>(&pn.bound)) gtk_check_button_set_active(GTK_CHECK_BUTTON(nn.widget), *checked);
      },
      [&](const n8v_toggle_options &) {
        gtk_label_set_text(GTK_LABEL(nn.label), label.c_str());
        if (const bool *checked = std::get_if<bool>(&pn.bound)) gtk_switch_set_active(GTK_SWITCH(nn.widget), *checked);
      },
      [&](const n8v_radio_options &o) {
        gtk_check_button_set_label(GTK_CHECK_BUTTON(nn.widget), label.c_str());
        if (const int *selected = std::get_if<int>(&pn.bound)) gtk_check_button_set_active(GTK_CHECK_BUTTON(nn.widget), *selected == o.value);
      },
      [&](const n8v_entry_options &o) {
        if (o.multiline) {
          applyMultilineEntry(nn, o);
          return;
        }
        gtk_entry_set_placeholder_text(GTK_ENTRY(nn.widget), o.placeholder ? o.placeholder : "");
        gtk_entry_set_visibility(GTK_ENTRY(nn.widget), !o.password);
        if (const std::string *text = std::get_if<std::string>(&pn.bound)) {
          if (*text != gtk_editable_get_text(GTK_EDITABLE(nn.widget))) gtk_editable_set_text(GTK_EDITABLE(nn.widget), text->c_str());
        }
      },
      [&](const n8v_dropdown_options &o) {
        const bool hasPlaceholder = o.placeholder && *o.placeholder;
        std::vector<const char *> items;
        std::string key;
        if (hasPlaceholder) {
          items.push_back(o.placeholder);
          key += std::string(o.placeholder) + '\n';
        }
        for (size_t i = 0; i < o.item_count; ++i) {
          items.push_back(o.items[i] ? o.items[i] : "");
          key += std::string(items.back()) + '\n';
        }
        if (key != nn.lastItemsKey || (nn.lastItemsKey.empty() && !items.empty())) {
          GListModel *model = gtk_drop_down_get_model(GTK_DROP_DOWN(nn.widget));
          const size_t count = items.size();
          items.push_back(nullptr);
          gtk_string_list_splice(GTK_STRING_LIST(model), 0, (guint)nn.dropdownCount, items.data());
          nn.dropdownCount = count;
          nn.lastItemsKey = key;
        }
        const int *selected = std::get_if<int>(&pn.bound);
        const bool inRange = selected && *selected >= 0 && (size_t)*selected < o.item_count;
        guint want = inRange ? (guint)*selected + (hasPlaceholder ? 1 : 0) : (hasPlaceholder ? 0 : GTK_INVALID_LIST_POSITION);
        if (gtk_drop_down_get_selected(GTK_DROP_DOWN(nn.widget)) != want) gtk_drop_down_set_selected(GTK_DROP_DOWN(nn.widget), want);
      },
      [&](const n8v_slider_options &o) {
        gtk_range_set_range(GTK_RANGE(nn.widget), o.min, o.max);
        gtk_range_set_increments(GTK_RANGE(nn.widget), (o.max - o.min) / 100.0, (o.max - o.min) / 10.0);
        if (const float *value = std::get_if<float>(&pn.bound)) {
          if (gtk_range_get_value(GTK_RANGE(nn.widget)) != *value) gtk_range_set_value(GTK_RANGE(nn.widget), *value);
        }
      },
      [&](const n8v_image_options &o) {
        const DecodedImage *decoded = getOrDecodeImage(o);
        if (!decoded) {
          gtk_picture_set_paintable(GTK_PICTURE(nn.widget), nullptr);
          nn.textureSource = nullptr;
          nn.imageWidth = nn.imageHeight = 0;
          return;
        }
        nn.imageWidth = decoded->width;
        nn.imageHeight = decoded->height;
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
        if (nn.textureSource != baked) {
          GdkTexture *texture = makeTexture(baked);
          gtk_picture_set_paintable(GTK_PICTURE(nn.widget), GDK_PAINTABLE(texture));
          g_object_unref(texture);
          nn.textureSource = baked;
        }
        gtk_widget_set_size_request(nn.widget, targetW, targetH);
      },
      [&](const n8v_video_options &o) { applyVideo(nn, o); },
      [&](const n8v_audio_options &o) { applyAudio(nn, o); },
      [&](const n8v_divider_options &o) {
        std::string wanted = themeClass("n8v-fill-", o.color);
        if (wanted.empty() && o.color.a > 0.0f) {
          char name[40];
          std::snprintf(name, sizeof(name), "n8v-divider-%02x%02x%02x%02x", (int)o.color.r & 255, (int)o.color.g & 255, (int)o.color.b & 255, (int)o.color.a & 255);
          wanted = name;
          static std::unordered_set<std::string> made;
          if (made.insert(wanted).second) {
            char rule[200];
            std::snprintf(
              rule,
              sizeof(rule),
              ".n8v-divider.%s { background-color: rgba(%d,%d,%d,%.3f); }",
              name,
              (int)o.color.r,
              (int)o.color.g,
              (int)o.color.b,
              o.color.a / 255.0f
            );
            GtkCssProvider *provider = gtk_css_provider_new();
            gtk_css_provider_load_from_string(provider, rule);
            gtk_style_context_add_provider_for_display(gtk_widget_get_display(nn.outer), GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
            g_object_unref(provider);
          }
        }
        if (wanted != nn.roundClass) {
          if (!nn.roundClass.empty()) gtk_widget_remove_css_class(nn.outer, nn.roundClass.c_str());
          if (!wanted.empty()) gtk_widget_add_css_class(nn.outer, wanted.c_str());
          nn.roundClass = wanted;
        }
      },
      [&](const n8v_icon_options &o) {
        const int size = o.width.mode == N8V_SIZING_FIXED ? (int)o.width.value : kDefaultIconSize;
        setIconImage(nn, nn.widget, o.name, size);
      },
      [&](const n8v_document_options &o) { applyDocument(nn, o); },
      [&](const n8v_sidebar_section_options &o) { gtk_label_set_text(GTK_LABEL(nn.label), o.title ? o.title : ""); },
      [&](const n8v_sidebar_divider_options &) {},
    },
    pn.current.props
  );
}

} // namespace

std::unique_ptr<Backend> makeGtk4TreeBackend() { return std::make_unique<Gtk4TreeBackend>(); }

} // namespace n8v::detail
