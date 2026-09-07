#include "milsko_backend.hpp"

#include "core/native_widget_meta.hpp"
#include "core/open_url.hpp"
#include "core/text_style_flags.hpp"

#include <Mw/Milsko.h>

#include "milsko_lazy_vars.h"

#include <cmath>
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
};

class MilskoBackend final : public Backend {
public:
  ~MilskoBackend() override { shutdown(); }

  bool initialize(int width, int height, std::string_view title) override {
    MwLibraryInit();

    std::string titleStr(title);
    window_ = MwVaCreateWidget(MwWindowClass, "n8v", nullptr, MwDEFAULT, MwDEFAULT, (unsigned int)width, (unsigned int)height, MwNtitle, titleStr.c_str(), NULL);
    if (!window_) return false;

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
        positionWidget(widget, command->boundingBox);
        pendingLabelTarget = widget;
        continue;
      }

      if (command->commandType == CLAY_RENDER_COMMAND_TYPE_TEXT) {
        auto *flags = static_cast<TextStyleFlags *>(command->userData);
        std::string text(command->renderData.text.stringContents.chars, (size_t)command->renderData.text.stringContents.length);

        if (flags && flags->ownedByWidget) {
          if (pendingLabelTarget) MwSetText(pendingLabelTarget, MwNtext, text.c_str());
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
        action.callback = meta.onClick ? *meta.onClick : std::function<void()>{};
      } else {
        action.url = meta.url ? *meta.url : std::string();
      }
      return it->second;
    }

    ClickAction &action = actions_[meta.ordinal];
    action.isLink = meta.kind == NativeWidgetKind::Link;
    if (action.isLink) {
      action.url = meta.url ? *meta.url : std::string();
    } else {
      action.callback = meta.onClick ? *meta.onClick : std::function<void()>{};
    }

    MwWidget widget = MwCreateWidget(MwButtonClass, "n8v-widget", window_, 0, 0, 1, 1);
    if (action.isLink) {
      MwSetInteger(widget, MwNflat, 1);
      MwSetText(widget, MwNforeground, "#4287f5");
    }
    MwAddUserHandler(widget, MwNactivateHandler, onActivate, &action);

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
};

} // namespace

std::unique_ptr<Backend> makeMilskoBackend() { return std::make_unique<MilskoBackend>(); }

} // namespace n8v::detail
