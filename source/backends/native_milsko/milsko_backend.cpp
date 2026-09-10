#include "milsko_backend.hpp"

#include "core/native_widget_meta.hpp"
#include "core/open_url.hpp"
#include "core/text_style_flags.hpp"

#include <Mw/Milsko.h>

#include "milsko_lazy_vars.h"

#include <cmath>
#include <cstdlib>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>

namespace n8v::detail {
namespace {

struct ClickAction {
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
  float sliderMin = 0.0f;
  float sliderMax = 1.0f;
  std::function<void(float)> onSliderChange;
};

constexpr int sliderSteps = 10000;

int sliderPositionFor(float value, float min, float max) {
  float range = max - min;
  if (range <= 0.0f) return 0;
  float clamped = value < min ? min : value > max ? max : value;
  return (int)((clamped - min) / range * sliderSteps + 0.5f);
}

class MilskoBackend final : public Backend {
public:
  ~MilskoBackend() override { shutdown(); }

  bool initialize(int width, int height, std::string_view title) override {
    MwLibraryInit();

    std::string titleStr(title);
    window_ = MwVaCreateWidget(MwWindowClass, "n8v", nullptr, MwDEFAULT, MwDEFAULT, (unsigned int)width, (unsigned int)height, MwNtitle, titleStr.c_str(), NULL);
    if (!window_) return false;

    const char *style = std::getenv("N8V_STYLE");
    if (style && strcmp(style, "classic") == 0) {
      MwVaApply(window_, MwNmodernLook, 0, NULL);
    }

    measureLabel_ = MwCreateWidget(MwLabelClass, "n8v-measure", window_, 0, 0, 1, 1);
    MwShow(measureLabel_, 0);

    return true;
  }

  bool pumpEvents() override {
    while (MwPending(window_)) {
      if (MwStep(window_) != 0) return false;
    }
    return !MwWindowShouldClose(window_);
  }

  bool pointerDown() const override { return false; }

  Clay_Dimensions windowSize() const override { return {(float)MwGetInteger(window_, MwNwidth), (float)MwGetInteger(window_, MwNheight)}; }

  Clay_Dimensions measureNativeChrome(NativeWidgetKind kind, std::string_view, uint16_t) const override {
    if (kind == NativeWidgetKind::Dropdown) {
      return {0, (float)MwTextHeight(measureLabel_, nullptr, "Xg") + 14.0f};
    }
    if (kind == NativeWidgetKind::Slider) {
      return {0, 20.0f};
    }
    return {0, 0};
  }

  Clay_Dimensions measureText(std::string_view text, FontFamily, uint16_t, bool, bool) const override {
    std::string s(text);

    bool allWhitespace = !s.empty();
    for (char c : s) {
      if (c != ' ' && c != '\t' && c != '\n') {
        allWhitespace = false;
        break;
      }
    }

    int width;
    if (allWhitespace) {
      width = MwTextWidth(measureLabel_, nullptr, (s + "x").c_str()) - MwTextWidth(measureLabel_, nullptr, "x");
    } else {
      width = MwTextWidth(measureLabel_, nullptr, s.c_str());
    }

    return {(float)width, (float)MwTextHeight(measureLabel_, nullptr, s.c_str())};
  }

  void beginFrame() override {}

  void present(Clay_RenderCommandArray commands) override {
    std::map<int, int> wrapLineCounts;
    std::set<WidgetKey> seenKeys;
    MwWidget pendingLabelTarget = nullptr;
    NativeWidgetKind pendingKind = NativeWidgetKind::Button;
    MwWidget pendingCheckboxWidget = nullptr;
    int pendingCheckboxOrdinal = -1;

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
        MwWidget widget = ensureWidget(key, *meta);

        if (meta->kind == NativeWidgetKind::Checkbox || meta->kind == NativeWidgetKind::Radio) {
          pendingCheckboxWidget = widget;
          pendingCheckboxOrdinal = meta->ordinal;
        } else {
          positionWidget(widget, command->boundingBox);
          pendingLabelTarget = widget;
          pendingKind = meta->kind;
        }
        continue;
      }

