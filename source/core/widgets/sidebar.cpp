#include "core/widgets/page_badge.hpp"
#include <n8v/n8v_c.h>

#include "core/backend.hpp"
#include "core/icon_loader.hpp"
#include "core/image_loader.hpp"
#include "core/style.hpp"

#include "core/clay_convert.hpp"
#include "core/native_widget_meta.hpp"
#include "core/text_style_flags.hpp"
#include "core/theme_color.hpp"
#include "core/tree/emit.hpp"
#include "core/ui_core_internal.hpp"

#include <clay.h>

#include <algorithm>
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

constexpr float kCollapsedWidth = 72.0f;
constexpr float kCollapsedRowHeight = 60.0f;
constexpr float kCollapsedIconSize = 40.0f;
constexpr float kCollapsedGlyphSize = 24.0f;
constexpr float kCollapseButtonSize = 28.0f;

struct SidebarContext {
  int *selectedPtr = nullptr;
  std::function<void(int)> onChange;
  int pageOrdinal = 0;
  int ordinal = 0;
  bool compact = false;
  float contentWidth = 0.0f;
  bool collapsible = false;
  bool collapsed = false;
};

struct CollapseState {
  bool *collapsed = nullptr;
  n8v_bool_change_fn onCollapse = nullptr;
  void *onCollapseUserdata = nullptr;
};

std::unordered_map<int, CollapseState> collapseStates;

void dispatchCollapse(void *userdata) {
  auto *state = static_cast<CollapseState *>(userdata);
  if (!state || !state->collapsed) return;
  *state->collapsed = !*state->collapsed;
  if (state->onCollapse) state->onCollapse(*state->collapsed, state->onCollapseUserdata);
}

n8v::Color badgeFill(int color) {
  if (color <= 0) {
    n8v::Color text = n8v::activePaint().text(n8v::TextOptions{}).color;
    text.a = 46;
    return text;
  }
  const n8v_color named = n8v::detail::builtinThemeColor((n8v_named_color)color, false);
  return {named.r, named.g, named.b, 255.0f};
}

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

/** The arrow button that folds and unfolds a sidebar: a small hoverable square with a chevron. */
void emitCollapseButton(const SidebarContext &ctx, int key, bool pointsRight) {
  const n8v::ButtonPaint paint = n8v::activePaint().button(n8v::ButtonStyle::Ghost, false, false);
  Clay__OpenElement();
  const bool hovered = Clay_Hovered();
  const n8v::ButtonPaint hoverPaint = n8v::activePaint().button(n8v::ButtonStyle::Ghost, hovered, hovered && n8v::activeBackend().pointerDown());
  if (hovered) pendingCursor = n8v::CursorKind::Pointer;

  Clay_ElementDeclaration decl = {};
  decl.layout.sizing.width = CLAY_SIZING_FIXED(kCollapseButtonSize);
  decl.layout.sizing.height = CLAY_SIZING_FIXED(kCollapseButtonSize);
  decl.layout.childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER};
  decl.backgroundColor = n8v::detail::toClay(hoverPaint.background);
  decl.cornerRadius = {5, 5, 5, 5};

  widgetMetaStorage.push_back(n8v::detail::NativeWidgetMeta{});
  n8v::detail::NativeWidgetMeta &meta = widgetMetaStorage.back();
  meta.kind = n8v::NativeWidgetKind::Button;
  meta.ordinal = key;
  meta.onClick = &dispatchCollapse;
  meta.onClickUserdata = &collapseStates[ctx.ordinal];
  decl.userData = &meta;
  Clay__ConfigureOpenElement(decl);
  Clay_OnHover(dispatchRowHover, &meta);

  const int iconSize = (int)(paint.fontSize + 2);
  const n8v::detail::DecodedImage *chevron = n8v::detail::getOrDecodeIcon(pointsRight ? "chevron-right" : "chevron-left", (uint16_t)iconSize, paint.textColor);
  if (chevron) {
    Clay__OpenElement();
    Clay_ElementDeclaration iconDecl = {};
    iconDecl.image.imageData = const_cast<n8v::detail::DecodedImage *>(chevron);
    iconDecl.layout.sizing.width = CLAY_SIZING_FIXED((float)iconSize);
    iconDecl.layout.sizing.height = CLAY_SIZING_FIXED((float)iconSize);
    widgetMetaStorage.push_back(n8v::detail::NativeWidgetMeta{});
    n8v::detail::NativeWidgetMeta &iconMeta = widgetMetaStorage.back();
    iconMeta.kind = n8v::NativeWidgetKind::Icon;
    iconMeta.ordinal = key + 1;
    iconMeta.image = chevron;
    iconMeta.iconName = pointsRight ? "chevron-right" : "chevron-left";
    iconMeta.iconTint = paint.textColor;
    iconDecl.userData = &iconMeta;
    Clay__ConfigureOpenElement(iconDecl);
    Clay__CloseElement();
  }
  Clay__CloseElement();
}

} // namespace

