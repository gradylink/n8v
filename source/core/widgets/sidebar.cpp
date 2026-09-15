#include <n8v/n8v_c.h>

#include "core/backend.hpp"
#include "core/icon_loader.hpp"
#include "core/image_loader.hpp"
#include "core/style.hpp"

#include "core/clay_convert.hpp"
#include "core/native_widget_meta.hpp"
#include "core/text_style_flags.hpp"
#include "core/ui_core_internal.hpp"

#include <clay.h>

#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

using namespace n8v::detail::ui_internal;

uint16_t snapToGrid(uint16_t value, float unit) {
  if (unit <= 0.0f || value == 0) return value;
  return (uint16_t)(std::lround((float)value / unit) * unit);
}

uint16_t snapGapToGrid(uint16_t value, float unit) {
  uint16_t rounded = snapToGrid(value, unit);
  if (rounded == 0 && value > 0 && unit > 0.0f) rounded = (uint16_t)unit;
  return rounded;
}

Clay_ElementId sidebarContentAnchorId(int sidebarOrdinal) { return Clay_GetElementIdWithIndex(CLAY_STRING("n8v-sidebar-content"), (uint32_t)sidebarOrdinal); }

struct SidebarContext {
  int *selectedPtr = nullptr;
  std::function<void(int)> onChange;
  int pageOrdinal = 0;
  int ordinal = 0;
};

std::vector<SidebarContext> sidebarStack;

struct PageClickState {
  int *selectedPtr = nullptr;
  std::function<void(int)> onChange;
  int value = 0;
};

std::unordered_map<int, PageClickState> pageClickStates;

void dispatchPageClick(void *userdata) {
  auto *state = static_cast<PageClickState *>(userdata);
  if (!state || !state->selectedPtr || *state->selectedPtr == state->value) return;
  *state->selectedPtr = state->value;
  if (state->onChange) state->onChange(state->value);
}

void dispatchRowHover(Clay_ElementId /*elementId*/, Clay_PointerData pointerData, void *userData) {
  if (pointerData.state != CLAY_POINTER_DATA_PRESSED_THIS_FRAME) return;
  auto *meta = static_cast<n8v::detail::NativeWidgetMeta *>(userData);
  if (meta && meta->onClick) meta->onClick(meta->onClickUserdata);
}

} // namespace

namespace n8v::detail::ui_internal {

void resetSidebarFrameState() { sidebarStack.clear(); }

} // namespace n8v::detail::ui_internal