      if (command->commandType == CLAY_RENDER_COMMAND_TYPE_TEXT) {
        auto *flags = static_cast<TextStyleFlags *>(command->userData);
        std::string text(command->renderData.text.stringContents.chars, (size_t)command->renderData.text.stringContents.length);

        if (flags && flags->ownedByWidget && pendingCheckboxWidget) {
          const Clay_BoundingBox &box = command->boundingBox;
          float squareSize = box.height;
          float gap = squareSize * 0.4f;
          MwVaApply(
            pendingCheckboxWidget,
            MwNx,
            (int)std::floor(box.x - squareSize - gap),
            MwNy,
            (int)std::floor(box.y),
            MwNwidth,
            (int)std::ceil(squareSize),
            MwNheight,
            (int)std::ceil(squareSize),
            NULL
          );

          WidgetKey labelKey{pendingCheckboxOrdinal, -2};
          seenKeys.insert(labelKey);
          MwWidget label = ensureLabel(labelKey);
          MwSetText(label, MwNtext, text.c_str());
          positionWidget(label, box);

          pendingCheckboxWidget = nullptr;
          pendingCheckboxOrdinal = -1;
          continue;
        }

        if (flags && flags->ownedByWidget) {
          if (pendingLabelTarget && pendingKind != NativeWidgetKind::Entry && pendingKind != NativeWidgetKind::Dropdown) MwSetText(pendingLabelTarget, MwNtext, text.c_str());
          pendingLabelTarget = nullptr;
          continue;
        }

        int ordinal = flags ? flags->ordinal : -1;
        int wrapLineIndex = wrapLineCounts[ordinal]++;
        WidgetKey key{ordinal, wrapLineIndex};
        seenKeys.insert(key);
        MwWidget label = ensureLabel(key);
        MwSetText(label, MwNtext, text.c_str());
        positionWidget(label, command->boundingBox);
        pendingLabelTarget = nullptr;
        continue;
      }

      pendingLabelTarget = nullptr;
    }

    for (auto it = widgets_.begin(); it != widgets_.end();) {
      if (!seenKeys.count(it->first)) {
        MwDestroyWidget(it->second);
        actions_.erase(it->first.ordinal);
        entryStates_.erase(it->first.ordinal);
        it = widgets_.erase(it);
      } else {
        ++it;
      }
    }
  }

  void setCursor(CursorKind) override {}

  void shutdown() override {
    if (window_) {
      MwDestroyWidget(window_);
      window_ = nullptr;
      measureLabel_ = nullptr;
    }
  }

private:
  static void MWAPI onActivate(MwWidget /*handle*/, void *userData, void * /*callData*/) {
    auto *action = static_cast<ClickAction *>(userData);
    if (!action) return;
    if (action->isLink) {
      n8v::detail::openUrl(action->url);
    } else if (action->callback) {
      action->callback();
    }
  }

  static void MWAPI onCheckboxChanged(MwWidget handle, void *userData, void * /*callData*/) {
    auto *action = static_cast<ClickAction *>(userData);
    if (!action || !action->checked) return;
    bool newValue = MwGetInteger(handle, MwNchecked) != 0;
    *action->checked = newValue;
    if (action->onChange) action->onChange(newValue);
  }

  static void MWAPI onRadioChanged(MwWidget handle, void *userData, void * /*callData*/) {
    auto *action = static_cast<ClickAction *>(userData);
    if (!action || !action->radioSelected || MwGetInteger(handle, MwNchecked) == 0) return;
    if (*action->radioSelected == action->radioValue) return;
    *action->radioSelected = action->radioValue;
    if (action->onRadioChange) action->onRadioChange(action->radioValue);
  }

  static void MWAPI onDropdownChanged(MwWidget handle, void *userData, void * /*callData*/) {
    auto *action = static_cast<ClickAction *>(userData);
    if (!action || !action->dropdownSelected) return;
    int newValue = MwGetInteger(handle, MwNvalue);
    if (newValue == *action->dropdownSelected) return;
    *action->dropdownSelected = newValue;
    if (action->onDropdownChange) action->onDropdownChange(newValue);
  }

  static void MWAPI onSliderChanged(MwWidget handle, void *userData, void * /*callData*/) {
    auto *action = static_cast<ClickAction *>(userData);
    if (!action || !action->sliderValue) return;
    int position = MwGetInteger(handle, MwNvalue);
    float range = action->sliderMax - action->sliderMin;
    float newValue = range > 0.0f ? action->sliderMin + (position / (float)sliderSteps) * range : action->sliderMin;
    if (newValue == *action->sliderValue) return;
    *action->sliderValue = newValue;
    if (action->onSliderChange) action->onSliderChange(newValue);
  }

