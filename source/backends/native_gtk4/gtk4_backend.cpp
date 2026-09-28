#include "gtk4_backend.hpp"

#include "core/icon_loader.hpp"
#include "core/icon_registry.hpp"
#include "core/image_loader.hpp"
#include "core/native_widget_meta.hpp"
#include "core/text_style_flags.hpp"
#include "core/ui_core_internal.hpp"

#include <gtk/gtk.h>

// fix conflict with function of same name
#undef g_object_ref_sink
#undef g_object_ref

#ifdef N8V_HAVE_ADWAITA
#include <adwaita.h>
#endif

#include <algorithm>
#include <cstdio>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

extern "C" {
gulong g_signal_connect_data(gpointer instance, const gchar *detailed_signal, GCallback c_handler, gpointer data, GClosureNotify destroy_data, GConnectFlags connect_flags);
gpointer g_object_ref_sink(gpointer object);
gpointer g_object_ref(gpointer object);
void g_object_unref(gpointer object);
}

namespace n8v::detail {
namespace {

std::string linkMarkup(std::string_view text, std::string_view url) {
  gchar *escapedText = g_markup_escape_text(std::string(text).c_str(), -1);
  gchar *escapedUrl = g_markup_escape_text(std::string(url).c_str(), -1);
  std::string result = std::string("<a href=\"") + escapedUrl + "\">" + escapedText + "</a>";
  g_free(escapedText);
  g_free(escapedUrl);
  return result;
}

int compareActionRowOrdinals(GtkListBoxRow *a, GtkListBoxRow *b, gpointer /*userData*/) {
  int ordinalA = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(a), "n8v-ordinal"));
  int ordinalB = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(b), "n8v-ordinal"));
  return ordinalA - ordinalB;
}

bool useAdwActionRow(const n8v::detail::NativeWidgetMeta &meta) {
#ifdef N8V_HAVE_ADWAITA
  return !meta.sidebarCompact;
#else
  (void)meta;
  return false;
#endif
}

void setPlainLabelText(GtkWidget *label, std::string_view text, bool bold, bool italic, bool strikethrough = false) {
  if (!bold && !italic && !strikethrough) {
    gtk_label_set_text(GTK_LABEL(label), std::string(text).c_str());
    return;
  }
  gchar *escaped = g_markup_escape_text(std::string(text).c_str(), -1);
  std::string markup = escaped;
  g_free(escaped);
  if (strikethrough) markup = "<s>" + markup + "</s>";
  if (italic) markup = "<i>" + markup + "</i>";
  if (bold) markup = "<b>" + markup + "</b>";
  gtk_label_set_markup(GTK_LABEL(label), markup.c_str());
}

class Gtk4Backend final : public Backend {
public:
  ~Gtk4Backend() override { shutdown(); }

  bool initialize(int width, int height, std::string_view title) override {
    gtk_init();
#ifdef N8V_HAVE_ADWAITA
    adw_init();
#endif

    window_ = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(window_), std::string(title).c_str());
    gtk_window_set_default_size(GTK_WINDOW(window_), width, height);