namespace n8v::detail::ui_internal {

void releaseSidebarState(int ordinal) {
  pageClickStates.erase(ordinal);
  collapseStates.erase(ordinal);
}

void resetSidebarFrameState() { sidebarStack.clear(); }

} // namespace n8v::detail::ui_internal

namespace n8v::detail::emit {

void openSidebar(const n8v_sidebar_options &opts, int ordinal) {
  Clay__OpenElement();
  Clay_ElementDeclaration shellDecl = {};
  shellDecl.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
  shellDecl.layout.sizing.width = CLAY_SIZING_GROW(0);
  shellDecl.layout.sizing.height = CLAY_SIZING_GROW(0);
  Clay__ConfigureOpenElement(shellDecl);

  const n8v::SidebarPaint paint = n8v::activePaint().sidebar();
  const n8v::Sizing listWidthSizing = toSizing(opts.width);
  if (listWidthSizing.mode != n8v::SizingMode::Fixed) {
    std::fprintf(stderr, "[n8v] sidebar(): width should be Sizing::fixed(...) - the content pane's position is derived from it directly\n");
  }
  const bool collapsible = opts.collapsed != nullptr;
  const bool collapsed = collapsible && *opts.collapsed;
  if (collapsible) collapseStates[ordinal] = CollapseState{opts.collapsed, opts.on_collapse, opts.on_collapse_userdata};
  const float listWidthPx = collapsed ? kCollapsedWidth : listWidthSizing.value;

  Clay_Dimensions windowSize = n8v::activeBackend().windowSize();
  float availableWidth = sidebarStack.empty() ? windowSize.width : sidebarStack.back().contentWidth;
  float contentWidth = availableWidth - listWidthPx;

  Clay__OpenElementWithId(sidebarContentAnchorId(ordinal));
  Clay_ElementDeclaration anchorDecl = {};
  anchorDecl.layout.sizing.width = CLAY_SIZING_FIXED(contentWidth);
  anchorDecl.layout.sizing.height = CLAY_SIZING_FIXED(windowSize.height);
  anchorDecl.floating.attachTo = CLAY_ATTACH_TO_PARENT;
  anchorDecl.floating.offset = {listWidthPx, 0};
  anchorDecl.floating.attachPoints.element = CLAY_ATTACH_POINT_LEFT_TOP;
  anchorDecl.floating.attachPoints.parent = CLAY_ATTACH_POINT_LEFT_TOP;
  Clay__ConfigureOpenElement(anchorDecl);
  Clay__CloseElement();

  n8v::Padding pad = paint.padding;
  if (collapsed) pad.left = pad.right = 6;
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
  decl.layout.sizing.width = collapsed ? CLAY_SIZING_FIXED(kCollapsedWidth) : n8v::detail::toClay(toSizing(opts.width));
  decl.layout.sizing.height = CLAY_SIZING_GROW(0);
  decl.backgroundColor = n8v::detail::toClay(paint.background);
  decl.cornerRadius = n8v::detail::toClay(paint.cornerRadius);
  decl.border.color = n8v::detail::toClay(paint.borderColor);
  decl.border.width = {0, (uint16_t)paint.borderWidth, 0, 0, 0}; // along the edge facing the content
  decl.clip.vertical = true;
  if (n8v::activeBackend().ownsScrollMath()) {
    decl.clip.childOffset = Clay_GetScrollOffset();
  }

  widgetMetaStorage.push_back(n8v::detail::NativeWidgetMeta{});
  n8v::detail::NativeWidgetMeta &meta = widgetMetaStorage.back();
  meta.kind = n8v::NativeWidgetKind::Sidebar;
  meta.ordinal = ordinal;
  meta.sidebarCompact = opts.compact;
  decl.userData = &meta;

  Clay__ConfigureOpenElement(decl);

  SidebarContext context{opts.selected, n8v::detail::toStdFunction(opts.on_change, opts.on_change_userdata), 0, ordinal, opts.compact, contentWidth, collapsible, collapsed};

  std::string_view titleView = toView(opts.title);
  if (!collapsed && (!titleView.empty() || collapsible)) {
    Clay__OpenElement();
    Clay_ElementDeclaration headerDecl = {};
    headerDecl.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
    headerDecl.layout.sizing.width = CLAY_SIZING_GROW(0);
    headerDecl.layout.childAlignment.y = CLAY_ALIGN_Y_CENTER;
    headerDecl.layout.padding = {0, 0, 0, 8}; // a little air between the title and the first row
    Clay__ConfigureOpenElement(headerDecl);
    if (collapsible) {
      Clay__OpenElement();
      Clay_ElementDeclaration spacer = {};
      spacer.layout.sizing.width = CLAY_SIZING_FIXED(kCollapseButtonSize);
      Clay__ConfigureOpenElement(spacer);
      Clay__CloseElement();
    }
    Clay__OpenElement();
    Clay_ElementDeclaration titleDecl = {};
    titleDecl.layout.sizing.width = CLAY_SIZING_GROW(0);
    titleDecl.layout.childAlignment.x = CLAY_ALIGN_X_CENTER;
    Clay__ConfigureOpenElement(titleDecl);
    if (!titleView.empty()) {
      n8v_text_options textOpts = {};
      textOpts.bold = true;
      text(textOpts, titleView, ordinal + 1);
    }
    Clay__CloseElement();
    if (collapsible) emitCollapseButton(context, ordinal + 5001, false);
    Clay__CloseElement();
  }

  sidebarStack.push_back(context);
}

void closeSidebar() {
  const SidebarContext context = sidebarStack.back();
  if (context.collapsible && context.collapsed) {
    Clay__OpenElement();
    Clay_ElementDeclaration fill = {};
    fill.layout.sizing.height = CLAY_SIZING_GROW(0);
    Clay__ConfigureOpenElement(fill);
    Clay__CloseElement();
    emitCollapseButton(context, context.ordinal + 5002, true);
  }
  sidebarStack.pop_back();

  Clay__CloseElement(); // list region
  Clay__CloseElement(); // shell
}

void sidebarSection(const n8v_sidebar_section_options &opts, int ordinal) {
  if (sidebarStack.empty()) return;
  const n8v::SidebarPaint paint = n8v::activePaint().sidebar();
  const n8v::ButtonPaint rowPaint = n8v::activePaint().button(n8v::ButtonStyle::Secondary, false, false);

  Clay__OpenElement();
  Clay_ElementDeclaration decl = {};
  decl.layout.padding = {(uint16_t)rowPaint.padding.left, (uint16_t)rowPaint.padding.right, (uint16_t)(rowPaint.fontSize * 0.8f), (uint16_t)(rowPaint.fontSize * 0.2f)};
  decl.layout.sizing.width = CLAY_SIZING_GROW(0);
  Clay__ConfigureOpenElement(decl);

  n8v_text_options textOpts = {};
  textOpts.bold = true;
  n8v::Color dim = n8v::activePaint().text(n8v::TextOptions{}).color;
  dim.a *= 0.65f;
  textOpts.color = {dim.r, dim.g, dim.b, dim.a};
  text(textOpts, toView(opts.title), ordinal);

  Clay__CloseElement();
  (void)paint;
}

void sidebarDivider(int /*ordinal*/) {
  if (sidebarStack.empty()) return;
  n8v::Color line = n8v::activePaint().text(n8v::TextOptions{}).color;
  line.a *= 0.18f;

  Clay__OpenElement();
  Clay_ElementDeclaration box = {};
  box.layout.sizing.width = CLAY_SIZING_GROW(0);
  box.layout.padding = {0, 0, 8, 8};
  Clay__ConfigureOpenElement(box);

  Clay__OpenElement();
  Clay_ElementDeclaration decl = {};
  decl.layout.sizing.width = CLAY_SIZING_GROW(0);
  decl.layout.sizing.height = CLAY_SIZING_FIXED(1);
  decl.backgroundColor = n8v::detail::toClay(line);
  Clay__ConfigureOpenElement(decl);
  Clay__CloseElement();

  Clay__CloseElement();
}

bool openPage(const n8v_page_options &opts, int rowOrdinal) {
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

  const bool hovered = Clay_Hovered();
  const bool pressed = hovered && n8v::activeBackend().pointerDown();
  n8v::ButtonPaint paint = n8v::activePaint().button(selected ? n8v::ButtonStyle::Primary : n8v::ButtonStyle::Secondary, hovered, pressed);
  paint.padding = n8v::activePaint().button(n8v::ButtonStyle::Secondary, false, false).padding;
  {
    const n8v::Padding rowPadding = n8v::activePaint().sidebar().rowPadding;
    if (rowPadding.left || rowPadding.right || rowPadding.top || rowPadding.bottom) paint.padding = rowPadding;
  }
  {
    const n8v::SidebarPaint rows = n8v::activePaint().sidebar();
    const n8v::Color rowBackground = selected && rows.rowSelectedBackground.a > 0.0f ? rows.rowSelectedBackground
                                     : hovered && rows.rowHoverBackground.a > 0.0f   ? rows.rowHoverBackground
                                                                                     : rows.rowBackground;
    if (rows.rowSelectedBackground.a > 0.0f || rows.rowHoverBackground.a > 0.0f || rows.rowBackground.a > 0.0f) paint.background = rowBackground;
    const n8v::Color rowText = selected && rows.rowSelectedTextColor.a > 0.0f ? rows.rowSelectedTextColor : rows.rowTextColor;
    if (rowText.a > 0.0f) paint.textColor = rowText;
    if (rows.rowCornerRadius > 0.0f) paint.cornerRadius = {rows.rowCornerRadius, rows.rowCornerRadius, rows.rowCornerRadius, rows.rowCornerRadius};
  }

  const bool collapsed = ctx.collapsed;
  const float iconSlotSize = collapsed ? (hasImage ? kCollapsedIconSize : kCollapsedGlyphSize) : hasImage ? (opts.image_size > 0.0f ? opts.image_size : (float)paint.fontSize * 2.0f) : (float)paint.fontSize;

  std::string imagePathStorage(imageView);
  const n8v::detail::DecodedImage *rowImage = nullptr;
  if (hasIcon) {
    rowImage = n8v::detail::getOrDecodeIcon(iconView, (uint16_t)std::lround(iconSlotSize), paint.textColor);
  } else if (hasImage) {
    n8v_image_options imgOpts = {};
    imgOpts.source_kind = N8V_IMAGE_SOURCE_PATH;
    imgOpts.path = imagePathStorage.c_str();
    rowImage = n8v::detail::getOrDecodeImage(imgOpts);
    rowImage = n8v::detail::getOrFitImageInSquare(rowImage, (int)std::lround(iconSlotSize));
  }

  Clay_ElementDeclaration decl = {};
  decl.layout.padding = n8v::detail::toClay(paint.padding);
  decl.layout.sizing.width = CLAY_SIZING_GROW(0);
  decl.backgroundColor = n8v::detail::toClay(paint.background);
  decl.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
  decl.layout.childGap = paint.fontSize / 2;
  decl.layout.childAlignment.y = CLAY_ALIGN_Y_CENTER;
  if (collapsed) {
    decl.layout.padding = {0, 0, 0, 0};
    decl.layout.childAlignment.x = CLAY_ALIGN_X_CENTER;
    decl.layout.sizing.height = CLAY_SIZING_FIXED(kCollapsedRowHeight);
  } else if (!ctx.compact) {
    decl.layout.sizing.height = CLAY_SIZING_FIXED((float)paint.fontSize * 2.0f + paint.padding.top + paint.padding.bottom);
  }
  float radius = easeValue(animKey(rowOrdinal, 0), paint.cornerRadius.topLeft, paint.transitionSeconds);
  decl.cornerRadius = {radius, radius, radius, radius};
  if (paint.transitionSeconds > 0.0f) {
    decl.transition.handler = Clay_EaseOut;
    decl.transition.duration = paint.transitionSeconds;
    decl.transition.properties = CLAY_TRANSITION_PROPERTY_BACKGROUND_COLOR;
  }

  Clay_Dimensions nativeSize =
    n8v::activeBackend().measureNativeChrome(n8v::NativeWidgetKind::Button, nameView, paint.fontSize, rowImage != nullptr, hasImage ? iconSlotSize : 0.0f);
  if (nativeSize.height > 0 && !collapsed) decl.layout.sizing.height = CLAY_SIZING_FIXED(nativeSize.height);

  n8v::CornerRadius imageCornerRadius{};
  if (hasImage) {
    n8v::Rounding imageRounding = toRounding(opts.image_rounding);
    switch (imageRounding.mode) {
    case n8v::RoundingMode::StyleDefault:
      imageCornerRadius = n8v::activePaint().image().cornerRadius;
      break;
    case n8v::RoundingMode::None:
      imageCornerRadius = {};
      break;
    case n8v::RoundingMode::Fixed:
      imageCornerRadius = imageRounding.radius;
      break;
    }
  }

  PageClickState *clickState = &pageClickStates[rowOrdinal];
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
  meta.imageBoxSize = hasImage ? iconSlotSize : 0.0f;
  meta.imageCornerRadius = imageCornerRadius;
  meta.iconName = hasIcon ? internCString(iconView) : nullptr;
  meta.iconVariant = n8v::IconVariant::Outline;
  meta.buttonSelected = selected;
  meta.sidebarCompact = ctx.compact;
  decl.userData = &meta;

  Clay__ConfigureOpenElement(decl);
  Clay_OnHover(dispatchRowHover, &meta);

  if (rowImage && !n8v::activeBackend().rendersNativeChrome()) {
    Clay__OpenElement();
    Clay_ElementDeclaration iconDecl = {};
    iconDecl.image.imageData = const_cast<n8v::detail::DecodedImage *>(rowImage);
    iconDecl.layout.sizing.width = CLAY_SIZING_FIXED(iconSlotSize);
    iconDecl.layout.sizing.height = CLAY_SIZING_FIXED(iconSlotSize);
    if (hasImage) iconDecl.cornerRadius = n8v::detail::toClay(imageCornerRadius);
    widgetMetaStorage.push_back(n8v::detail::NativeWidgetMeta{});
    n8v::detail::NativeWidgetMeta &iconMeta = widgetMetaStorage.back();
    iconMeta.kind = n8v::NativeWidgetKind::Icon;
    iconMeta.ordinal = rowOrdinal + 1;
    iconMeta.image = rowImage;
    iconMeta.imageCornerRadius = imageCornerRadius;
    iconMeta.iconName = hasIcon ? internCString(iconView) : nullptr;
    iconMeta.iconTint = paint.textColor;
    iconDecl.userData = &iconMeta;
    Clay__ConfigureOpenElement(iconDecl);
    Clay__CloseElement();
  }

  if (opts.notify) {
    Clay__OpenElement();
    Clay_ElementDeclaration bar = {};
    bar.layout.sizing.width = CLAY_SIZING_FIXED(3);
    bar.layout.sizing.height = CLAY_SIZING_FIXED(16);
    bar.backgroundColor = n8v::detail::toClay(paint.textColor);
    bar.cornerRadius = {2, 2, 2, 2};
    bar.floating.attachTo = CLAY_ATTACH_TO_PARENT;
    bar.floating.attachPoints.element = CLAY_ATTACH_POINT_LEFT_CENTER;
    bar.floating.attachPoints.parent = CLAY_ATTACH_POINT_LEFT_CENTER;
    bar.floating.offset = {collapsed ? 2.0f : 3.0f, 0};
    bar.floating.pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH;
    Clay__ConfigureOpenElement(bar);
    Clay__CloseElement();
  }

  if (collapsed && !rowImage && !nameView.empty()) {
    size_t length = 1;
    const unsigned char lead = (unsigned char)nameView[0];
    if (lead >= 0xF0) length = 4;
    else if (lead >= 0xE0) length = 3;
    else if (lead >= 0xC0) length = 2;
    textStyleStorage.push_back(n8v::detail::TextStyleFlags{paint.font, true, false, false, true, rowOrdinal});
    Clay_TextElementConfig initial = {};
    initial.textColor = n8v::detail::toClay(paint.textColor);
    initial.fontSize = (uint16_t)(paint.fontSize + 5);
    initial.fontId = n8v::detail::textFontId(paint.font, true, false);
    initial.wrapMode = CLAY_TEXT_WRAP_NONE;
    initial.userData = &textStyleStorage.back();
    CLAY_TEXT(internString(std::string(nameView.substr(0, std::min(length, nameView.size())))), initial);
  }

  if (!collapsed) {
    Clay__OpenElement();
    Clay_ElementDeclaration nameBox = {};
    nameBox.layout.sizing.width = CLAY_SIZING_GROW(0);
    nameBox.clip.horizontal = true;
    Clay__ConfigureOpenElement(nameBox);
    textStyleStorage.push_back(n8v::detail::TextStyleFlags{paint.font, opts.notify, false, false, true, rowOrdinal});
    Clay_TextElementConfig textConfig = {};
    textConfig.textColor = n8v::detail::toClay(paint.textColor);
    textConfig.fontSize = paint.fontSize;
    textConfig.fontId = n8v::detail::textFontId(paint.font, opts.notify, false);
    textConfig.wrapMode = CLAY_TEXT_WRAP_NONE;
    textConfig.userData = &textStyleStorage.back();
    CLAY_TEXT(internString(std::string(nameView)), textConfig);
    Clay__CloseElement();

    if (opts.badge && *opts.badge) {

      Clay__OpenElement();
      Clay_ElementDeclaration pill = {};
      pill.layout.padding = {7, 7, 1, 1};
      pill.layout.childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER};
      pill.backgroundColor = n8v::detail::toClay(badgeFill((int)opts.badge_color));
      const float pillRadius = paint.fontSize;
      pill.cornerRadius = {pillRadius, pillRadius, pillRadius, pillRadius};
      Clay__ConfigureOpenElement(pill);
      textStyleStorage.push_back(n8v::detail::TextStyleFlags{paint.font, true, false, false, true, rowOrdinal, false, false, true});
      Clay_TextElementConfig badgeText = {};
      badgeText.textColor = n8v::detail::toClay(opts.badge_color > 0 ? n8v::Color{255, 255, 255, 255} : paint.textColor);
      badgeText.fontSize = (uint16_t)std::max(8L, std::lround(paint.fontSize * 0.8));
      badgeText.fontId = n8v::detail::textFontId(paint.font, true, false);
      badgeText.wrapMode = CLAY_TEXT_WRAP_NONE;
      badgeText.userData = &textStyleStorage.back();
      CLAY_TEXT(internString(std::string(opts.badge)), badgeText);
      Clay__CloseElement();
    }
  }

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

void closePage() { Clay__CloseElement(); }

} // namespace n8v::detail::emit
