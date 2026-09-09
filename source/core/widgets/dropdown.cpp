#include <n8v/backend.hpp>
#include <n8v/style.hpp>
#include <n8v/ui.hpp>

#include "core/clay_convert.hpp"
#include "core/native_widget_meta.hpp"
#include "core/text_style_flags.hpp"
#include "core/ui_core_internal.hpp"

#include <clay.h>

#include <deque>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace {

using namespace n8v::detail::ui_internal;

struct DropdownItemClick {
  n8v::detail::NativeWidgetMeta *meta;
  int index;
};

std::deque<std::vector<std::string>> dropdownItemsStorage;
std::deque<std::function<void(int)>> dropdownChangeCallbacks;
std::deque<int> dropdownOrdinalStorage;
std::deque<DropdownItemClick> dropdownItemClickStorage;
std::unordered_map<int, bool> dropdownOpenState;

void dispatchDropdownToggle(Clay_ElementId /*elementId*/, Clay_PointerData pointerData, void *userData) {
  if (pointerData.state != CLAY_POINTER_DATA_PRESSED_THIS_FRAME) return;
  auto *meta = static_cast<n8v::detail::NativeWidgetMeta *>(userData);
  if (!meta) return;
  bool &open = dropdownOpenState[meta->ordinal];
  open = !open;
}

void dispatchDropdownClose(Clay_ElementId /*elementId*/, Clay_PointerData pointerData, void *userData) {
  if (pointerData.state != CLAY_POINTER_DATA_PRESSED_THIS_FRAME) return;
  auto *ordinal = static_cast<int *>(userData);
  if (!ordinal) return;
  dropdownOpenState[*ordinal] = false;
}

void dispatchDropdownSelect(Clay_ElementId /*elementId*/, Clay_PointerData pointerData, void *userData) {
  if (pointerData.state != CLAY_POINTER_DATA_PRESSED_THIS_FRAME) return;
  auto *click = static_cast<DropdownItemClick *>(userData);
  if (!click || !click->meta) return;
  if (click->meta->dropdownSelected) *click->meta->dropdownSelected = click->index;
  if (click->meta->onDropdownChange && *click->meta->onDropdownChange) (*click->meta->onDropdownChange)(click->index);
  dropdownOpenState[click->meta->ordinal] = false;
}

} // namespace

namespace n8v::detail::ui_internal {

void resetDropdownFrameState() {
  dropdownItemsStorage.clear();
  dropdownChangeCallbacks.clear();
  dropdownOrdinalStorage.clear();
  dropdownItemClickStorage.clear();
}

} // namespace n8v::detail::ui_internal