    fixed_ = gtk_fixed_new();
    fixedScroll_ = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(fixedScroll_), GTK_POLICY_EXTERNAL, GTK_POLICY_EXTERNAL);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(fixedScroll_), fixed_);
    gtk_widget_set_hexpand(fixedScroll_, TRUE);
    gtk_widget_set_vexpand(fixedScroll_, TRUE);
    gtk_window_set_child(GTK_WINDOW(window_), fixedScroll_);

    g_signal_connect_data(window_, "close-request", G_CALLBACK(&Gtk4Backend::onCloseRequest), this, nullptr, (GConnectFlags)0);

    GtkEventController *motion = gtk_event_controller_motion_new();
    g_signal_connect_data(motion, "motion", G_CALLBACK(&Gtk4Backend::onPointerMotion), this, nullptr, (GConnectFlags)0);
    gtk_widget_add_controller(fixed_, motion);

    GtkGesture *click = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click), 0);
    g_signal_connect_data(click, "pressed", G_CALLBACK(&Gtk4Backend::onPointerPressed), this, nullptr, (GConnectFlags)0);
    g_signal_connect_data(click, "released", G_CALLBACK(&Gtk4Backend::onPointerReleased), this, nullptr, (GConnectFlags)0);
    gtk_widget_add_controller(fixed_, GTK_EVENT_CONTROLLER(click));

    gtk_window_present(GTK_WINDOW(window_));

    measureLabel_ = gtk_label_new("");
    g_object_ref_sink(measureLabel_);

    measureButton_ = gtk_button_new_with_label("");
    g_object_ref_sink(measureButton_);

    measureLink_ = gtk_label_new("");
    gtk_label_set_use_markup(GTK_LABEL(measureLink_), TRUE);
    g_object_ref_sink(measureLink_);

    measureEntry_ = gtk_entry_new();
    g_object_ref_sink(measureEntry_);

    measureCheckbox_ = gtk_check_button_new();
    g_object_ref_sink(measureCheckbox_);

    measureRadio_ = gtk_check_button_new();
    g_object_ref_sink(measureRadio_);

    const char *const emptyItems[] = {nullptr};
    measureDropdown_ = gtk_drop_down_new_from_strings(emptyItems);
    g_object_ref_sink(measureDropdown_);

    measureSlider_ = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 1.0, 0.01);
    g_object_ref_sink(measureSlider_);

    measureSwitch_ = gtk_switch_new();
    g_object_ref_sink(measureSwitch_);

    panelCssProvider_ = gtk_css_provider_new();
    gtk_style_context_add_provider_for_display(gtk_widget_get_display(window_), GTK_STYLE_PROVIDER(panelCssProvider_), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    return true;
  }

  bool pumpEvents() override {
    GMainContext *context = g_main_context_default();
    while (g_main_context_pending(context)) {
      g_main_context_iteration(context, FALSE);
    }
    return !closeRequested_;
  }

  bool pointerDown() const override { return pointerDown_; }

  Clay_Dimensions windowSize() const override { return {(float)gtk_widget_get_width(window_), (float)gtk_widget_get_height(window_)}; }

  Clay_Dimensions measureText(std::string_view text, FontFamily, uint16_t, bool bold, bool italic) const override {
    setPlainLabelText(measureLabel_, text, bold, italic);
    int minW = 0, natW = 0, minH = 0, natH = 0;
    gtk_widget_measure(measureLabel_, GTK_ORIENTATION_HORIZONTAL, -1, &minW, &natW, nullptr, nullptr);
    gtk_widget_measure(measureLabel_, GTK_ORIENTATION_VERTICAL, -1, &minH, &natH, nullptr, nullptr);
    return {(float)natW, (float)natH};
  }

  Clay_Dimensions measureNativeChrome(NativeWidgetKind kind, std::string_view text, uint16_t fontSize, bool hasIcon) const override {
    if (kind == NativeWidgetKind::Entry) {
      int minW = 0, natW = 0, minH = 0, natH = 0;
      gtk_widget_measure(measureEntry_, GTK_ORIENTATION_HORIZONTAL, -1, &minW, &natW, nullptr, nullptr);
      gtk_widget_measure(measureEntry_, GTK_ORIENTATION_VERTICAL, -1, &minH, &natH, nullptr, nullptr);
      return {0, (float)natH};
    }
    if (kind == NativeWidgetKind::Link) {
      gtk_label_set_markup(GTK_LABEL(measureLink_), linkMarkup(text, "about:blank").c_str());
      int minW = 0, natW = 0, minH = 0, natH = 0;
      gtk_widget_measure(measureLink_, GTK_ORIENTATION_HORIZONTAL, -1, &minW, &natW, nullptr, nullptr);
      gtk_widget_measure(measureLink_, GTK_ORIENTATION_VERTICAL, -1, &minH, &natH, nullptr, nullptr);
      return {(float)natW, (float)natH};
    }
    if (kind == NativeWidgetKind::Dropdown) {
      int minW = 0, natW = 0, minH = 0, natH = 0;
      gtk_widget_measure(measureDropdown_, GTK_ORIENTATION_HORIZONTAL, -1, &minW, &natW, nullptr, nullptr);
      gtk_widget_measure(measureDropdown_, GTK_ORIENTATION_VERTICAL, -1, &minH, &natH, nullptr, nullptr);
      return {0, (float)natH};
    }
    if (kind == NativeWidgetKind::Slider) {
      int minW = 0, natW = 0, minH = 0, natH = 0;
      gtk_widget_measure(measureSlider_, GTK_ORIENTATION_HORIZONTAL, -1, &minW, &natW, nullptr, nullptr);
      gtk_widget_measure(measureSlider_, GTK_ORIENTATION_VERTICAL, -1, &minH, &natH, nullptr, nullptr);
      return {0, (float)natH};
    }
    if (kind == NativeWidgetKind::Switch) {
      int swMinW = 0, swNatW = 0, swMinH = 0, swNatH = 0;
      gtk_widget_measure(measureSwitch_, GTK_ORIENTATION_HORIZONTAL, -1, &swMinW, &swNatW, nullptr, nullptr);
      gtk_widget_measure(measureSwitch_, GTK_ORIENTATION_VERTICAL, -1, &swMinH, &swNatH, nullptr, nullptr);
      gtk_label_set_text(GTK_LABEL(measureLabel_), std::string(text).c_str());
      int lblMinW = 0, lblNatW = 0, lblMinH = 0, lblNatH = 0;
      gtk_widget_measure(measureLabel_, GTK_ORIENTATION_HORIZONTAL, -1, &lblMinW, &lblNatW, nullptr, nullptr);
      gtk_widget_measure(measureLabel_, GTK_ORIENTATION_VERTICAL, -1, &lblMinH, &lblNatH, nullptr, nullptr);
      return {(float)(swNatW + 6 + lblNatW), (float)std::max(swNatH, lblNatH)};
    }
    if (kind == NativeWidgetKind::Checkbox || kind == NativeWidgetKind::Radio) {
      GtkWidget *probe = kind == NativeWidgetKind::Radio ? measureRadio_ : measureCheckbox_;
      gtk_check_button_set_label(GTK_CHECK_BUTTON(probe), std::string(text).c_str());
      int minW = 0, natW = 0, minH = 0, natH = 0;
      gtk_widget_measure(probe, GTK_ORIENTATION_HORIZONTAL, -1, &minW, &natW, nullptr, nullptr);
      gtk_widget_measure(probe, GTK_ORIENTATION_VERTICAL, -1, &minH, &natH, nullptr, nullptr);
      return {(float)natW, (float)natH};
    }
    if (hasIcon && text.empty()) {
      float size = fontSize + 20.0f;
      return {size, size};
    }
    gtk_button_set_label(GTK_BUTTON(measureButton_), std::string(text).c_str());
    int minW = 0, natW = 0, minH = 0, natH = 0;
    gtk_widget_measure(measureButton_, GTK_ORIENTATION_HORIZONTAL, -1, &minW, &natW, nullptr, nullptr);
    gtk_widget_measure(measureButton_, GTK_ORIENTATION_VERTICAL, -1, &minH, &natH, nullptr, nullptr);
    if (hasIcon) natW += fontSize * 1.5;
    return {(float)natW, (float)natH};
  }

  void beginFrame() override { Clay_SetPointerState({pointerX_, pointerY_}, false); }

  void setScrollOffsetY(uint32_t containerId, float y) override {
    auto it = scrollContainers_.find(containerId);
    if (it == scrollContainers_.end()) return;
    gtk_adjustment_set_value(gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(it->second.scrolled)), y);
  }

  void setScrollOffsetX(uint32_t containerId, float x) override {
    auto it = scrollContainers_.find(containerId);
    if (it == scrollContainers_.end()) return;
    gtk_adjustment_set_value(gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(it->second.scrolled)), x);
  }

  void present(Clay_RenderCommandArray commands) override {
    std::map<int, int> wrapLineCounts;
    std::set<WidgetKey> seenKeys;
    GtkWidget *pendingLabelTarget = nullptr;
    GtkWidget *pendingButtonIconLabel = nullptr;
    GtkWidget *pendingSwitchLabel = nullptr;
    bool pendingIsActionRow = false;
    bool pendingActionRowIsAdw = false;
    NativeWidgetKind pendingKind = NativeWidgetKind::Button;
    std::string pendingLinkUrl;
    std::set<int> seenActionRowOrdinals;
    std::set<int> seenSidebarOrdinals;
    containerStack_.clear();
    touchedScrollContainers_.clear();

    for (int32_t i = 0; i < commands.length; ++i) {
      Clay_RenderCommand *command = Clay_RenderCommandArray_Get(&commands, i);

      if (command->commandType == CLAY_RENDER_COMMAND_TYPE_RECTANGLE) {
        auto *meta = static_cast<NativeWidgetMeta *>(command->userData);
        if (!meta || meta->kind == NativeWidgetKind::Sidebar) {
          pendingLabelTarget = nullptr;
          pendingButtonIconLabel = nullptr;
          pendingSwitchLabel = nullptr;
          pendingIsActionRow = false;
          continue;
        }
        if (meta->kind == NativeWidgetKind::Button && !containerStack_.empty() && containerStack_.back().rowListBox) {
          seenActionRowOrdinals.insert(meta->ordinal);
          pendingLabelTarget = ensureActionRow(containerStack_.back(), *meta);
          pendingActionRowIsAdw = useAdwActionRow(*meta);
          pendingKind = NativeWidgetKind::Button;
          pendingButtonIconLabel = nullptr;
          pendingSwitchLabel = nullptr;
          pendingIsActionRow = true;
          continue;
        }
        pendingIsActionRow = false;
        WidgetKey key{meta->ordinal, -1};
        seenKeys.insert(key);
        GtkWidget *widget = ensureWidget(key, *meta);
        positionWidget(widget, command->boundingBox);
        pendingLabelTarget = widget;
        pendingKind = meta->kind;
        pendingLinkUrl = meta->kind == NativeWidgetKind::Link && meta->url ? *meta->url : std::string();
        pendingButtonIconLabel = meta->kind == NativeWidgetKind::Button ? ensureButtonIcon(widget, *meta) : nullptr;
        pendingSwitchLabel = meta->kind == NativeWidgetKind::Switch ? switchStates_[meta->ordinal].label : nullptr;
        continue;
      }

      if (command->commandType == CLAY_RENDER_COMMAND_TYPE_IMAGE) {
        auto *meta = static_cast<NativeWidgetMeta *>(command->userData);
        if (!meta) continue;
        WidgetKey key{meta->ordinal, -1};
        seenKeys.insert(key);
        GtkWidget *widget = ensureWidget(key, *meta);
        positionWidget(widget, command->boundingBox);
        if (meta->kind == NativeWidgetKind::Icon) {
          ensureIconImage(widget, *meta);
        } else {
          ensureImageTexture(widget, *meta, (int)command->boundingBox.width, (int)command->boundingBox.height, command->renderData.image.cornerRadius);
        }
        pendingLabelTarget = nullptr;
        pendingButtonIconLabel = nullptr;
        pendingSwitchLabel = nullptr;
        pendingIsActionRow = false;
        continue;
      }

      if (command->commandType == CLAY_RENDER_COMMAND_TYPE_SCISSOR_START) {
        auto *meta = static_cast<NativeWidgetMeta *>(command->userData);
        if (meta && meta->kind == NativeWidgetKind::Sidebar) {
          seenSidebarOrdinals.insert(meta->ordinal);
          ensureSidebarPanel(meta->ordinal, command->boundingBox, meta->sidebarCompact);
        } else {
          ensureScrollContainer(command->id, command->boundingBox, command->renderData.clip);
        }
        pendingLabelTarget = nullptr;
        pendingButtonIconLabel = nullptr;
        pendingSwitchLabel = nullptr;
        pendingIsActionRow = false;
        continue;
      }

      if (command->commandType == CLAY_RENDER_COMMAND_TYPE_SCISSOR_END) {
        closeScrollContainer();
        pendingLabelTarget = nullptr;
        pendingButtonIconLabel = nullptr;
        pendingSwitchLabel = nullptr;
        pendingIsActionRow = false;
        continue;
      }

      if (command->commandType == CLAY_RENDER_COMMAND_TYPE_TEXT) {
        auto *flags = static_cast<TextStyleFlags *>(command->userData);
        std::string text(command->renderData.text.stringContents.chars, (size_t)command->renderData.text.stringContents.length);

        if (flags && flags->ownedByWidget) {
          if (pendingIsActionRow && pendingLabelTarget) {
#ifdef N8V_HAVE_ADWAITA
            if (pendingActionRowIsAdw) {
              adw_preferences_row_set_title(ADW_PREFERENCES_ROW(pendingLabelTarget), text.c_str());
            } else
#endif
            {
              gtk_label_set_text(GTK_LABEL(pendingLabelTarget), text.c_str());
            }
          } else if (pendingLabelTarget && (pendingKind == NativeWidgetKind::Checkbox || pendingKind == NativeWidgetKind::Radio)) {
            gtk_check_button_set_label(GTK_CHECK_BUTTON(pendingLabelTarget), text.c_str());
          } else if (pendingLabelTarget && pendingKind == NativeWidgetKind::Link) {
            gtk_label_set_markup(GTK_LABEL(pendingLabelTarget), linkMarkup(text, pendingLinkUrl).c_str());
          } else if (pendingButtonIconLabel) {
            gtk_label_set_text(GTK_LABEL(pendingButtonIconLabel), text.c_str());
            gtk_widget_set_visible(pendingButtonIconLabel, TRUE);
          } else if (pendingSwitchLabel) {
            gtk_label_set_text(GTK_LABEL(pendingSwitchLabel), text.c_str());
          } else if (pendingLabelTarget && pendingKind != NativeWidgetKind::Entry && pendingKind != NativeWidgetKind::Dropdown) {
            gtk_button_set_label(GTK_BUTTON(pendingLabelTarget), text.c_str());
          }
          pendingLabelTarget = nullptr;
          pendingButtonIconLabel = nullptr;
          pendingSwitchLabel = nullptr;
          pendingIsActionRow = false;
          continue;
        }

        if (!containerStack_.empty() && containerStack_.back().rowListBox) {
          setPlainLabelText(containerStack_.back().sidebarTitleLabel, text, flags && flags->bold, flags && flags->italic, flags && flags->strikethrough);
          pendingLabelTarget = nullptr;
          pendingButtonIconLabel = nullptr;
          pendingSwitchLabel = nullptr;
          pendingIsActionRow = false;
          continue;
        }

        int ordinal = flags ? flags->ordinal : -1;
        int wrapLineIndex = wrapLineCounts[ordinal]++;
        WidgetKey key{ordinal, wrapLineIndex};
        seenKeys.insert(key);
        GtkWidget *widget = ensureLabel(key);
        setPlainLabelText(widget, text, flags && flags->bold, flags && flags->italic, flags && flags->strikethrough);
        positionWidget(widget, command->boundingBox);
        pendingLabelTarget = nullptr;
        pendingButtonIconLabel = nullptr;
        pendingSwitchLabel = nullptr;
        pendingIsActionRow = false;
        continue;
      }

      pendingLabelTarget = nullptr;
      pendingButtonIconLabel = nullptr;
      pendingSwitchLabel = nullptr;
      pendingIsActionRow = false;
    }

    for (auto it = actionRows_.begin(); it != actionRows_.end();) {
      if (!seenActionRowOrdinals.count(it->first)) {
        gtk_list_box_remove(GTK_LIST_BOX(gtk_widget_get_parent(it->second)), it->second);
        buttonCallbacks_.erase(it->first);
        rowLabels_.erase(it->first);
        rowIcons_.erase(it->first);
        it = actionRows_.erase(it);
      } else {
        ++it;
      }
    }

    for (auto it = widgets_.begin(); it != widgets_.end();) {
      if (!seenKeys.count(it->first)) {
        destroyWidgetEntry(it->first, it->second);
        it = widgets_.erase(it);
      } else {
        ++it;
      }
    }

    for (auto it = scrollContainers_.begin(); it != scrollContainers_.end();) {
      if (!touchedScrollContainers_.count(it->first)) {
        gtk_widget_unparent(it->second.scrolled);
        it = scrollContainers_.erase(it);
      } else {
        ++it;
      }
    }

    for (auto it = sidebarPanels_.begin(); it != sidebarPanels_.end();) {
      if (!it->second.isPrimary && !seenSidebarOrdinals.count(it->first)) {
        gtk_widget_unparent(it->second.root);
        it = sidebarPanels_.erase(it);
      } else {
        ++it;
      }
    }
  }

  void setCursor(CursorKind) override {}

  void shutdown() override {
    if (measureLabel_) {
      g_object_unref(measureLabel_);
      measureLabel_ = nullptr;
    }
    if (measureButton_) {
      g_object_unref(measureButton_);
      measureButton_ = nullptr;
    }
    if (measureLink_) {
      g_object_unref(measureLink_);
      measureLink_ = nullptr;
    }
    if (measureEntry_) {
      g_object_unref(measureEntry_);
      measureEntry_ = nullptr;
    }
    if (measureCheckbox_) {
      g_object_unref(measureCheckbox_);
      measureCheckbox_ = nullptr;
    }
    if (measureRadio_) {
      g_object_unref(measureRadio_);
      measureRadio_ = nullptr;
    }
    if (measureDropdown_) {
      g_object_unref(measureDropdown_);
      measureDropdown_ = nullptr;
    }
    if (measureSlider_) {
      g_object_unref(measureSlider_);
      measureSlider_ = nullptr;
    }
    if (measureSwitch_) {
      g_object_unref(measureSwitch_);
      measureSwitch_ = nullptr;
    }
    if (window_) {
      gtk_window_destroy(GTK_WINDOW(window_));
      window_ = nullptr;
      fixed_ = nullptr;
      fixedScroll_ = nullptr;
      sidebarPanels_.clear();
      primarySidebarOrdinal_ = -1;
#ifdef N8V_HAVE_ADWAITA
      splitView_ = nullptr;
#endif
    }
  }