extern "C" {

void n8v_open_sidebar(n8v_sidebar_options opts) {
  Clay__OpenElement();
  Clay_ElementDeclaration shellDecl = {};
  shellDecl.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
  shellDecl.layout.sizing.width = CLAY_SIZING_GROW(0);
  shellDecl.layout.sizing.height = CLAY_SIZING_GROW(0);
  Clay__ConfigureOpenElement(shellDecl);

  const int ordinal = widgetOrdinal++;
  const n8v::SidebarPaint paint = n8v::activePaint().sidebar();
  const n8v::Sizing listWidthSizing = toSizing(opts.width);
  if (listWidthSizing.mode != n8v::SizingMode::Fixed) {
    std::fprintf(stderr, "[n8v] sidebar(): width should be Sizing::fixed(...) - the content pane's position is derived from it directly\n");
  }
  const float listWidthPx = listWidthSizing.value;

  Clay_Dimensions windowSize = n8v::activeBackend().windowSize();
  Clay__OpenElementWithId(sidebarContentAnchorId(ordinal));
  Clay_ElementDeclaration anchorDecl = {};
  anchorDecl.layout.sizing.width = CLAY_SIZING_FIXED(windowSize.width - listWidthPx);
  anchorDecl.layout.sizing.height = CLAY_SIZING_FIXED(windowSize.height);
  anchorDecl.floating.attachTo = CLAY_ATTACH_TO_ROOT;
  anchorDecl.floating.offset = {listWidthPx, 0};
  anchorDecl.floating.attachPoints.element = CLAY_ATTACH_POINT_LEFT_TOP;
  anchorDecl.floating.attachPoints.parent = CLAY_ATTACH_POINT_LEFT_TOP;
  Clay__ConfigureOpenElement(anchorDecl);
  Clay__CloseElement();

  n8v::Padding pad = paint.padding;
  uint16_t gap = (uint16_t)paint.rowGap;
  Clay_Dimensions cell = n8v::activeBackend().cellSize();
  if (cell.width > 0.0f && cell.height > 0.0f) {
    pad.left = snapToGrid(pad.left, cell.width);
    pad.right = snapToGrid(pad.right, cell.width);
    pad.top = snapToGrid(pad.top, cell.height);
    pad.bottom = snapToGrid(pad.bottom, cell.height);
    gap = snapGapToGrid(gap, cell.height);
  }

  Clay__OpenElement();
  Clay_ElementDeclaration decl = {};
  decl.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
  decl.layout.childGap = gap;
  decl.layout.padding = n8v::detail::toClay(pad);
  decl.layout.childAlignment.x = CLAY_ALIGN_X_CENTER;
  decl.layout.sizing.width = n8v::detail::toClay(toSizing(opts.width));
  decl.layout.sizing.height = CLAY_SIZING_GROW(0);
  decl.backgroundColor = n8v::detail::toClay(paint.background);
  decl.cornerRadius = n8v::detail::toClay(paint.cornerRadius);
  decl.border.color = n8v::detail::toClay(paint.borderColor);
  decl.border.width = {(uint16_t)paint.borderWidth, 0, 0, 0, 0};
  decl.clip.vertical = true;

  widgetMetaStorage.push_back(n8v::detail::NativeWidgetMeta{});
  n8v::detail::NativeWidgetMeta &meta = widgetMetaStorage.back();
  meta.kind = n8v::NativeWidgetKind::Sidebar;
  meta.ordinal = ordinal;
  decl.userData = &meta;

  Clay__ConfigureOpenElement(decl);

  std::string_view titleView = toView(opts.title);
  if (!titleView.empty()) {
    n8v_text_options textOpts = {};
    textOpts.bold = true;
    _n8v_set_text_opts(textOpts);
    _n8v_text_commit(opts.title);
  }

  sidebarStack.push_back(SidebarContext{opts.selected, n8v::detail::toStdFunction(opts.on_change, opts.on_change_userdata), 0, ordinal});
}

void n8v_close_sidebar(void) {
  sidebarStack.pop_back();

  Clay__CloseElement(); // list region
  Clay__CloseElement(); // shell
}

bool n8v_open_page(n8v_page_options opts) {
  if (sidebarStack.empty()) {
    std::fprintf(stderr, "[n8v] page() called outside of sidebar()\n");
    return false;
  }
  SidebarContext &ctx = sidebarStack.back();
  const int thisOrdinal = ctx.pageOrdinal++;

  std::string_view nameView = toView(opts.name);
  std::string_view iconView = toView(opts.icon);
  std::string_view imageView = toView(opts.image);
  if (!iconView.empty() && !imageView.empty()) {
    std::fprintf(stderr, "[n8v] page(): .icon and .image are mutually exclusive - using .icon\n");
    imageView = {};
  }
  const bool hasIcon = !iconView.empty();
  const bool hasImage = !imageView.empty();
  const bool selected = ctx.selectedPtr ? (*ctx.selectedPtr == thisOrdinal) : (thisOrdinal == 0);

  Clay__OpenElement();

  const int rowOrdinal = 1000000000 + ctx.ordinal * 1000 + thisOrdinal;
  const bool hovered = Clay_Hovered();
  const bool pressed = hovered && n8v::activeBackend().pointerDown();
  const n8v::ButtonPaint paint = n8v::activePaint().button(selected ? n8v::ButtonStyle::Primary : n8v::ButtonStyle::Secondary, hovered, pressed);

  std::string imagePathStorage(imageView);
  const n8v::detail::DecodedImage *rowImage = nullptr;
  if (hasIcon) {
    rowImage = n8v::detail::getOrDecodeIcon(iconView, paint.fontSize, paint.textColor);
  } else if (hasImage) {
    n8v_image_options imgOpts = {};
    imgOpts.source_kind = N8V_IMAGE_SOURCE_PATH;
    imgOpts.path = imagePathStorage.c_str();
    rowImage = n8v::detail::getOrDecodeImage(imgOpts);
  }

  Clay_ElementDeclaration decl = {};
  decl.layout.padding = n8v::detail::toClay(paint.padding);
  decl.layout.sizing.width = CLAY_SIZING_GROW(0);
  decl.backgroundColor = n8v::detail::toClay(paint.background);
  if (rowImage) {
    decl.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
    decl.layout.childGap = paint.fontSize / 2;
    decl.layout.childAlignment.y = CLAY_ALIGN_Y_CENTER;
  }
  float radius = paint.cornerRadius.topLeft;
  decl.cornerRadius = {radius, radius, radius, radius};

  Clay_Dimensions nativeSize = n8v::activeBackend().measureNativeChrome(n8v::NativeWidgetKind::Button, nameView, paint.fontSize, rowImage != nullptr);
  if (nativeSize.height > 0) decl.layout.sizing.height = CLAY_SIZING_FIXED(nativeSize.height);

  PageClickState *clickState = &pageClickStates[thisOrdinal];
  clickState->selectedPtr = ctx.selectedPtr;
  clickState->onChange = ctx.onChange;
  clickState->value = thisOrdinal;

  widgetMetaStorage.push_back(n8v::detail::NativeWidgetMeta{});
  n8v::detail::NativeWidgetMeta &meta = widgetMetaStorage.back();
  meta.kind = n8v::NativeWidgetKind::Button;
  meta.ordinal = rowOrdinal;
  meta.onClick = &dispatchPageClick;
  meta.onClickUserdata = clickState;
  meta.image = rowImage;
  meta.iconName = hasIcon ? internCString(iconView) : nullptr;
  meta.iconVariant = n8v::IconVariant::Outline;
  meta.buttonSelected = selected;
  decl.userData = &meta;

  Clay__ConfigureOpenElement(decl);
  Clay_OnHover(dispatchRowHover, &meta);

  if (rowImage && !n8v::activeBackend().rendersNativeChrome()) {
    Clay__OpenElement();
    Clay_ElementDeclaration iconDecl = {};
    iconDecl.image.imageData = const_cast<n8v::detail::DecodedImage *>(rowImage);
    iconDecl.layout.sizing.width = CLAY_SIZING_FIXED((float)paint.fontSize);
    iconDecl.layout.sizing.height = CLAY_SIZING_FIXED((float)paint.fontSize);
    widgetMetaStorage.push_back(n8v::detail::NativeWidgetMeta{});
    n8v::detail::NativeWidgetMeta &iconMeta = widgetMetaStorage.back();
    iconMeta.kind = n8v::NativeWidgetKind::Icon;
    iconMeta.ordinal = widgetOrdinal++;
    iconMeta.image = rowImage;
    iconMeta.iconName = hasIcon ? internCString(iconView) : nullptr;
    iconMeta.iconTint = paint.textColor;
    iconDecl.userData = &iconMeta;
    Clay__ConfigureOpenElement(iconDecl);
    Clay__CloseElement();
  }

  textStyleStorage.push_back(n8v::detail::TextStyleFlags{paint.font, false, false, false, true, rowOrdinal});
  Clay_TextElementConfig textConfig = {};
  textConfig.textColor = n8v::detail::toClay(paint.textColor);
  textConfig.fontSize = paint.fontSize;
  textConfig.wrapMode = CLAY_TEXT_WRAP_NONE;
  textConfig.userData = &textStyleStorage.back();
  CLAY_TEXT(internString(nameView), textConfig);

  Clay__CloseElement(); // row

  if (!selected) return false;

  Clay__OpenElement();
  Clay_ElementDeclaration contentDecl = {};
  contentDecl.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
  contentDecl.layout.sizing.width = CLAY_SIZING_GROW(0);
  contentDecl.layout.sizing.height = CLAY_SIZING_GROW(0);
  contentDecl.floating.attachTo = CLAY_ATTACH_TO_ELEMENT_WITH_ID;
  contentDecl.floating.parentId = sidebarContentAnchorId(ctx.ordinal).id;
  contentDecl.floating.attachPoints.element = CLAY_ATTACH_POINT_LEFT_TOP;
  contentDecl.floating.attachPoints.parent = CLAY_ATTACH_POINT_LEFT_TOP;
  Clay__ConfigureOpenElement(contentDecl);
  return true;
}

void n8v_close_page(void) { Clay__CloseElement(); }

} // extern "C"
