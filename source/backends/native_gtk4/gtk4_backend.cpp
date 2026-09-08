#include "gtk4_backend.hpp"

#include "core/native_widget_meta.hpp"
#include "core/text_style_flags.hpp"

#include <gtk/gtk.h>

// fix conflict with function of same name
#undef g_object_ref_sink

#include <cstdio>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>

extern "C" {
gulong g_signal_connect_data(gpointer instance, const gchar *detailed_signal, GCallback c_handler, gpointer data, GClosureNotify destroy_data, GConnectFlags connect_flags);
gpointer g_object_ref_sink(gpointer object);
void g_object_unref(gpointer object);
}

namespace n8v::detail {
namespace {

class Gtk4Backend final : public Backend {
public:
  ~Gtk4Backend() override { shutdown(); }

  bool initialize(int width, int height, std::string_view title) override {
    gtk_init();

    window_ = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(window_), std::string(title).c_str());
    gtk_window_set_default_size(GTK_WINDOW(window_), width, height);

    fixed_ = gtk_fixed_new();
    gtk_window_set_child(GTK_WINDOW(window_), fixed_);

    g_signal_connect_data(window_, "close-request", G_CALLBACK(&Gtk4Backend::onCloseRequest), this, nullptr, (GConnectFlags)0);

    gtk_window_present(GTK_WINDOW(window_));

    measureLabel_ = gtk_label_new("");
    g_object_ref_sink(measureLabel_);

    measureButton_ = gtk_button_new_with_label("");
    g_object_ref_sink(measureButton_);

    measureLink_ = gtk_link_button_new_with_label("about:blank", "");
    g_object_ref_sink(measureLink_);

    measureEntry_ = gtk_entry_new();
    g_object_ref_sink(measureEntry_);

    return true;
  }

  bool pumpEvents() override {
    GMainContext *context = g_main_context_default();
    while (g_main_context_pending(context)) {
      g_main_context_iteration(context, FALSE);
    }
    return !closeRequested_;
  }

  bool pointerDown() const override { return false; }

  Clay_Dimensions windowSize() const override { return {(float)gtk_widget_get_width(fixed_), (float)gtk_widget_get_height(fixed_)}; }

  Clay_Dimensions measureText(std::string_view text, FontFamily, uint16_t, bool, bool) const override {
    gtk_label_set_text(GTK_LABEL(measureLabel_), std::string(text).c_str());
    int minW = 0, natW = 0, minH = 0, natH = 0;
    gtk_widget_measure(measureLabel_, GTK_ORIENTATION_HORIZONTAL, -1, &minW, &natW, nullptr, nullptr);
    gtk_widget_measure(measureLabel_, GTK_ORIENTATION_VERTICAL, -1, &minH, &natH, nullptr, nullptr);
    return {(float)natW, (float)natH};
  }

  Clay_Dimensions measureNativeChrome(NativeWidgetKind kind, std::string_view text, uint16_t) const override {
    if (kind == NativeWidgetKind::Entry) {
      int minW = 0, natW = 0, minH = 0, natH = 0;
      gtk_widget_measure(measureEntry_, GTK_ORIENTATION_HORIZONTAL, -1, &minW, &natW, nullptr, nullptr);
      gtk_widget_measure(measureEntry_, GTK_ORIENTATION_VERTICAL, -1, &minH, &natH, nullptr, nullptr);
      return {0, (float)natH};
    }
    GtkWidget *probe = kind == NativeWidgetKind::Button ? measureButton_ : measureLink_;
    gtk_button_set_label(GTK_BUTTON(probe), std::string(text).c_str());
    int minW = 0, natW = 0, minH = 0, natH = 0;
    gtk_widget_measure(probe, GTK_ORIENTATION_HORIZONTAL, -1, &minW, &natW, nullptr, nullptr);
    gtk_widget_measure(probe, GTK_ORIENTATION_VERTICAL, -1, &minH, &natH, nullptr, nullptr);
    return {(float)natW, (float)natH};
  }

  void beginFrame() override {}

