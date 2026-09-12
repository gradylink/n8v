#pragma once

#include "html_backend.hpp"

#include "core/native_widget_meta.hpp"
#include "core/text_style_flags.hpp"

#include <n8v/n8v_c.h>

#include <clay.h>
#include <emscripten/val.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

extern "C" void n8vHtmlInstallListeners();

namespace n8v::detail {

std::string htmlFontFamilyName(FontFamily family);

class HtmlBackend;

void n8vHtmlSetInstance(HtmlBackend *backend);
HtmlBackend *n8vHtmlInstance();

std::string cssColor(const Clay_Color &color);
std::string cssColor(const n8v::Color &color);
void setCornerRadii(emscripten::val &el, const Clay_CornerRadius &radius);

class HtmlBackend final : public Backend {
public:
  ~HtmlBackend() override { shutdown(); }

  bool initialize(int width, int height, std::string_view title) override;
  bool pumpEvents() override;
  bool pointerDown() const override { return pointerDown_; }
  bool isEntryFocused(int ordinal) const override;
  Clay_Dimensions windowSize() const override;
  Clay_Dimensions measureText(std::string_view text, FontFamily family, uint16_t fontSize, bool bold, bool italic) const override;
  void beginFrame() override;
  void present(Clay_RenderCommandArray commands) override;
  void setCursor(CursorKind cursor) override;
  void shutdown() override;

  void onPointerMove(float x, float y);
  void onPointerDown(bool down);
  void onEntryInput(int ordinal, std::string_view value);

private:
  struct EntryBinding {
    std::string *entryValue = nullptr;
    n8v_string_buf *entryBuf = nullptr;
    n8v_text_change_fn onChange = nullptr;
    void *onChangeUserdata = nullptr;
  };

  using ElementKey = uint64_t;
  static ElementKey elementKey(uint32_t id, Clay_RenderCommandType type) { return (static_cast<uint64_t>(id) << 8) | static_cast<uint64_t>(type); }

  emscripten::val getOrCreateElement(uint32_t id, Clay_RenderCommandType type, const char *tag, bool &created);
  void reorderElement(emscripten::val &el);
  void positionElement(emscripten::val &el, Clay_BoundingBox &lastBox, const Clay_BoundingBox &box);
  void removeUntouchedElements();

  struct PendingState {
    NativeWidgetMeta *indicatorMeta = nullptr;
    bool isCheckbox = false;
    bool isRadio = false;
    NativeWidgetMeta *entryMeta = nullptr;
    Clay_BoundingBox entryFieldBox{};
    NativeWidgetMeta *linkMeta = nullptr;

    void clear() {
      indicatorMeta = nullptr;
      isCheckbox = false;
      isRadio = false;
      entryMeta = nullptr;
      linkMeta = nullptr;
    }
  };

  void renderRectangle(const Clay_RenderCommand &command, PendingState &pending);
  void renderBorder(const Clay_RenderCommand &command);
  void renderText(const Clay_RenderCommand &command, PendingState &pending);
  void renderImage(const Clay_RenderCommand &command);

  void renderCheckboxOrRadioIndicator(NativeWidgetMeta &meta, const Clay_BoundingBox &labelBox, bool isRadio);
  void renderRadioIndicator(NativeWidgetMeta &meta, const Clay_BoundingBox &squareBox);
  void renderCheckboxIndicator(NativeWidgetMeta &meta, const Clay_BoundingBox &squareBox);
  void renderDropdownChevron(const Clay_RenderCommand &command);
  void removeUntouchedIndicators();
  void syncEntryInput(NativeWidgetMeta &meta, const Clay_BoundingBox &fieldBox, const Clay_RenderCommand &textCommand);
  void removeUntouchedEntryInputs();
  void renderLinkText(NativeWidgetMeta &meta, const Clay_RenderCommand &command);
  void injectFontFaces();

  struct RectSignature {
    Clay_Color background{};
    Clay_CornerRadius radius{};
  };
  struct TextSignature {
    Clay_Color color{};
    FontFamily family = FontFamily::DejaVuSans;
    uint16_t fontSize = 0;
    bool bold = false, italic = false, underline = false;
    std::string text;
  };
  struct BorderSignature {
    float width = -1.0f;
    Clay_Color color{};
    Clay_CornerRadius radius{};
  };
  struct IndicatorSignature {
    n8v::Color fill{}, border{}, glyph{};
    float borderWidth = -1.0f, cornerRadius = -1.0f, glyphScale = -1.0f;
  };
  struct ChevronSignature {
    bool pointsUp = false;
    n8v::Color color{};
    bool set = false;
  };
  struct EntryStyleSignature {
    bool password = false;
    FontFamily family = FontFamily::DejaVuSans;
    uint16_t fontSize = 0;
    Clay_Color color{};
    float padLeft = -1.0f, padTop = -1.0f;
    std::string placeholderAttr;
    bool hasPlaceholderAttr = false;
  };

  static bool colorEquals(const Clay_Color &a, const Clay_Color &b) { return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a; }
  static bool colorEquals(const n8v::Color &a, const n8v::Color &b) { return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a; }
  static bool radiusEquals(const Clay_CornerRadius &a, const Clay_CornerRadius &b) {
    return a.topLeft == b.topLeft && a.topRight == b.topRight && a.bottomLeft == b.bottomLeft && a.bottomRight == b.bottomRight;
  }

  emscripten::val doc_;
  emscripten::val root_;
  emscripten::val lastAppendedSibling_ = emscripten::val::null();
  mutable emscripten::val measureCtx_;
  std::unordered_map<ElementKey, emscripten::val> elementCache_;
  std::unordered_map<ElementKey, Clay_BoundingBox> elementLastBox_;
  std::unordered_map<ElementKey, RectSignature> elementRectSig_;
  std::unordered_map<ElementKey, TextSignature> elementTextSig_;
  std::unordered_map<ElementKey, BorderSignature> elementBorderSig_;
  std::unordered_map<ElementKey, ChevronSignature> elementChevronSig_;
  std::unordered_map<ElementKey, bool> touchedThisFrame_;
  std::unordered_map<int, emscripten::val> entryElements_;
  std::unordered_map<int, Clay_BoundingBox> entryLastBox_;
  std::unordered_map<int, EntryStyleSignature> entryStyleSig_;
  std::unordered_map<int, bool> touchedEntryThisFrame_;
  std::unordered_map<int, EntryBinding> entryBindings_;
  std::unordered_map<int, emscripten::val> indicatorElements_;
  std::unordered_map<int, Clay_BoundingBox> indicatorLastBox_;
  std::unordered_map<int, IndicatorSignature> indicatorSig_;
  std::unordered_map<int, bool> touchedIndicatorThisFrame_;
  std::unordered_map<const void *, std::string> imageDataUris_;
  std::unordered_map<ElementKey, const void *> elementImageSource_;

  float pointerX_ = 0.0f, pointerY_ = 0.0f;
  bool pointerDown_ = false;
  CursorKind currentCursorKind_ = CursorKind::Default;
  int width_ = 0, height_ = 0;
  bool initialized_ = false;
};

} // namespace n8v::detail