namespace n8v::detail {

void dropdown(const DropdownOptions &options) {
  Clay__OpenElement();

  const int ordinal = widgetOrdinal++;
  const bool hovered = Clay_Hovered();
  // if (hovered) pendingCursor = CursorKind::Pointer;
  const bool pressed = hovered && activeBackend().pointerDown();
  const bool open = dropdownOpenState[ordinal];
  const bool hasSelection = options.selected && *options.selected >= 0 && (size_t)*options.selected < options.items.size();
  const DropdownPaint paint = activePaint().dropdown(open, hasSelection, hovered, pressed);

  const bool floatingLabelStyle = paint.labelColor.a > 0.0f;
  const bool labelFloated = floatingLabelStyle && (hasSelection || open);
  std::string_view selectedText = hasSelection ? options.items[(size_t)*options.selected] : std::string_view{};

  Clay_ElementDeclaration decl = {};
  decl.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
  decl.layout.sizing.width = CLAY_SIZING_GROW(0);
  decl.backgroundColor = toClay(paint.background);
  decl.cornerRadius = {paint.cornerRadius.topLeft, paint.cornerRadius.topRight, paint.cornerRadius.bottomLeft, paint.cornerRadius.bottomRight};
  if (!floatingLabelStyle) decl.layout.padding = toClay(paint.padding);

  Clay_Dimensions nativeSize = activeBackend().measureNativeChrome(NativeWidgetKind::Dropdown, hasSelection ? selectedText : options.placeholder, paint.fontSize);
  const bool hasNativeChrome = nativeSize.height > 0;
  float contentHeight = (float)paint.padding.top + (float)paint.labelFontSize * 1.2f + (float)paint.fontSize * 1.2f + (float)paint.padding.bottom;
  if (hasNativeChrome) {
    decl.layout.sizing.height = CLAY_SIZING_FIXED(nativeSize.height);
  } else if (floatingLabelStyle) {
    decl.layout.sizing.height = CLAY_SIZING_FIXED(contentHeight + paint.indicatorWidth);
  }

  dropdownItemsStorage.push_back({});
  std::vector<std::string> &itemsCopy = dropdownItemsStorage.back();
  itemsCopy.reserve(options.items.size());
  for (std::string_view item : options.items) itemsCopy.emplace_back(item);

  const bool hasOnChange = static_cast<bool>(options.onChange);
  if (hasOnChange) dropdownChangeCallbacks.push_back(options.onChange);

  widgetMetaStorage.push_back(NativeWidgetMeta{});
  NativeWidgetMeta &meta = widgetMetaStorage.back();
  meta.kind = NativeWidgetKind::Dropdown;
  meta.ordinal = ordinal;
  meta.dropdownItems = &itemsCopy;
  meta.dropdownSelected = options.selected;
  meta.onDropdownChange = hasOnChange ? &dropdownChangeCallbacks.back() : nullptr;
  decl.userData = &meta;

  Clay__ConfigureOpenElement(decl);

  if (!hasNativeChrome) {
    Clay_OnHover(dispatchDropdownToggle, &meta);
  }

  if (!hasNativeChrome && floatingLabelStyle) {
    Clay__OpenElement();
    Clay_ElementDeclaration contentRowDecl = {};
    contentRowDecl.layout.padding = toClay(paint.padding);
    contentRowDecl.layout.sizing.width = CLAY_SIZING_GROW(0);
    contentRowDecl.layout.sizing.height = CLAY_SIZING_GROW(0);
    contentRowDecl.layout.childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_BOTTOM};
    Clay__ConfigureOpenElement(contentRowDecl);

    std::string_view lineText = hasSelection ? selectedText : std::string_view{"\xC2\xA0"};
    textStyleStorage.push_back(n8v::detail::TextStyleFlags{paint.font, false, false, false, true, ordinal});
    Clay_TextElementConfig valueTextConfig = {};
    valueTextConfig.textColor = toClay(paint.textColor);
    valueTextConfig.fontSize = paint.fontSize;
    valueTextConfig.wrapMode = CLAY_TEXT_WRAP_NONE;
    valueTextConfig.userData = &textStyleStorage.back();
    CLAY_TEXT(internString(lineText), valueTextConfig);

    Clay__OpenElement();
    Clay_ElementDeclaration chevronDecl = {};
    chevronDecl.layout.sizing.width = CLAY_SIZING_FIXED(12.0f);
    chevronDecl.layout.sizing.height = CLAY_SIZING_FIXED(12.0f);
    chevronDecl.backgroundColor = {0, 0, 0, 1};
    widgetMetaStorage.push_back(NativeWidgetMeta{});
    NativeWidgetMeta &chevronMeta = widgetMetaStorage.back();
    chevronMeta.kind = NativeWidgetKind::DropdownChevron;
    chevronMeta.ordinal = ordinal;
    chevronMeta.chevronColor = paint.labelColor;
    chevronMeta.chevronPointsUp = open;
    chevronDecl.userData = &chevronMeta;
    Clay__ConfigureOpenElement(chevronDecl);
    Clay__CloseElement();

    Clay__CloseElement(); // contentRow

    if (!options.placeholder.empty()) {
      Clay__OpenElement();
      Clay_ElementDeclaration labelDecl = {};
      labelDecl.floating.attachTo = CLAY_ATTACH_TO_PARENT;
      labelDecl.floating.attachPoints = {CLAY_ATTACH_POINT_LEFT_TOP, CLAY_ATTACH_POINT_LEFT_TOP};
      labelDecl.floating.pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH;
      labelDecl.floating.zIndex = 10;
      if (paint.transitionSeconds > 0.0f) {
        labelDecl.transition.handler = Clay_EaseOut;
        labelDecl.transition.duration = paint.transitionSeconds;
        labelDecl.transition.properties = CLAY_TRANSITION_PROPERTY_BOUNDING_BOX;
      }
      uint16_t labelFontSize = (uint16_t)easeValue(animKey(ordinal, 32), labelFloated ? paint.labelFontSize : paint.fontSize, paint.transitionSeconds);
      if (labelFloated) {
        labelDecl.floating.offset = {(float)paint.padding.left, (float)paint.padding.top};
      } else {
        labelDecl.floating.offset = {(float)paint.padding.left, (contentHeight - (float)labelFontSize) / 2.0f};
      }
      Clay__ConfigureOpenElement(labelDecl);

      textStyleStorage.push_back(n8v::detail::TextStyleFlags{paint.font, false, false, false, true, ordinal});
      Clay_TextElementConfig labelTextConfig = {};
      labelTextConfig.textColor = toClay(paint.labelColor);
      labelTextConfig.fontSize = labelFontSize;
      labelTextConfig.wrapMode = CLAY_TEXT_WRAP_NONE;
      labelTextConfig.userData = &textStyleStorage.back();
      CLAY_TEXT(internString(options.placeholder), labelTextConfig);

      Clay__CloseElement();
    }

    if (paint.indicatorWidth > 0.0f) {
      Clay__OpenElement();
      Clay_ElementDeclaration indicatorDecl = {};
      indicatorDecl.layout.sizing.width = CLAY_SIZING_GROW(0);
      indicatorDecl.layout.sizing.height = CLAY_SIZING_FIXED(paint.indicatorWidth);
      indicatorDecl.backgroundColor = toClay(paint.indicatorColor);
      Clay__ConfigureOpenElement(indicatorDecl);
      Clay__CloseElement();
    }
  } else {
    textStyleStorage.push_back(n8v::detail::TextStyleFlags{paint.font, false, false, false, true, ordinal});
    Clay_TextElementConfig textConfig = {};
    textConfig.textColor = toClay(hasSelection ? paint.textColor : paint.placeholderColor);
    textConfig.fontSize = paint.fontSize;
    textConfig.wrapMode = CLAY_TEXT_WRAP_NONE;
    textConfig.userData = &textStyleStorage.back();
    CLAY_TEXT(internString(hasSelection ? selectedText : options.placeholder), textConfig);
  }