  void present(Clay_RenderCommandArray commands) override {
    std::map<int, int> wrapLineCounts;
    std::set<WidgetKey> seenKeys;
    GtkWidget *pendingLabelTarget = nullptr;
    NativeWidgetKind pendingKind = NativeWidgetKind::Button;

    for (int32_t i = 0; i < commands.length; ++i) {
      Clay_RenderCommand *command = Clay_RenderCommandArray_Get(&commands, i);

      if (command->commandType == CLAY_RENDER_COMMAND_TYPE_RECTANGLE) {
        auto *meta = static_cast<NativeWidgetMeta *>(command->userData);
        if (!meta) {
          pendingLabelTarget = nullptr;
          continue;
        }
        WidgetKey key{meta->ordinal, -1};
        seenKeys.insert(key);
        GtkWidget *widget = ensureWidget(key, *meta);
        positionWidget(widget, command->boundingBox);
        pendingLabelTarget = widget;
        pendingKind = meta->kind;
        continue;
      }

      if (command->commandType == CLAY_RENDER_COMMAND_TYPE_TEXT) {
        auto *flags = static_cast<TextStyleFlags *>(command->userData);
        std::string text(command->renderData.text.stringContents.chars, (size_t)command->renderData.text.stringContents.length);

        if (flags && flags->ownedByWidget) {
          if (pendingLabelTarget && pendingKind == NativeWidgetKind::Checkbox) {
            gtk_check_button_set_label(GTK_CHECK_BUTTON(pendingLabelTarget), text.c_str());
          } else if (pendingLabelTarget && pendingKind != NativeWidgetKind::Entry) {
            gtk_button_set_label(GTK_BUTTON(pendingLabelTarget), text.c_str());
          }
          pendingLabelTarget = nullptr;
          continue;
        }

        int ordinal = flags ? flags->ordinal : -1;
        int wrapLineIndex = wrapLineCounts[ordinal]++;
        WidgetKey key{ordinal, wrapLineIndex};
        seenKeys.insert(key);
        GtkWidget *widget = ensureLabel(key);
        gtk_label_set_text(GTK_LABEL(widget), text.c_str());
        positionWidget(widget, command->boundingBox);
        pendingLabelTarget = nullptr;
        continue;
      }

      pendingLabelTarget = nullptr;
    }

    for (auto it = widgets_.begin(); it != widgets_.end();) {
      if (!seenKeys.count(it->first)) {
        gtk_fixed_remove(GTK_FIXED(fixed_), it->second);
        buttonCallbacks_.erase(it->first.ordinal);
        checkboxStates_.erase(it->first.ordinal);
        entryStates_.erase(it->first.ordinal);
        it = widgets_.erase(it);
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
    if (window_) {
      gtk_window_destroy(GTK_WINDOW(window_));
      window_ = nullptr;
      fixed_ = nullptr;
    }
  }

private:
  static gboolean onCloseRequest(GtkWindow *, gpointer userData) {
    static_cast<Gtk4Backend *>(userData)->closeRequested_ = true;
    return TRUE;
  }

  static void onButtonClicked(GtkButton *, gpointer userData) {
    auto *callback = static_cast<std::function<void()> *>(userData);
    if (callback && *callback) (*callback)();
  }

  struct CheckboxState {
    bool *checked = nullptr;
    std::function<void(bool)> onChange;
  };

  static void onCheckboxToggled(GtkCheckButton *button, gpointer userData) {
    auto *state = static_cast<CheckboxState *>(userData);
    if (!state || !state->checked) return;
    bool newValue = gtk_check_button_get_active(button);
    if (newValue == *state->checked) return;
    *state->checked = newValue;
    if (state->onChange) state->onChange(newValue);
  }

  struct EntryState {
    std::string *value = nullptr;
    std::function<void(std::string_view)> onChange;
    std::string lastSynced;
  };

  static void syncEntry(GtkWidget *widget, const NativeWidgetMeta &meta, EntryState &state) {
    if (!meta.entryValue) return;
    state.value = meta.entryValue;
    state.onChange = meta.onEntryChange ? *meta.onEntryChange : std::function<void(std::string_view)>{};

    std::string widgetText = gtk_editable_get_text(GTK_EDITABLE(widget));
    if (widgetText != state.lastSynced) {
      *state.value = widgetText;
      state.lastSynced = widgetText;
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

  GtkWidget *ensureWidget(const WidgetKey &key, const NativeWidgetMeta &meta) {
    auto it = widgets_.find(key);
    if (it != widgets_.end()) {
      if (meta.kind == NativeWidgetKind::Button) {
        buttonCallbacks_[meta.ordinal] = meta.onClick ? *meta.onClick : std::function<void()>{};
      } else if (meta.kind == NativeWidgetKind::Link && meta.url) {
        gtk_link_button_set_uri(GTK_LINK_BUTTON(it->second), meta.url->c_str());
      } else if (meta.kind == NativeWidgetKind::Checkbox && meta.checked) {
        CheckboxState &state = checkboxStates_[meta.ordinal];
        state.checked = meta.checked;
        state.onChange = meta.onChange ? *meta.onChange : std::function<void(bool)>{};
        gboolean current = gtk_check_button_get_active(GTK_CHECK_BUTTON(it->second));
        if ((bool)current != *meta.checked) gtk_check_button_set_active(GTK_CHECK_BUTTON(it->second), *meta.checked);
      } else if (meta.kind == NativeWidgetKind::Entry) {
        gtk_entry_set_visibility(GTK_ENTRY(it->second), !meta.password);
        gtk_entry_set_placeholder_text(GTK_ENTRY(it->second), meta.placeholder ? meta.placeholder->c_str() : "");
        syncEntry(it->second, meta, entryStates_[meta.ordinal]);
      }
      return it->second;
    }

    GtkWidget *widget = nullptr;
    if (meta.kind == NativeWidgetKind::Button) {
      widget = gtk_button_new_with_label("");
      buttonCallbacks_[meta.ordinal] = meta.onClick ? *meta.onClick : std::function<void()>{};
      g_signal_connect_data(widget, "clicked", G_CALLBACK(&Gtk4Backend::onButtonClicked), &buttonCallbacks_[meta.ordinal], nullptr, (GConnectFlags)0);
    } else if (meta.kind == NativeWidgetKind::Checkbox) {
      widget = gtk_check_button_new();
      CheckboxState &state = checkboxStates_[meta.ordinal];
      state.checked = meta.checked;
      state.onChange = meta.onChange ? *meta.onChange : std::function<void(bool)>{};
      gtk_check_button_set_active(GTK_CHECK_BUTTON(widget), meta.checked && *meta.checked);
      g_signal_connect_data(widget, "toggled", G_CALLBACK(&Gtk4Backend::onCheckboxToggled), &checkboxStates_[meta.ordinal], nullptr, (GConnectFlags)0);
    } else if (meta.kind == NativeWidgetKind::Entry) {
      widget = gtk_entry_new();
      gtk_entry_set_visibility(GTK_ENTRY(widget), !meta.password);
      gtk_entry_set_placeholder_text(GTK_ENTRY(widget), meta.placeholder ? meta.placeholder->c_str() : "");
      syncEntry(widget, meta, entryStates_[meta.ordinal]);
    } else {
      widget = gtk_link_button_new(meta.url ? meta.url->c_str() : "");
    }

    gtk_fixed_put(GTK_FIXED(fixed_), widget, 0, 0);
    gtk_widget_set_visible(widget, TRUE);
    widgets_[key] = widget;
    return widget;
  }

  GtkWidget *ensureLabel(const WidgetKey &key) {
    auto it = widgets_.find(key);
    if (it != widgets_.end()) return it->second;

    GtkWidget *label = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_widget_set_valign(label, GTK_ALIGN_START);
    gtk_fixed_put(GTK_FIXED(fixed_), label, 0, 0);
    gtk_widget_set_visible(label, TRUE);
    widgets_[key] = label;
    return label;
  }

  void positionWidget(GtkWidget *widget, const Clay_BoundingBox &box) {
    gtk_fixed_move(GTK_FIXED(fixed_), widget, box.x, box.y);
    gtk_widget_set_size_request(widget, (int)box.width, (int)box.height);
  }

  GtkWidget *window_ = nullptr;
  GtkWidget *fixed_ = nullptr;
  GtkWidget *measureLabel_ = nullptr;
  GtkWidget *measureButton_ = nullptr;
  GtkWidget *measureLink_ = nullptr;
  GtkWidget *measureEntry_ = nullptr;
  bool closeRequested_ = false;

  std::map<WidgetKey, GtkWidget *> widgets_;
  std::unordered_map<int, std::function<void()>> buttonCallbacks_;
  std::unordered_map<int, CheckboxState> checkboxStates_;
  std::unordered_map<int, EntryState> entryStates_;
};

} // namespace

std::unique_ptr<Backend> makeGtk4Backend() { return std::make_unique<Gtk4Backend>(); }

} // namespace n8v::detail