private:
  static gboolean onCloseRequest(GtkWindow *, gpointer userData) {
    static_cast<Gtk4Backend *>(userData)->closeRequested_ = true;
    return TRUE;
  }

  static void onPointerMotion(GtkEventControllerMotion *, gdouble x, gdouble y, gpointer userData) {
    auto *backend = static_cast<Gtk4Backend *>(userData);
    backend->pointerX_ = (float)x;
    backend->pointerY_ = (float)y;
  }
  static void onPointerPressed(GtkGestureClick *, gint, gdouble, gdouble, gpointer userData) { static_cast<Gtk4Backend *>(userData)->pointerDown_ = true; }
  static void onPointerReleased(GtkGestureClick *, gint, gdouble, gdouble, gpointer userData) { static_cast<Gtk4Backend *>(userData)->pointerDown_ = false; }

  struct OrdinalRef {
    Gtk4Backend *backend;
    int ordinal;
  };

  static void deleteOrdinalRef(gpointer data, GClosure *) { delete static_cast<OrdinalRef *>(data); }

  static void connectOrdinal(gpointer instance, const char *signal, GCallback handler, Gtk4Backend *backend, int ordinal) {
    g_signal_connect_data(instance, signal, handler, new OrdinalRef{backend, ordinal}, &Gtk4Backend::deleteOrdinalRef, (GConnectFlags)0);
  }

  static void onButtonClicked(GtkButton *, gpointer userData) {
    auto *ref = static_cast<OrdinalRef *>(userData);
    auto it = ref->backend->buttonCallbacks_.find(ref->ordinal);
    if (it != ref->backend->buttonCallbacks_.end() && it->second) it->second();
  }

  static void onSidebarRowActivated(GtkListBox *, GtkListBoxRow *row, gpointer userData) {
    auto *backend = static_cast<Gtk4Backend *>(userData);
    int ordinal = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(row), "n8v-ordinal"));
    auto it = backend->buttonCallbacks_.find(ordinal);
    if (it != backend->buttonCallbacks_.end() && it->second) it->second();
  }

  struct CheckboxState {
    bool *checked = nullptr;
    std::function<void(bool)> onChange;
  };

  static void onCheckboxToggled(GtkCheckButton *button, gpointer userData) {
    auto *ref = static_cast<OrdinalRef *>(userData);
    auto it = ref->backend->checkboxStates_.find(ref->ordinal);
    if (it == ref->backend->checkboxStates_.end()) return;
    CheckboxState &state = it->second;
    if (!state.checked) return;
    bool newValue = gtk_check_button_get_active(button);
    if (newValue == *state.checked) return;
    *state.checked = newValue;
    if (state.onChange) state.onChange(newValue);
  }

  struct SwitchState {
    GtkWidget *box = nullptr;
    GtkWidget *sw = nullptr;
    GtkWidget *label = nullptr;
    bool *checked = nullptr;
    std::function<void(bool)> onChange;
  };

  static void onSwitchToggled(GObject *object, GParamSpec *, gpointer userData) {
    auto *ref = static_cast<OrdinalRef *>(userData);
    auto it = ref->backend->switchStates_.find(ref->ordinal);
    if (it == ref->backend->switchStates_.end()) return;
    SwitchState &state = it->second;
    if (!state.checked) return;
    bool newValue = gtk_switch_get_active(GTK_SWITCH(object));
    if (newValue == *state.checked) return;
    *state.checked = newValue;
    if (state.onChange) state.onChange(newValue);
  }

  struct RadioState {
    int *selected = nullptr;
    int value = 0;
    std::function<void(int)> onChange;
  };

  static void onRadioToggled(GtkCheckButton *button, gpointer userData) {
    auto *ref = static_cast<OrdinalRef *>(userData);
    auto it = ref->backend->radioStates_.find(ref->ordinal);
    if (it == ref->backend->radioStates_.end()) return;
    RadioState &state = it->second;
    if (!state.selected || !gtk_check_button_get_active(button)) return;
    if (*state.selected == state.value) return;
    *state.selected = state.value;
    if (state.onChange) state.onChange(state.value);
  }

  struct DropdownState {
    int *selected = nullptr;
    std::function<void(int)> onChange;
  };

  static void onDropdownChanged(GObject *object, GParamSpec *, gpointer userData) {
    auto *ref = static_cast<OrdinalRef *>(userData);
    auto it = ref->backend->dropdownStates_.find(ref->ordinal);
    if (it == ref->backend->dropdownStates_.end()) return;
    DropdownState &state = it->second;
    if (!state.selected) return;
    int newValue = (int)gtk_drop_down_get_selected(GTK_DROP_DOWN(object));
    if (newValue == *state.selected) return;
    *state.selected = newValue;
    if (state.onChange) state.onChange(newValue);
  }

  struct SliderState {
    float *value = nullptr;
    std::function<void(float)> onChange;
  };

  static void onSliderChanged(GtkRange *range, gpointer userData) {
    auto *ref = static_cast<OrdinalRef *>(userData);
    auto it = ref->backend->sliderStates_.find(ref->ordinal);
    if (it == ref->backend->sliderStates_.end()) return;
    SliderState &state = it->second;
    if (!state.value) return;
    float newValue = (float)gtk_range_get_value(range);
    if (newValue == *state.value) return;
    *state.value = newValue;
    if (state.onChange) state.onChange(newValue);
  }

  struct EntryState {
    std::string *value = nullptr;
    std::function<void(std::string_view)> onChange;
    std::function<void()> onSubmit;
    std::string lastSynced;
  };

  static void onEntryActivated(GtkEntry *, gpointer userData) {
    auto *ref = static_cast<OrdinalRef *>(userData);
    auto it = ref->backend->entryStates_.find(ref->ordinal);
    if (it != ref->backend->entryStates_.end() && it->second.onSubmit) it->second.onSubmit();
  }

  static void syncEntry(GtkWidget *widget, const NativeWidgetMeta &meta, EntryState &state) {
    if (!meta.entryValue) return;
    state.value = meta.entryValue;
    state.onChange = toStdFunction(meta.onEntryChange, meta.onEntryChangeUserdata);
    state.onSubmit = toStdFunction(meta.onEntrySubmit, meta.onEntrySubmitUserdata);

    std::string widgetText = gtk_editable_get_text(GTK_EDITABLE(widget));
    if (widgetText != state.lastSynced) {
      *state.value = widgetText;
      state.lastSynced = widgetText;
      if (meta.entryBuf) ui_internal::writeToStringBuf(*state.value, *meta.entryBuf);
      if (state.onChange) state.onChange(widgetText);
    } else if (*state.value != state.lastSynced) {
      gtk_editable_set_text(GTK_EDITABLE(widget), state.value->c_str());
      state.lastSynced = *state.value;
    }
  }

  struct WidgetKey {
    int ordinal;
    int subIndex; // -1 for a button/link. 0, 1, 2... per wrapped line of standalone text
    bool operator<(const WidgetKey &other) const { return ordinal != other.ordinal ? ordinal < other.ordinal : subIndex < other.subIndex; }
  };

  void destroyWidgetEntry(const WidgetKey &key, GtkWidget *widget) {
    gtk_widget_unparent(widget);
    buttonCallbacks_.erase(key.ordinal);
    checkboxStates_.erase(key.ordinal);
    entryStates_.erase(key.ordinal);
    radioStates_.erase(key.ordinal);
    for (auto groupIt = radioGroups_.begin(); groupIt != radioGroups_.end();) {
      if (groupIt->second == widget) groupIt = radioGroups_.erase(groupIt);
      else ++groupIt;
    }
    dropdownStates_.erase(key.ordinal);
    sliderStates_.erase(key.ordinal);
    switchStates_.erase(key.ordinal);
    buttonIconStates_.erase(key.ordinal);
    if (panelStates_.erase(key.ordinal)) {
      panelCssRules_.erase(key.ordinal);
      reloadPanelCss();
    }
    imageTextureSources_.erase(widget);
    widgetKinds_.erase(key);
  }

  GtkWidget *ensureWidget(const WidgetKey &key, const NativeWidgetMeta &meta) {
    auto it = widgets_.find(key);
    bool kindMismatch = it != widgets_.end() && widgetKinds_[key] != meta.kind;
    if (!kindMismatch && it != widgets_.end() && meta.kind == NativeWidgetKind::Panel) {
      bool wasListItem = GTK_IS_BUTTON(it->second);
      bool wantListItem = meta.panelRole == n8v::PanelRole::ListItem;
      kindMismatch = wasListItem != wantListItem;
    }
    if (kindMismatch) {
      destroyWidgetEntry(key, it->second);
      widgets_.erase(it);
      it = widgets_.end();
    }
    if (it != widgets_.end()) {
      if (meta.kind == NativeWidgetKind::Button) {
        buttonCallbacks_[meta.ordinal] = toStdFunction(meta.onClick, meta.onClickUserdata);
        if (meta.buttonFlat) gtk_widget_add_css_class(it->second, "flat");
        else gtk_widget_remove_css_class(it->second, "flat");
      } else if (meta.kind == NativeWidgetKind::Checkbox && meta.checked) {
        CheckboxState &state = checkboxStates_[meta.ordinal];
        state.checked = meta.checked;
        state.onChange = toStdFunction(meta.onChange, meta.onChangeUserdata);
        gboolean current = gtk_check_button_get_active(GTK_CHECK_BUTTON(it->second));
        if ((bool)current != *meta.checked) gtk_check_button_set_active(GTK_CHECK_BUTTON(it->second), *meta.checked);
      } else if (meta.kind == NativeWidgetKind::Entry) {
        gtk_entry_set_visibility(GTK_ENTRY(it->second), !meta.password);
        gtk_entry_set_placeholder_text(GTK_ENTRY(it->second), meta.placeholder ? meta.placeholder->c_str() : "");
        syncEntry(it->second, meta, entryStates_[meta.ordinal]);
      } else if (meta.kind == NativeWidgetKind::Radio && meta.radioSelected) {
        RadioState &state = radioStates_[meta.ordinal];
        state.selected = meta.radioSelected;
        state.value = meta.radioValue;
        state.onChange = toStdFunction(meta.onRadioChange, meta.onRadioChangeUserdata);
        bool shouldBeActive = *meta.radioSelected == meta.radioValue;
        gboolean current = gtk_check_button_get_active(GTK_CHECK_BUTTON(it->second));
        if ((bool)current != shouldBeActive) gtk_check_button_set_active(GTK_CHECK_BUTTON(it->second), shouldBeActive);
      } else if (meta.kind == NativeWidgetKind::Switch && meta.checked) {
        SwitchState &state = switchStates_[meta.ordinal];
        state.checked = meta.checked;
        state.onChange = toStdFunction(meta.onChange, meta.onChangeUserdata);
        gboolean current = gtk_switch_get_active(GTK_SWITCH(state.sw));
        if ((bool)current != *meta.checked) gtk_switch_set_active(GTK_SWITCH(state.sw), *meta.checked);
      } else if (meta.kind == NativeWidgetKind::Dropdown && meta.dropdownSelected) {
        DropdownState &state = dropdownStates_[meta.ordinal];
        state.selected = meta.dropdownSelected;
        state.onChange = toStdFunction(meta.onDropdownChange, meta.onDropdownChangeUserdata);
        guint wantSelected = *meta.dropdownSelected >= 0 ? (guint)*meta.dropdownSelected : GTK_INVALID_LIST_POSITION;
        guint current = gtk_drop_down_get_selected(GTK_DROP_DOWN(it->second));
        if (current != wantSelected) gtk_drop_down_set_selected(GTK_DROP_DOWN(it->second), wantSelected);
      } else if (meta.kind == NativeWidgetKind::Panel) {
        if (meta.panelRole != n8v::PanelRole::ListItem) ensurePanelStyle(it->second, meta);
      } else if (meta.kind == NativeWidgetKind::Slider && meta.sliderValue) {
        SliderState &state = sliderStates_[meta.ordinal];
        state.value = meta.sliderValue;
        state.onChange = toStdFunction(meta.onSliderChange, meta.onSliderChangeUserdata);
        gtk_range_set_range(GTK_RANGE(it->second), meta.sliderMin, meta.sliderMax);
        if (gtk_range_get_value(GTK_RANGE(it->second)) != *meta.sliderValue) gtk_range_set_value(GTK_RANGE(it->second), *meta.sliderValue);
      }
      return it->second;
    }

    GtkWidget *widget = nullptr;
    if (meta.kind == NativeWidgetKind::Button) {
      widget = gtk_button_new_with_label("");
      buttonCallbacks_[meta.ordinal] = toStdFunction(meta.onClick, meta.onClickUserdata);
      connectOrdinal(widget, "clicked", G_CALLBACK(&Gtk4Backend::onButtonClicked), this, meta.ordinal);
      if (meta.buttonFlat) gtk_widget_add_css_class(widget, "flat");
    } else if (meta.kind == NativeWidgetKind::Checkbox) {
      widget = gtk_check_button_new();
      CheckboxState &state = checkboxStates_[meta.ordinal];
      state.checked = meta.checked;
      state.onChange = toStdFunction(meta.onChange, meta.onChangeUserdata);
      gtk_check_button_set_active(GTK_CHECK_BUTTON(widget), meta.checked && *meta.checked);
      connectOrdinal(widget, "toggled", G_CALLBACK(&Gtk4Backend::onCheckboxToggled), this, meta.ordinal);
    } else if (meta.kind == NativeWidgetKind::Entry) {
      widget = gtk_entry_new();
      gtk_entry_set_visibility(GTK_ENTRY(widget), !meta.password);
      gtk_entry_set_placeholder_text(GTK_ENTRY(widget), meta.placeholder ? meta.placeholder->c_str() : "");
      syncEntry(widget, meta, entryStates_[meta.ordinal]);
      connectOrdinal(widget, "activate", G_CALLBACK(&Gtk4Backend::onEntryActivated), this, meta.ordinal);
    } else if (meta.kind == NativeWidgetKind::Radio) {
      widget = gtk_check_button_new();
      RadioState &state = radioStates_[meta.ordinal];
      state.selected = meta.radioSelected;
      state.value = meta.radioValue;
      state.onChange = toStdFunction(meta.onRadioChange, meta.onRadioChangeUserdata);
      gtk_check_button_set_active(GTK_CHECK_BUTTON(widget), meta.radioSelected && *meta.radioSelected == meta.radioValue);
      if (meta.radioSelected) {
        GtkWidget *&leader = radioGroups_[meta.radioSelected];
        if (leader) gtk_check_button_set_group(GTK_CHECK_BUTTON(widget), GTK_CHECK_BUTTON(leader));
        else leader = widget;
      }
      connectOrdinal(widget, "toggled", G_CALLBACK(&Gtk4Backend::onRadioToggled), this, meta.ordinal);
    } else if (meta.kind == NativeWidgetKind::Switch) {
      SwitchState &state = switchStates_[meta.ordinal];
      state.box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
      state.sw = gtk_switch_new();
      state.label = gtk_label_new("");
      gtk_box_append(GTK_BOX(state.box), state.sw);
      gtk_box_append(GTK_BOX(state.box), state.label);
      state.checked = meta.checked;
      state.onChange = toStdFunction(meta.onChange, meta.onChangeUserdata);
      gtk_switch_set_active(GTK_SWITCH(state.sw), meta.checked && *meta.checked);
      connectOrdinal(state.sw, "notify::active", G_CALLBACK(&Gtk4Backend::onSwitchToggled), this, meta.ordinal);
      widget = state.box;
    } else if (meta.kind == NativeWidgetKind::Dropdown) {
      std::vector<const char *> cstrs;
      if (meta.dropdownItems) {
        cstrs.reserve(meta.dropdownItems->size() + 1);
        for (const std::string &item : *meta.dropdownItems) cstrs.push_back(item.c_str());
      }
      cstrs.push_back(nullptr);
      widget = gtk_drop_down_new_from_strings(cstrs.data());
      DropdownState &state = dropdownStates_[meta.ordinal];
      state.selected = meta.dropdownSelected;
      state.onChange = toStdFunction(meta.onDropdownChange, meta.onDropdownChangeUserdata);
      guint initialSelected = meta.dropdownSelected && *meta.dropdownSelected >= 0 ? (guint)*meta.dropdownSelected : GTK_INVALID_LIST_POSITION;
      gtk_drop_down_set_selected(GTK_DROP_DOWN(widget), initialSelected);
      connectOrdinal(widget, "notify::selected", G_CALLBACK(&Gtk4Backend::onDropdownChanged), this, meta.ordinal);
    } else if (meta.kind == NativeWidgetKind::Slider) {
      widget = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, meta.sliderMin, meta.sliderMax, (meta.sliderMax - meta.sliderMin) / 1000.0);
      gtk_scale_set_draw_value(GTK_SCALE(widget), FALSE);
      SliderState &state = sliderStates_[meta.ordinal];
      state.value = meta.sliderValue;
      state.onChange = toStdFunction(meta.onSliderChange, meta.onSliderChangeUserdata);
      if (meta.sliderValue) gtk_range_set_value(GTK_RANGE(widget), *meta.sliderValue);
      connectOrdinal(widget, "value-changed", G_CALLBACK(&Gtk4Backend::onSliderChanged), this, meta.ordinal);
    } else if (meta.kind == NativeWidgetKind::Image) {
      widget = gtk_picture_new();
      gtk_picture_set_content_fit(GTK_PICTURE(widget), GTK_CONTENT_FIT_FILL);
    } else if (meta.kind == NativeWidgetKind::Icon) {
      widget = gtk_image_new();
    } else if (meta.kind == NativeWidgetKind::Panel) {
      if (meta.panelRole == n8v::PanelRole::ListItem) {
        widget = gtk_button_new();
        gtk_widget_add_css_class(widget, "flat");
        gtk_widget_set_focusable(widget, FALSE);
      } else {
        widget = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_widget_set_can_target(widget, FALSE);
        ensurePanelStyle(widget, meta);
      }
    } else {
      widget = gtk_label_new("");
      gtk_label_set_use_markup(GTK_LABEL(widget), TRUE);
      gtk_label_set_xalign(GTK_LABEL(widget), 0.0f);
      gtk_widget_set_valign(widget, GTK_ALIGN_START);
    }

    gtk_fixed_put(GTK_FIXED(currentFixed()), widget, 0, 0);
    gtk_widget_set_visible(widget, TRUE);
    widgets_[key] = widget;
    widgetKinds_[key] = meta.kind;
    return widget;
  }

  void ensurePanelStyle(GtkWidget *widget, const NativeWidgetMeta &meta) {
#ifdef N8V_HAVE_ADWAITA
    gtk_widget_add_css_class(widget, "card");
    return;
#endif
    PanelState &state = panelStates_[meta.ordinal];
    bool changed = !state.initialized || state.background.r != meta.panelBackground.r || state.background.g != meta.panelBackground.g ||
                   state.background.b != meta.panelBackground.b || state.background.a != meta.panelBackground.a || state.borderColor.r != meta.panelBorderColor.r ||
                   state.borderColor.g != meta.panelBorderColor.g || state.borderColor.b != meta.panelBorderColor.b || state.borderColor.a != meta.panelBorderColor.a ||
                   state.borderWidth != meta.panelBorderWidth || state.cornerRadius.topLeft != meta.panelCornerRadius.topLeft ||
                   state.cornerRadius.topRight != meta.panelCornerRadius.topRight || state.cornerRadius.bottomLeft != meta.panelCornerRadius.bottomLeft ||
                   state.cornerRadius.bottomRight != meta.panelCornerRadius.bottomRight;
    if (!changed) return;
    state.initialized = true;
    state.background = meta.panelBackground;
    state.borderColor = meta.panelBorderColor;
    state.borderWidth = meta.panelBorderWidth;
    state.cornerRadius = meta.panelCornerRadius;

    if (!state.named) {
      std::string name = "n8v-panel-" + std::to_string(meta.ordinal);
      gtk_widget_set_name(widget, name.c_str());
      state.named = true;
    }

    char rule[512];
    std::snprintf(
      rule,
      sizeof(rule),
      "#n8v-panel-%d { background-color: rgba(%d,%d,%d,%.4f); border-style: solid; border-color: rgba(%d,%d,%d,%.4f); border-width: %.1fpx; "
      "border-top-left-radius: %.1fpx; border-top-right-radius: %.1fpx; border-bottom-left-radius: %.1fpx; border-bottom-right-radius: %.1fpx; }\n",
      meta.ordinal,
      (int)meta.panelBackground.r,
      (int)meta.panelBackground.g,
      (int)meta.panelBackground.b,
      meta.panelBackground.a / 255.0f,
      (int)meta.panelBorderColor.r,
      (int)meta.panelBorderColor.g,
      (int)meta.panelBorderColor.b,
      meta.panelBorderColor.a / 255.0f,
      meta.panelBorderWidth,
      meta.panelCornerRadius.topLeft,
      meta.panelCornerRadius.topRight,
      meta.panelCornerRadius.bottomLeft,
      meta.panelCornerRadius.bottomRight
    );
    panelCssRules_[meta.ordinal] = rule;
    reloadPanelCss();
  }

  GtkWidget *ensureButtonIcon(GtkWidget *button, const NativeWidgetMeta &meta) {
    ButtonIconState &state = buttonIconStates_[meta.ordinal];
    bool wantIcon = meta.iconName != nullptr || meta.image != nullptr;
    if (!wantIcon) {
      if (state.hasIcon) buttonIconStates_.erase(meta.ordinal);
      return nullptr;
    }

    if (meta.buttonFlat) gtk_widget_add_css_class(button, "n8v-icon-only");

    if (!state.hasIcon) {
      state.box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
      gtk_widget_set_halign(state.box, GTK_ALIGN_CENTER);
      gtk_widget_set_valign(state.box, GTK_ALIGN_CENTER);
      state.image = gtk_image_new();
      state.label = gtk_label_new("");
      gtk_widget_set_visible(state.label, FALSE);
      if (meta.iconTrailing) {
        gtk_box_append(GTK_BOX(state.box), state.label);
        gtk_box_append(GTK_BOX(state.box), state.image);
      } else {
        gtk_box_append(GTK_BOX(state.box), state.image);
        gtk_box_append(GTK_BOX(state.box), state.label);
      }
      gtk_button_set_child(GTK_BUTTON(button), state.box);
      state.hasIcon = true;
    }

    const char *freedesktopName = meta.iconName ? n8v::detail::resolveFreedesktopIconName(meta.iconName) : nullptr;
    GtkIconTheme *theme = gtk_icon_theme_get_for_display(gtk_widget_get_display(button));
    if (freedesktopName && gtk_icon_theme_has_icon(theme, freedesktopName)) {
      gtk_image_set_from_icon_name(GTK_IMAGE(state.image), freedesktopName);
      state.fallbackSource = nullptr;
    } else if (meta.iconName && meta.image) {
      GdkRGBA fg;
      gtk_widget_get_color(button, &fg);
      const n8v::detail::DecodedImage *icon =
        n8v::detail::getOrDecodeIcon(meta.iconName, (uint16_t)meta.image->width, n8v::Color{fg.red * 255.0f, fg.green * 255.0f, fg.blue * 255.0f, 255.0f});
      if (icon && state.fallbackSource != icon) {
        gsize size = (gsize)icon->width * (gsize)icon->height * 4;
        GBytes *bytes = g_bytes_new(icon->rgba, size);
        GdkTexture *texture = gdk_memory_texture_new(icon->width, icon->height, GDK_MEMORY_R8G8B8A8, bytes, (gsize)icon->width * 4);
        g_bytes_unref(bytes);
        gtk_image_set_from_paintable(GTK_IMAGE(state.image), GDK_PAINTABLE(texture));
        g_object_unref(texture);
        state.fallbackSource = icon;
      }
    }
    return state.label;
  }

  void ensureIconImage(GtkWidget *widget, const NativeWidgetMeta &meta) {
    const char *freedesktopName = meta.iconName ? n8v::detail::resolveFreedesktopIconName(meta.iconName) : nullptr;
    GtkIconTheme *theme = gtk_icon_theme_get_for_display(gtk_widget_get_display(widget));
    if (freedesktopName && gtk_icon_theme_has_icon(theme, freedesktopName)) {
      gtk_image_set_from_icon_name(GTK_IMAGE(widget), freedesktopName);
      imageTextureSources_.erase(widget);
      return;
    }

    if (!meta.image) return;
    auto it = imageTextureSources_.find(widget);
    if (it != imageTextureSources_.end() && it->second == meta.image) return;

    gsize size = (gsize)meta.image->width * (gsize)meta.image->height * 4;
    GBytes *bytes = g_bytes_new(meta.image->rgba, size);
    GdkTexture *texture = gdk_memory_texture_new(meta.image->width, meta.image->height, GDK_MEMORY_R8G8B8A8, bytes, (gsize)meta.image->width * 4);
    g_bytes_unref(bytes);
    gtk_image_set_from_paintable(GTK_IMAGE(widget), GDK_PAINTABLE(texture));
    g_object_unref(texture);
    imageTextureSources_[widget] = meta.image;
  }

  void ensureImageTexture(GtkWidget *widget, const NativeWidgetMeta &meta, int targetW, int targetH, const Clay_CornerRadius &corner) {
    if (!meta.image) return;
    const n8v::detail::DecodedImage *image =
      n8v::detail::getOrBakeRoundedImage(meta.image, targetW, targetH, corner.topLeft, corner.topRight, corner.bottomLeft, corner.bottomRight);
    auto it = imageTextureSources_.find(widget);
    if (it != imageTextureSources_.end() && it->second == image) return;

    gsize size = (gsize)image->width * (gsize)image->height * 4;
    GBytes *bytes = g_bytes_new(image->rgba, size);
    GdkTexture *texture = gdk_memory_texture_new(image->width, image->height, GDK_MEMORY_R8G8B8A8, bytes, (gsize)image->width * 4);
    g_bytes_unref(bytes);
    gtk_picture_set_paintable(GTK_PICTURE(widget), GDK_PAINTABLE(texture));
    g_object_unref(texture);
    imageTextureSources_[widget] = image;
  }

  GtkWidget *ensureLabel(const WidgetKey &key) {
    auto it = widgets_.find(key);
    if (it != widgets_.end()) return it->second;

    GtkWidget *label = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_widget_set_valign(label, GTK_ALIGN_START);
    gtk_fixed_put(GTK_FIXED(currentFixed()), label, 0, 0);
    gtk_widget_set_visible(label, TRUE);
    widgets_[key] = label;
    return label;
  }

  void positionWidget(GtkWidget *widget, const Clay_BoundingBox &box) {
    float originX = containerStack_.empty() ? rootOriginX_ : containerStack_.back().originX;
    float originY = containerStack_.empty() ? 0.0f : containerStack_.back().originY;
    GtkWidget *fixed = currentFixed();
    if (gtk_widget_get_parent(widget) != fixed) {
      g_object_ref(widget);
      gtk_widget_unparent(widget);
      gtk_fixed_put(GTK_FIXED(fixed), widget, 0, 0);
      g_object_unref(widget);
    }
    gtk_fixed_move(GTK_FIXED(fixed), widget, box.x - originX, box.y - originY);
    gtk_widget_set_size_request(widget, (int)box.width, (int)box.height);
  }

  GtkWidget *currentFixed() const { return containerStack_.empty() ? fixed_ : containerStack_.back().fixed; }

  struct ContainerFrame {
    GtkWidget *scrolled = nullptr;
    GtkWidget *fixed = nullptr;
    float originX = 0.0f;
    float originY = 0.0f;
    GtkWidget *rowListBox = nullptr;
    GtkWidget *sidebarTitleLabel = nullptr;
  };

  struct SidebarPanel {
    GtkWidget *root = nullptr;
    GtkWidget *titleLabel = nullptr;
    GtkWidget *listBox = nullptr;
    bool isPrimary = false;
  };

  void ensureScrollContainer(uint32_t id, const Clay_BoundingBox &box, const Clay_ClipRenderData &clip) {
    auto it = scrollContainers_.find(id);
    if (it == scrollContainers_.end()) {
      GtkWidget *scrolled = gtk_scrolled_window_new();
      GtkWidget *innerFixed = gtk_fixed_new();
      gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), innerFixed);
      gtk_fixed_put(GTK_FIXED(currentFixed()), scrolled, 0, 0);
      gtk_widget_set_visible(scrolled, TRUE);
      it = scrollContainers_.emplace(id, ContainerFrame{scrolled, innerFixed, box.x, box.y}).first;
    }
    ContainerFrame &frame = it->second;
    gtk_scrolled_window_set_policy(
      GTK_SCROLLED_WINDOW(frame.scrolled),
      clip.horizontal ? GTK_POLICY_AUTOMATIC : GTK_POLICY_EXTERNAL,
      clip.vertical ? GTK_POLICY_AUTOMATIC : GTK_POLICY_EXTERNAL
    );
    float originX = containerStack_.empty() ? rootOriginX_ : containerStack_.back().originX;
    float originY = containerStack_.empty() ? 0.0f : containerStack_.back().originY;
    GtkWidget *fixed = currentFixed();
    if (gtk_widget_get_parent(frame.scrolled) != fixed) {
      g_object_ref(frame.scrolled);
      gtk_widget_unparent(frame.scrolled);
      gtk_fixed_put(GTK_FIXED(fixed), frame.scrolled, 0, 0);
      g_object_unref(frame.scrolled);
    }
    gtk_fixed_move(GTK_FIXED(fixed), frame.scrolled, box.x - originX, box.y - originY);
    gtk_widget_set_size_request(frame.scrolled, (int)box.width, (int)box.height);
    frame.originX = box.x;
    frame.originY = box.y;

    Clay_ScrollContainerData scrollData = Clay_GetScrollContainerData(Clay_ElementId{id});
    if (scrollData.found) {
      gtk_widget_set_size_request(frame.fixed, (int)scrollData.contentDimensions.width, (int)scrollData.contentDimensions.height);
    }

    containerStack_.push_back(frame);
    touchedScrollContainers_.insert(id);
  }

  void closeScrollContainer() {
    if (!containerStack_.empty()) containerStack_.pop_back();
  }

  SidebarPanel &ensureSidebarPanel(int ordinal, const Clay_BoundingBox &box, bool /*compact*/) {
    if (primarySidebarOrdinal_ < 0) primarySidebarOrdinal_ = ordinal;
    bool isPrimary = ordinal == primarySidebarOrdinal_;

    auto it = sidebarPanels_.find(ordinal);
    if (it == sidebarPanels_.end()) {
      SidebarPanel panel;
      panel.isPrimary = isPrimary;

      panel.titleLabel = gtk_label_new("");
      gtk_widget_add_css_class(panel.titleLabel, "heading");
      gtk_label_set_xalign(GTK_LABEL(panel.titleLabel), 0.5f);
      gtk_widget_set_margin_top(panel.titleLabel, 12);
      gtk_widget_set_margin_bottom(panel.titleLabel, 12);

      panel.listBox = gtk_list_box_new();
      gtk_widget_add_css_class(panel.listBox, "navigation-sidebar");
      gtk_list_box_set_selection_mode(GTK_LIST_BOX(panel.listBox), GTK_SELECTION_SINGLE);
      gtk_list_box_set_sort_func(GTK_LIST_BOX(panel.listBox), compareActionRowOrdinals, nullptr, nullptr);
      g_signal_connect_data(panel.listBox, "row-activated", G_CALLBACK(&Gtk4Backend::onSidebarRowActivated), this, nullptr, (GConnectFlags)0);

      GtkWidget *scrolled = gtk_scrolled_window_new();
      gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), GTK_POLICY_EXTERNAL, GTK_POLICY_AUTOMATIC);
      gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), panel.listBox);
      gtk_widget_set_vexpand(scrolled, TRUE);

      panel.root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
      gtk_box_append(GTK_BOX(panel.root), panel.titleLabel);
      gtk_box_append(GTK_BOX(panel.root), scrolled);

      if (isPrimary) {
#ifdef N8V_HAVE_ADWAITA
        g_object_ref(fixedScroll_);
        gtk_window_set_child(GTK_WINDOW(window_), nullptr);

        GtkWidget *splitViewWidget = adw_navigation_split_view_new();
        splitView_ = ADW_NAVIGATION_SPLIT_VIEW(splitViewWidget);
        AdwNavigationPage *sidebarPage = adw_navigation_page_new(panel.root, "Sidebar");
        AdwNavigationPage *contentPage = adw_navigation_page_new(fixedScroll_, "Content");
        adw_navigation_split_view_set_sidebar(splitView_, sidebarPage);
        adw_navigation_split_view_set_content(splitView_, contentPage);
        g_object_unref(fixedScroll_);

        gtk_window_set_child(GTK_WINDOW(window_), splitViewWidget);
#else
        g_object_ref(fixedScroll_);
        gtk_window_set_child(GTK_WINDOW(window_), nullptr);

        GtkWidget *box_ = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_widget_add_css_class(panel.root, "sidebar");
        gtk_box_append(GTK_BOX(box_), panel.root);
        gtk_box_append(GTK_BOX(box_), fixedScroll_);
        g_object_unref(fixedScroll_);

        gtk_window_set_child(GTK_WINDOW(window_), box_);
#endif
      } else {
        gtk_widget_add_css_class(panel.root, "sidebar");
        gtk_widget_set_overflow(panel.root, GTK_OVERFLOW_HIDDEN);
        gtk_fixed_put(GTK_FIXED(currentFixed()), panel.root, 0, 0);
        gtk_widget_set_visible(panel.root, TRUE);
      }

      it = sidebarPanels_.emplace(ordinal, panel).first;
    }

    SidebarPanel &panel = it->second;
    if (panel.isPrimary) {
      rootOriginX_ = box.width;
#ifdef N8V_HAVE_ADWAITA
      if (splitView_) {
        adw_navigation_split_view_set_min_sidebar_width(splitView_, box.width);
        adw_navigation_split_view_set_max_sidebar_width(splitView_, box.width);
      }
#endif
    } else {
      positionWidget(panel.root, box);
    }

    containerStack_.push_back(ContainerFrame{nullptr, nullptr, box.x, box.y, panel.listBox, panel.titleLabel});
    return panel;
  }

  GtkWidget *ensureActionRow(ContainerFrame &listFrame, const NativeWidgetMeta &meta) {
    GtkWidget *&row = actionRows_[meta.ordinal];
    bool wantAdw = useAdwActionRow(meta);

    if (row && GPOINTER_TO_INT(g_object_get_data(G_OBJECT(row), "n8v-adw")) != (wantAdw ? 1 : 0)) {
      gtk_list_box_remove(GTK_LIST_BOX(listFrame.rowListBox), row);
      buttonCallbacks_.erase(meta.ordinal);
      rowLabels_.erase(meta.ordinal);
      rowIcons_.erase(meta.ordinal);
      row = nullptr;
    }

    if (!row) {
#ifdef N8V_HAVE_ADWAITA
      if (wantAdw) {
        row = adw_action_row_new();
        g_object_set_data(G_OBJECT(row), "n8v-ordinal", GINT_TO_POINTER(meta.ordinal));
        g_object_set_data(G_OBJECT(row), "n8v-adw", GINT_TO_POINTER(1));
        gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(row), TRUE);
        GtkWidget *icon = gtk_image_new();
        adw_action_row_add_prefix(ADW_ACTION_ROW(row), icon);
        gtk_widget_set_visible(icon, FALSE);
        rowIcons_[meta.ordinal] = icon;
        gtk_list_box_append(GTK_LIST_BOX(listFrame.rowListBox), row);
        connectOrdinal(row, "activated", G_CALLBACK(&Gtk4Backend::onButtonClicked), this, meta.ordinal);
      } else
#endif
      {
        row = gtk_list_box_row_new();
        g_object_set_data(G_OBJECT(row), "n8v-ordinal", GINT_TO_POINTER(meta.ordinal));
        g_object_set_data(G_OBJECT(row), "n8v-adw", GINT_TO_POINTER(0));

        GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        int vmargin = meta.sidebarCompact ? 2 : 8;
        gtk_widget_set_margin_start(hbox, 12);
        gtk_widget_set_margin_end(hbox, 12);
        gtk_widget_set_margin_top(hbox, vmargin);
        gtk_widget_set_margin_bottom(hbox, vmargin);

        GtkWidget *icon = gtk_image_new();
        gtk_widget_set_visible(icon, FALSE);
        gtk_box_append(GTK_BOX(hbox), icon);
        rowIcons_[meta.ordinal] = icon;

        GtkWidget *label = gtk_label_new("");
        gtk_widget_set_halign(label, GTK_ALIGN_START);
        gtk_box_append(GTK_BOX(hbox), label);
        rowLabels_[meta.ordinal] = label;

        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), hbox);
        gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(row), TRUE);
        gtk_list_box_append(GTK_LIST_BOX(listFrame.rowListBox), row);
        connectOrdinal(row, "activate", G_CALLBACK(&Gtk4Backend::onButtonClicked), this, meta.ordinal);
      }
    }
    buttonCallbacks_[meta.ordinal] = toStdFunction(meta.onClick, meta.onClickUserdata);

    auto rowIconIt = rowIcons_.find(meta.ordinal);
    if (rowIconIt != rowIcons_.end()) {
      const char *freedesktopName = meta.iconName ? n8v::detail::resolveFreedesktopIconName(meta.iconName) : nullptr;
      if (freedesktopName) {
        gtk_image_set_from_icon_name(GTK_IMAGE(rowIconIt->second), freedesktopName);
        gtk_widget_set_visible(rowIconIt->second, TRUE);
      } else {
        gtk_widget_set_visible(rowIconIt->second, FALSE);
      }
    }

    if (meta.buttonSelected) gtk_list_box_select_row(GTK_LIST_BOX(listFrame.rowListBox), GTK_LIST_BOX_ROW(row));
    return wantAdw ? row : rowLabels_[meta.ordinal];
  }

  GtkWidget *window_ = nullptr;
  GtkWidget *fixed_ = nullptr;
  GtkWidget *fixedScroll_ = nullptr;
  GtkCssProvider *panelCssProvider_ = nullptr;
  std::unordered_map<int, std::string> panelCssRules_;

  void reloadPanelCss() {
    std::string css;
    for (const auto &[ordinal, rule] : panelCssRules_) css += rule;
    gtk_css_provider_load_from_string(panelCssProvider_, css.c_str());
  }
  std::unordered_map<int, SidebarPanel> sidebarPanels_;
  int primarySidebarOrdinal_ = -1;
  float rootOriginX_ = 0.0f;