  if (!hasNativeChrome && open && !itemsCopy.empty()) {
    Clay__OpenElement();
    dropdownOrdinalStorage.push_back(ordinal);
    Clay_ElementDeclaration backdropDecl = {};
    Clay_Dimensions winSize = activeBackend().windowSize();
    backdropDecl.layout.sizing.width = CLAY_SIZING_FIXED(winSize.width);
    backdropDecl.layout.sizing.height = CLAY_SIZING_FIXED(winSize.height);
    backdropDecl.floating.attachTo = CLAY_ATTACH_TO_ROOT;
    backdropDecl.floating.zIndex = 900;
    Clay__ConfigureOpenElement(backdropDecl);
    Clay_OnHover(dispatchDropdownClose, &dropdownOrdinalStorage.back());
    Clay__CloseElement();

    Clay__OpenElement();
    Clay_ElementDeclaration popupDecl = {};
    popupDecl.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
    popupDecl.layout.sizing.width = CLAY_SIZING_GROW(0);
    popupDecl.backgroundColor = toClay(paint.popupBackground);
    Clay_CornerRadius popupRadius = floatingLabelStyle ? Clay_CornerRadius{0, 0, paint.cornerRadius.topLeft, paint.cornerRadius.topRight} : decl.cornerRadius;
    popupDecl.cornerRadius = popupRadius;
    popupDecl.floating.attachTo = CLAY_ATTACH_TO_PARENT;
    popupDecl.floating.attachPoints = {CLAY_ATTACH_POINT_LEFT_TOP, CLAY_ATTACH_POINT_LEFT_BOTTOM};
    if (!floatingLabelStyle) popupDecl.floating.offset.y = 6.0f;
    popupDecl.floating.zIndex = 1000;
    Clay__ConfigureOpenElement(popupDecl);

    for (size_t i = 0; i < itemsCopy.size(); ++i) {
      Clay__OpenElement();

      const bool itemHovered = Clay_Hovered();
      // if (itemHovered) pendingCursor = CursorKind::Pointer;
      const bool itemSelected = hasSelection && (size_t)*options.selected == i;

      Clay_ElementDeclaration rowDecl = {};
      rowDecl.layout.padding = toClay(paint.padding);
      rowDecl.layout.sizing.width = CLAY_SIZING_GROW(0);
      Color rowBg = itemHovered ? paint.itemHoverBackground : (itemSelected && paint.itemSelectedBackground.a > 0.0f ? paint.itemSelectedBackground : paint.popupBackground);
      rowDecl.backgroundColor = toClay(rowBg);
      bool isFirstRow = i == 0, isLastRow = i == itemsCopy.size() - 1;
      rowDecl.cornerRadius = {
        isFirstRow ? popupRadius.topLeft : 0.0f,
        isFirstRow ? popupRadius.topRight : 0.0f,
        isLastRow ? popupRadius.bottomLeft : 0.0f,
        isLastRow ? popupRadius.bottomRight : 0.0f
      };
      Clay__ConfigureOpenElement(rowDecl);

      dropdownItemClickStorage.push_back(DropdownItemClick{&meta, (int)i});
      Clay_OnHover(dispatchDropdownSelect, &dropdownItemClickStorage.back());

      textStyleStorage.push_back(n8v::detail::TextStyleFlags{paint.font, false, false, false, true, ordinal});
      Clay_TextElementConfig itemTextConfig = {};
      itemTextConfig.textColor = toClay(paint.textColor);
      itemTextConfig.fontSize = paint.fontSize;
      itemTextConfig.wrapMode = CLAY_TEXT_WRAP_NONE;
      itemTextConfig.userData = &textStyleStorage.back();
      CLAY_TEXT(internString(itemsCopy[i]), itemTextConfig);

      Clay__CloseElement();
    }

    Clay__CloseElement(); // popup
  }

  Clay__CloseElement();
}

} // namespace n8v::detail
