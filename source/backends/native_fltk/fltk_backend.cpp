#include "fltk_backend.hpp"

#include "core/image_loader.hpp"
#include "core/native_widget_meta.hpp"
#include "core/open_url.hpp"
#include "core/text_style_flags.hpp"
#include "core/ui_core_internal.hpp"

#include <FL/Fl_Graphics_Driver.H>

#include "fltk_lazy_vars.h"

#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Hor_Slider.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_RGB_Image.H>
#include <FL/Fl_Round_Button.H>
#include <FL/fl_draw.H>

#include <functional>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace n8v::detail {
namespace {

std::string escapeMenuText(std::string_view text) {
  std::string out;
  out.reserve(text.size());
  for (char c : text) {
    if (c == '/' || c == '\\') out.push_back('\\');
    out.push_back(c);
  }
  return out;
}

struct WidgetAction {
  bool isLink = false;
  std::function<void()> callback;
  std::string url;

  bool *checked = nullptr;
  std::function<void(bool)> onChange;

  int *radioSelected = nullptr;
  int radioValue = 0;
  std::function<void(int)> onRadioChange;

  int *dropdownSelected = nullptr;
  std::function<void(int)> onDropdownChange;

  float *sliderValue = nullptr;
  std::function<void(float)> onSliderChange;
};

class FltkBackend final : public Backend {
public:
  ~FltkBackend() override { shutdown(); }

  bool initialize(int width, int height, std::string_view title) override {
    titleStr_ = title;
    window_ = new Fl_Double_Window(width, height, titleStr_.c_str());
    window_->resizable(window_);
    window_->size_range(1, 1);
    window_->end();
    window_->callback(&FltkBackend::onCloseRequest, this);
    window_->show();
    return true;
  }

  bool pumpEvents() override {
    Fl::check();
    return !closeRequested_;
  }

  bool pointerDown() const override { return false; }

  Clay_Dimensions windowSize() const override { return {(float)window_->w(), (float)window_->h()}; }

  Clay_Dimensions measureText(std::string_view text, FontFamily, uint16_t fontSize, bool bold, bool italic) const override {
    fl_font(FL_HELVETICA + (bold ? 1 : 0) + (italic ? 2 : 0), fontSize);
    int w = 0, h = 0;
    std::string s(text);
    fl_measure(s.c_str(), w, h, 0);
    return {(float)w, (float)h};
  }

  Clay_Dimensions measureNativeChrome(NativeWidgetKind kind, std::string_view, uint16_t fontSize) const override {
    if (kind == NativeWidgetKind::Dropdown) {
      fl_font(FL_HELVETICA, fontSize);
      return {0, (float)fl_height() + 14.0f};
    }
    if (kind == NativeWidgetKind::Slider) {
      return {0, 20.0f};
    }
    return {0, 0};
  }

  void beginFrame() override {}

  void present(Clay_RenderCommandArray commands) override {
    std::map<int, int> wrapLineCounts;
    std::set<WidgetKey> seenKeys;
    Fl_Widget *pendingLabelTarget = nullptr;
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
        Fl_Widget *widget = ensureWidget(key, *meta);
        positionWidget(widget, command->boundingBox);
        pendingLabelTarget = widget;
        pendingKind = meta->kind;
        continue;
      }

      if (command->commandType == CLAY_RENDER_COMMAND_TYPE_IMAGE) {
        auto *meta = static_cast<NativeWidgetMeta *>(command->userData);
        if (!meta) continue;
        WidgetKey key{meta->ordinal, -1};
        seenKeys.insert(key);
        Fl_Widget *widget = ensureWidget(key, *meta);
        positionWidget(widget, command->boundingBox);
        ensureImageScaled(static_cast<Fl_Box *>(widget), *meta, (int)command->boundingBox.width, (int)command->boundingBox.height, command->renderData.image.cornerRadius);
        pendingLabelTarget = nullptr;
        continue;
      }

      if (command->commandType == CLAY_RENDER_COMMAND_TYPE_TEXT) {
        auto *flags = static_cast<TextStyleFlags *>(command->userData);
        std::string text(command->renderData.text.stringContents.chars, (size_t)command->renderData.text.stringContents.length);

        if (flags && flags->ownedByWidget) {
          if (pendingLabelTarget && pendingKind != NativeWidgetKind::Entry && pendingKind != NativeWidgetKind::Dropdown) {
            pendingLabelTarget->copy_label(text.c_str());
          }
          pendingLabelTarget = nullptr;
          continue;
        }

        int ordinal = flags ? flags->ordinal : -1;
        int wrapLineIndex = wrapLineCounts[ordinal]++;
        WidgetKey key{ordinal, wrapLineIndex};
        seenKeys.insert(key);
        Fl_Box *label = ensureLabel(key);
        label->copy_label(text.c_str());
        positionWidget(label, command->boundingBox);
        pendingLabelTarget = nullptr;
        continue;
      }