#ifdef N8V_HAVE_ADWAITA
  AdwNavigationSplitView *splitView_ = nullptr;
#endif
  GtkWidget *measureLabel_ = nullptr;
  GtkWidget *measureButton_ = nullptr;
  GtkWidget *measureLink_ = nullptr;
  GtkWidget *measureEntry_ = nullptr;
  GtkWidget *measureCheckbox_ = nullptr;
  GtkWidget *measureRadio_ = nullptr;
  GtkWidget *measureDropdown_ = nullptr;
  GtkWidget *measureSlider_ = nullptr;
  GtkWidget *measureSwitch_ = nullptr;
  bool closeRequested_ = false;
  float pointerX_ = 0.0f;
  float pointerY_ = 0.0f;
  bool pointerDown_ = false;

  std::map<WidgetKey, GtkWidget *> widgets_;
  std::map<WidgetKey, NativeWidgetKind> widgetKinds_;
  std::unordered_map<int, GtkWidget *> actionRows_;
  std::unordered_map<int, GtkWidget *> rowLabels_;
  std::unordered_map<int, GtkWidget *> rowIcons_;
  std::unordered_map<int, std::function<void()>> buttonCallbacks_;
  std::unordered_map<int, CheckboxState> checkboxStates_;
  std::unordered_map<int, SwitchState> switchStates_;
  std::unordered_map<int, EntryState> entryStates_;
  std::unordered_map<int, RadioState> radioStates_;
  std::unordered_map<int *, GtkWidget *> radioGroups_;
  std::unordered_map<int, DropdownState> dropdownStates_;
  std::unordered_map<int, SliderState> sliderStates_;
  std::unordered_map<GtkWidget *, const void *> imageTextureSources_;
  struct ButtonIconState {
    bool hasIcon = false;
    GtkWidget *box = nullptr;
    GtkWidget *image = nullptr;
    GtkWidget *label = nullptr;
    const void *fallbackSource = nullptr;
  };
  std::unordered_map<int, ButtonIconState> buttonIconStates_;
  struct PanelState {
    bool initialized = false;
    bool named = false;
    n8v::Color background{};
    n8v::Color borderColor{};
    float borderWidth = -1.0f;
    n8v::CornerRadius cornerRadius{};
  };
  std::unordered_map<int, PanelState> panelStates_;
  std::unordered_map<uint32_t, ContainerFrame> scrollContainers_;
  std::vector<ContainerFrame> containerStack_;
  std::set<uint32_t> touchedScrollContainers_;
};

} // namespace

std::unique_ptr<Backend> makeGtk4Backend() { return std::make_unique<Gtk4Backend>(); }

} // namespace n8v::detail