  struct EntryState {
    std::string *value = nullptr;
    std::function<void(std::string_view)> onChange;
    std::string lastSynced;
  };

  static void syncEntry(MwWidget widget, const NativeWidgetMeta &meta, EntryState &state) {
    if (!meta.entryValue) return;
    state.value = meta.entryValue;
    state.onChange = toStdFunction(meta.onEntryChange, meta.onEntryChangeUserdata);

    const char *raw = MwGetText(widget, MwNtext);
    std::string widgetText = raw ? raw : "";
    if (widgetText != state.lastSynced) {
      *state.value = widgetText;
      state.lastSynced = widgetText;
      if (state.onChange) state.onChange(widgetText);
    } else if (*state.value != state.lastSynced) {
      MwSetText(widget, MwNtext, state.value->c_str());
      state.lastSynced = *state.value;
    }
  }

  struct WidgetKey {
    int ordinal;
    int subIndex; // -1 for a button/link. 0, 1, 2... per wrapped line of standalone text
    bool operator<(const WidgetKey &other) const { return ordinal != other.ordinal ? ordinal < other.ordinal : subIndex < other.subIndex; }
  };

  MwWidget ensureWidget(const WidgetKey &key, const NativeWidgetMeta &meta) {
    auto it = widgets_.find(key);
    if (it != widgets_.end()) {
      ClickAction &action = actions_[meta.ordinal];
      if (meta.kind == NativeWidgetKind::Button) {
        action.callback = toStdFunction(meta.onClick, meta.onClickUserdata);
      } else if (meta.kind == NativeWidgetKind::Checkbox && meta.checked) {
        action.checked = meta.checked;
        action.onChange = toStdFunction(meta.onChange, meta.onChangeUserdata);
        MwSetInteger(it->second, MwNchecked, *meta.checked ? 1 : 0);
      } else if (meta.kind == NativeWidgetKind::Entry) {
        MwSetInteger(it->second, MwNhideInput, meta.password ? 1 : 0);
        syncEntry(it->second, meta, entryStates_[meta.ordinal]);
      } else if (meta.kind == NativeWidgetKind::Radio && meta.radioSelected) {
        action.radioSelected = meta.radioSelected;
        action.radioValue = meta.radioValue;
        action.onRadioChange = toStdFunction(meta.onRadioChange, meta.onRadioChangeUserdata);
        MwSetInteger(it->second, MwNchecked, *meta.radioSelected == meta.radioValue ? 1 : 0);
      } else if (meta.kind == NativeWidgetKind::Dropdown && meta.dropdownSelected) {
        action.dropdownSelected = meta.dropdownSelected;
        action.onDropdownChange = toStdFunction(meta.onDropdownChange, meta.onDropdownChangeUserdata);
        if (*meta.dropdownSelected >= 0) MwSetInteger(it->second, MwNvalue, *meta.dropdownSelected);
      } else if (meta.kind == NativeWidgetKind::Slider && meta.sliderValue) {
        action.sliderValue = meta.sliderValue;
        action.sliderMin = meta.sliderMin;
        action.sliderMax = meta.sliderMax;
        action.onSliderChange = toStdFunction(meta.onSliderChange, meta.onSliderChangeUserdata);
        MwSetInteger(it->second, MwNvalue, sliderPositionFor(*meta.sliderValue, meta.sliderMin, meta.sliderMax));
      } else {
        action.url = meta.url ? *meta.url : std::string();
      }
      return it->second;
    }

    ClickAction &action = actions_[meta.ordinal];
    action.isLink = meta.kind == NativeWidgetKind::Link;

    MwWidget widget = nullptr;
    if (meta.kind == NativeWidgetKind::Checkbox) {
      action.checked = meta.checked;
      action.onChange = toStdFunction(meta.onChange, meta.onChangeUserdata);
      widget = MwCreateWidget(MwCheckBoxClass, "n8v-checkbox", window_, 0, 0, 1, 1);
      MwSetInteger(widget, MwNchecked, meta.checked && *meta.checked ? 1 : 0);
      MwAddUserHandler(widget, MwNchangedHandler, onCheckboxChanged, &action);
    } else if (meta.kind == NativeWidgetKind::Entry) {
      widget = MwCreateWidget(MwEntryClass, "n8v-entry", window_, 0, 0, 1, 1);
      MwSetInteger(widget, MwNhideInput, meta.password ? 1 : 0);
      syncEntry(widget, meta, entryStates_[meta.ordinal]);
    } else if (meta.kind == NativeWidgetKind::Radio) {
      action.radioSelected = meta.radioSelected;
      action.radioValue = meta.radioValue;
      action.onRadioChange = toStdFunction(meta.onRadioChange, meta.onRadioChangeUserdata);
      widget = MwCreateWidget(MwCheckBoxClass, "n8v-radio", window_, 0, 0, 1, 1);
      MwSetInteger(widget, MwNchecked, meta.radioSelected && *meta.radioSelected == meta.radioValue ? 1 : 0);
      MwAddUserHandler(widget, MwNchangedHandler, onRadioChanged, &action);
    } else if (meta.kind == NativeWidgetKind::Dropdown) {
      action.dropdownSelected = meta.dropdownSelected;
      action.onDropdownChange = toStdFunction(meta.onDropdownChange, meta.onDropdownChangeUserdata);
      widget = MwCreateWidget(MwComboBoxClass, "n8v-dropdown", window_, 0, 0, 1, 1);
      if (meta.dropdownItems) {
        for (const std::string &item : *meta.dropdownItems) MwComboBoxAdd(widget, -1, item.c_str());
      }
      if (meta.dropdownSelected && *meta.dropdownSelected >= 0) MwSetInteger(widget, MwNvalue, *meta.dropdownSelected);
      MwAddUserHandler(widget, MwNcomboBoxChangedHandler, onDropdownChanged, &action);
    } else if (meta.kind == NativeWidgetKind::Slider) {
      action.sliderValue = meta.sliderValue;
      action.sliderMin = meta.sliderMin;
      action.sliderMax = meta.sliderMax;
      action.onSliderChange = toStdFunction(meta.onSliderChange, meta.onSliderChangeUserdata);
      widget = MwCreateWidget(MwScrollBarClass, "n8v-slider", window_, 0, 0, 1, 1);
      MwVaApply(widget, MwNorientation, MwHORIZONTAL, MwNminValue, 0, MwNmaxValue, sliderSteps, MwNareaShown, sliderSteps / 30, NULL);
      if (meta.sliderValue) MwSetInteger(widget, MwNvalue, sliderPositionFor(*meta.sliderValue, meta.sliderMin, meta.sliderMax));
      MwAddUserHandler(widget, MwNchangedHandler, onSliderChanged, &action);
    } else {
      if (action.isLink) {
        action.url = meta.url ? *meta.url : std::string();
      } else {
        action.callback = toStdFunction(meta.onClick, meta.onClickUserdata);
      }

      widget = MwCreateWidget(MwButtonClass, "n8v-widget", window_, 0, 0, 1, 1);
      if (action.isLink) {
        MwSetInteger(widget, MwNflat, 1);
        MwSetText(widget, MwNforeground, "#4287f5");
      }
      MwAddUserHandler(widget, MwNactivateHandler, onActivate, &action);
    }

    widgets_[key] = widget;
    return widget;
  }

  MwWidget ensureLabel(const WidgetKey &key) {
    auto it = widgets_.find(key);
    if (it != widgets_.end()) return it->second;

    MwWidget label = MwCreateWidget(MwLabelClass, "n8v-label", window_, 0, 0, 1, 1);
    widgets_[key] = label;
    return label;
  }

  void positionWidget(MwWidget widget, const Clay_BoundingBox &box) {
    MwVaApply(widget, MwNx, (int)std::floor(box.x), MwNy, (int)std::floor(box.y), MwNwidth, (int)std::ceil(box.width), MwNheight, (int)std::ceil(box.height), NULL);
  }

  MwWidget window_ = nullptr;
  MwWidget measureLabel_ = nullptr;

  std::map<WidgetKey, MwWidget> widgets_;
  std::unordered_map<int, ClickAction> actions_;
  std::unordered_map<int, EntryState> entryStates_;
};

} // namespace

std::unique_ptr<Backend> makeMilskoBackend() { return std::make_unique<MilskoBackend>(); }

} // namespace n8v::detail