      pendingLabelTarget = nullptr;
    }

    for (auto it = widgets_.begin(); it != widgets_.end();) {
      if (!seenKeys.count(it->first)) {
        auto imageIt = imageStates_.find(it->second);
        if (imageIt != imageStates_.end()) {
          delete imageIt->second.image;
          imageStates_.erase(imageIt);
        }
        window_->remove(it->second);
        delete it->second;
        actions_.erase(it->first.ordinal);
        entryStates_.erase(it->first.ordinal);
        it = widgets_.erase(it);
      } else {
        ++it;
      }
    }

    window_->redraw();
  }

  void setCursor(CursorKind cursor) override {
    Fl_Cursor shape = FL_CURSOR_DEFAULT;
    if (cursor == CursorKind::Pointer) shape = FL_CURSOR_HAND;
    else if (cursor == CursorKind::Text) shape = FL_CURSOR_INSERT;
    window_->cursor(shape);
  }

  void shutdown() override {
    if (window_) {
      delete window_;
      window_ = nullptr;
    }
  }

private:
  static void onCloseRequest(Fl_Widget *, void *userData) { static_cast<FltkBackend *>(userData)->closeRequested_ = true; }

  static void onButtonClicked(Fl_Widget *, void *userData) {
    auto *action = static_cast<WidgetAction *>(userData);
    if (!action) return;
    if (action->isLink) {
      n8v::detail::openUrl(action->url);
    } else if (action->callback) {
      action->callback();
    }
  }

  static void onCheckboxChanged(Fl_Widget *widget, void *userData) {
    auto *action = static_cast<WidgetAction *>(userData);
    if (!action || !action->checked) return;
    bool newValue = static_cast<Fl_Button *>(widget)->value() != 0;
    if (newValue == *action->checked) return;
    *action->checked = newValue;
    if (action->onChange) action->onChange(newValue);
  }

  static void onRadioChanged(Fl_Widget *widget, void *userData) {
    auto *action = static_cast<WidgetAction *>(userData);
    if (!action || !action->radioSelected || static_cast<Fl_Button *>(widget)->value() == 0) return;
    if (*action->radioSelected == action->radioValue) return;
    *action->radioSelected = action->radioValue;
    if (action->onRadioChange) action->onRadioChange(action->radioValue);
  }

  static void onDropdownChanged(Fl_Widget *widget, void *userData) {
    auto *action = static_cast<WidgetAction *>(userData);
    if (!action || !action->dropdownSelected) return;
    int newValue = static_cast<Fl_Choice *>(widget)->value();
    if (newValue < 0 || newValue == *action->dropdownSelected) return;
    *action->dropdownSelected = newValue;
    if (action->onDropdownChange) action->onDropdownChange(newValue);
  }

  static void onSliderChanged(Fl_Widget *widget, void *userData) {
    auto *action = static_cast<WidgetAction *>(userData);
    if (!action || !action->sliderValue) return;
    float newValue = (float)static_cast<Fl_Slider *>(widget)->value();
    if (newValue == *action->sliderValue) return;
    *action->sliderValue = newValue;
    if (action->onSliderChange) action->onSliderChange(newValue);
  }

  struct EntryState {
    std::string *value = nullptr;
    std::function<void(std::string_view)> onChange;
    std::string lastSynced;
  };

  static void syncEntry(Fl_Input *widget, const NativeWidgetMeta &meta, EntryState &state) {
    if (!meta.entryValue) return;
    state.value = meta.entryValue;
    state.onChange = toStdFunction(meta.onEntryChange, meta.onEntryChangeUserdata);

    std::string widgetText = widget->value() ? widget->value() : "";
    if (widgetText != state.lastSynced) {
      *state.value = widgetText;
      state.lastSynced = widgetText;
      if (meta.entryBuf) ui_internal::writeToStringBuf(*state.value, *meta.entryBuf);
      if (state.onChange) state.onChange(widgetText);
    } else if (*state.value != state.lastSynced) {
      widget->value(state.value->c_str());
      state.lastSynced = *state.value;
    }
  }

  struct ImageState {
    Fl_RGB_Image *image = nullptr;
    const void *source = nullptr;
    int width = -1;
    int height = -1;
  };

  void ensureImageScaled(Fl_Box *box, const NativeWidgetMeta &meta, int targetW, int targetH, const Clay_CornerRadius &corner) {
    if (!meta.image) return;
    const n8v::detail::DecodedImage *image =
      n8v::detail::getOrBakeRoundedImage(meta.image, targetW, targetH, corner.topLeft, corner.topRight, corner.bottomLeft, corner.bottomRight);

    ImageState &state = imageStates_[box];
    if (state.source == image && state.width == targetW && state.height == targetH) return;

    if (state.source != image) {
      delete state.image;
      state.image = new Fl_RGB_Image(image->rgba, image->width, image->height, 4);
      state.source = image;
    }
    if (state.image && targetW > 0 && targetH > 0) state.image->scale(targetW, targetH, 0, 1);
    state.width = targetW;
    state.height = targetH;
    box->image(state.image);
  }

  struct WidgetKey {
    int ordinal;
    int subIndex; // -1 for a button/link. 0, 1, 2... per wrapped line of standalone text
    bool operator<(const WidgetKey &other) const { return ordinal != other.ordinal ? ordinal < other.ordinal : subIndex < other.subIndex; }
  };

  Fl_Widget *ensureWidget(const WidgetKey &key, const NativeWidgetMeta &meta) {
    auto it = widgets_.find(key);
    if (it != widgets_.end()) {
      WidgetAction &action = actions_[meta.ordinal];
      if (meta.kind == NativeWidgetKind::Button) {
        action.callback = toStdFunction(meta.onClick, meta.onClickUserdata);
      } else if (meta.kind == NativeWidgetKind::Link) {
        action.url = meta.url ? *meta.url : std::string();
      } else if (meta.kind == NativeWidgetKind::Checkbox && meta.checked) {
        action.checked = meta.checked;
        action.onChange = toStdFunction(meta.onChange, meta.onChangeUserdata);
        auto *button = static_cast<Fl_Button *>(it->second);
        if ((button->value() != 0) != *meta.checked) button->value(*meta.checked);
      } else if (meta.kind == NativeWidgetKind::Entry) {
        auto *input = static_cast<Fl_Input *>(it->second);
        input->input_type(meta.password ? FL_SECRET_INPUT : FL_NORMAL_INPUT);
        syncEntry(input, meta, entryStates_[meta.ordinal]);
      } else if (meta.kind == NativeWidgetKind::Radio && meta.radioSelected) {
        action.radioSelected = meta.radioSelected;
        action.radioValue = meta.radioValue;
        action.onRadioChange = toStdFunction(meta.onRadioChange, meta.onRadioChangeUserdata);
        bool shouldBeActive = *meta.radioSelected == meta.radioValue;
        auto *button = static_cast<Fl_Button *>(it->second);
        if ((button->value() != 0) != shouldBeActive) button->value(shouldBeActive);
      } else if (meta.kind == NativeWidgetKind::Dropdown && meta.dropdownSelected) {
        action.dropdownSelected = meta.dropdownSelected;
        action.onDropdownChange = toStdFunction(meta.onDropdownChange, meta.onDropdownChangeUserdata);
        if (*meta.dropdownSelected >= 0) {
          auto *choice = static_cast<Fl_Choice *>(it->second);
          if (choice->value() != *meta.dropdownSelected) choice->value(*meta.dropdownSelected);
        }
      } else if (meta.kind == NativeWidgetKind::Slider && meta.sliderValue) {
        action.sliderValue = meta.sliderValue;
        action.onSliderChange = toStdFunction(meta.onSliderChange, meta.onSliderChangeUserdata);
        auto *sliderWidget = static_cast<Fl_Slider *>(it->second);
        sliderWidget->bounds(meta.sliderMin, meta.sliderMax);
        if (sliderWidget->value() != *meta.sliderValue) sliderWidget->value(*meta.sliderValue);
      }
      return it->second;
    }

    Fl_Widget *widget = nullptr;
    WidgetAction &action = actions_[meta.ordinal];
    if (meta.kind == NativeWidgetKind::Button) {
      auto *button = new Fl_Button(0, 0, 1, 1);
      action.callback = toStdFunction(meta.onClick, meta.onClickUserdata);
      button->callback(&FltkBackend::onButtonClicked, &action);
      widget = button;
    } else if (meta.kind == NativeWidgetKind::Checkbox) {
      auto *button = new Fl_Check_Button(0, 0, 1, 1);
      action.checked = meta.checked;
      action.onChange = toStdFunction(meta.onChange, meta.onChangeUserdata);
      button->value(meta.checked && *meta.checked);
      button->when(FL_WHEN_CHANGED);
      button->callback(&FltkBackend::onCheckboxChanged, &action);
      widget = button;
    } else if (meta.kind == NativeWidgetKind::Entry) {
      auto *input = new Fl_Input(0, 0, 1, 1);
      input->input_type(meta.password ? FL_SECRET_INPUT : FL_NORMAL_INPUT);
      syncEntry(input, meta, entryStates_[meta.ordinal]);
      widget = input;
    } else if (meta.kind == NativeWidgetKind::Radio) {
      auto *button = new Fl_Round_Button(0, 0, 1, 1);
      action.radioSelected = meta.radioSelected;
      action.radioValue = meta.radioValue;
      action.onRadioChange = toStdFunction(meta.onRadioChange, meta.onRadioChangeUserdata);
      button->value(meta.radioSelected && *meta.radioSelected == meta.radioValue);
      button->when(FL_WHEN_CHANGED);
      button->callback(&FltkBackend::onRadioChanged, &action);
      widget = button;
    } else if (meta.kind == NativeWidgetKind::Dropdown) {
      auto *choice = new Fl_Choice(0, 0, 1, 1);
      if (meta.dropdownItems) {
        for (const std::string &item : *meta.dropdownItems) choice->add(escapeMenuText(item).c_str());
      }
      action.dropdownSelected = meta.dropdownSelected;
      action.onDropdownChange = toStdFunction(meta.onDropdownChange, meta.onDropdownChangeUserdata);
      if (meta.dropdownSelected && *meta.dropdownSelected >= 0) choice->value(*meta.dropdownSelected);
      choice->callback(&FltkBackend::onDropdownChanged, &action);
      widget = choice;
    } else if (meta.kind == NativeWidgetKind::Slider) {
      auto *sliderWidget = new Fl_Hor_Slider(0, 0, 1, 1);
      sliderWidget->bounds(meta.sliderMin, meta.sliderMax);
      action.sliderValue = meta.sliderValue;
      action.onSliderChange = toStdFunction(meta.onSliderChange, meta.onSliderChangeUserdata);
      if (meta.sliderValue) sliderWidget->value(*meta.sliderValue);
      sliderWidget->when(FL_WHEN_CHANGED);
      sliderWidget->callback(&FltkBackend::onSliderChanged, &action);
      widget = sliderWidget;
    } else if (meta.kind == NativeWidgetKind::Image) {
      widget = new Fl_Box(0, 0, 1, 1);
    } else {
      auto *button = new Fl_Button(0, 0, 1, 1);
      action.isLink = true;
      action.url = meta.url ? *meta.url : std::string();
      button->box(FL_NO_BOX);
      button->labelcolor(fl_rgb_color(66, 135, 245));
      button->align(FL_ALIGN_INSIDE | FL_ALIGN_LEFT);
      button->callback(&FltkBackend::onButtonClicked, &action);
      widget = button;
    }

    window_->add(widget);
    widgets_[key] = widget;
    return widget;
  }

  Fl_Box *ensureLabel(const WidgetKey &key) {
    auto it = widgets_.find(key);
    if (it != widgets_.end()) return static_cast<Fl_Box *>(it->second);

    auto *label = new Fl_Box(0, 0, 1, 1);
    label->align(FL_ALIGN_INSIDE | FL_ALIGN_LEFT | FL_ALIGN_TOP);
    window_->add(label);
    widgets_[key] = label;
    return label;
  }

  void positionWidget(Fl_Widget *widget, const Clay_BoundingBox &box) { widget->resize((int)box.x, (int)box.y, (int)box.width, (int)box.height); }

  std::string titleStr_;
  Fl_Double_Window *window_ = nullptr;
  bool closeRequested_ = false;

  std::map<WidgetKey, Fl_Widget *> widgets_;
  std::unordered_map<int, WidgetAction> actions_;
  std::unordered_map<int, EntryState> entryStates_;
  std::unordered_map<Fl_Widget *, ImageState> imageStates_;
};

} // namespace

std::unique_ptr<Backend> makeFltkBackend() { return std::make_unique<FltkBackend>(); }

} // namespace n8v::detail
