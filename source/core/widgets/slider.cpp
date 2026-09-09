#include <n8v/backend.hpp>
#include <n8v/style.hpp>
#include <n8v/ui.hpp>

#include "core/clay_convert.hpp"
#include "core/native_widget_meta.hpp"
#include "core/ui_core_internal.hpp"

#include <clay.h>

#include <deque>
#include <functional>

namespace {

using namespace n8v::detail::ui_internal;

std::deque<std::function<void(float)>> sliderChangeCallbacks;
int draggingSliderOrdinal = -1;

Clay_ElementId sliderTrackId(int ordinal) { return Clay__HashStringWithOffset(CLAY_STRING("n8v-slider-track"), (uint32_t)ordinal, 0); }

void dispatchSliderDrag(Clay_ElementId elementId, Clay_PointerData pointerData, void *userData) {
  if (pointerData.state != CLAY_POINTER_DATA_PRESSED_THIS_FRAME) return;
  auto *meta = static_cast<n8v::detail::NativeWidgetMeta *>(userData);
  if (!meta || !meta->sliderValue) return;
  draggingSliderOrdinal = meta->ordinal;
  Clay_ElementData data = Clay_GetElementData(elementId);
  if (!data.found || data.boundingBox.width <= 0.0f) return;
  float fraction = (pointerData.position.x - data.boundingBox.x) / data.boundingBox.width;
  if (fraction < 0.0f) fraction = 0.0f;
  if (fraction > 1.0f) fraction = 1.0f;
  float newValue = meta->sliderMin + fraction * (meta->sliderMax - meta->sliderMin);
  if (newValue == *meta->sliderValue) return;
  *meta->sliderValue = newValue;
  if (meta->onSliderChange && *meta->onSliderChange) (*meta->onSliderChange)(newValue);
}

} // namespace

namespace n8v::detail::ui_internal {

void resetSliderFrameState() { sliderChangeCallbacks.clear(); }

} // namespace n8v::detail::ui_internal

namespace n8v::detail {

void slider(const SliderOptions &options) {
  const int ordinal = widgetOrdinal++;

  if (draggingSliderOrdinal == ordinal && options.value) {
    Clay_PointerData globalPointer = Clay_GetPointerState();
    if (globalPointer.state == CLAY_POINTER_DATA_PRESSED || globalPointer.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME) {
      Clay_ElementData data = Clay_GetElementData(sliderTrackId(ordinal));
      if (data.found && data.boundingBox.width > 0.0f) {
        float dragFraction = (globalPointer.position.x - data.boundingBox.x) / data.boundingBox.width;
        if (dragFraction < 0.0f) dragFraction = 0.0f;
        if (dragFraction > 1.0f) dragFraction = 1.0f;
        float newValue = options.min + dragFraction * (options.max - options.min);
        if (newValue != *options.value) {
          *options.value = newValue;
          if (options.onChange) options.onChange(newValue);
        }
      }
    } else {
      draggingSliderOrdinal = -1;
    }
  }

  Clay__OpenElementWithId(sliderTrackId(ordinal));

  const bool hovered = Clay_Hovered();
  // if (hovered) pendingCursor = CursorKind::Pointer;
  const bool pressed = hovered && activeBackend().pointerDown();
  const SliderPaint paint = activePaint().slider(hovered, pressed);

  Clay_Dimensions nativeSize = activeBackend().measureNativeChrome(NativeWidgetKind::Slider, {}, 0);
  const bool hasNativeChrome = nativeSize.height > 0;

  float outerHeight = hasNativeChrome ? nativeSize.height : (paint.trackHeight > paint.thumbHeight ? paint.trackHeight : paint.thumbHeight);

  float range = options.max - options.min;
  float rawValue = options.value ? *options.value : options.min;
  if (rawValue < options.min) rawValue = options.min;
  if (rawValue > options.max) rawValue = options.max;
  float fraction = range > 0.0f ? (rawValue - options.min) / range : 0.0f;

  Clay_ElementDeclaration decl = {};
  decl.layout.sizing.width = CLAY_SIZING_GROW(0);
  decl.layout.sizing.height = CLAY_SIZING_FIXED(outerHeight);
  decl.layout.childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER};
  decl.backgroundColor = {0, 0, 0, 1};

  const bool hasOnChange = static_cast<bool>(options.onChange);
  if (hasOnChange) sliderChangeCallbacks.push_back(options.onChange);

  widgetMetaStorage.push_back(NativeWidgetMeta{});
  NativeWidgetMeta &meta = widgetMetaStorage.back();
  meta.kind = NativeWidgetKind::Slider;
  meta.ordinal = ordinal;
  meta.sliderValue = options.value;
  meta.sliderMin = options.min;
  meta.sliderMax = options.max;
  meta.onSliderChange = hasOnChange ? &sliderChangeCallbacks.back() : nullptr;
  decl.userData = &meta;

  Clay__ConfigureOpenElement(decl);

  if (!hasNativeChrome && options.value) {
    Clay_OnHover(dispatchSliderDrag, &meta);
  }

  if (!hasNativeChrome) {
    float trackRadius = paint.trackHeight / 2.0f;
    float thumbRadius = (paint.thumbWidth < paint.thumbHeight ? paint.thumbWidth : paint.thumbHeight) / 2.0f;

    if (paint.trackGap > 0.0f) {
      Clay__OpenElement();
      Clay_ElementDeclaration fillDecl = {};
      fillDecl.layout.sizing.width = CLAY_SIZING_PERCENT(fraction);
      fillDecl.layout.sizing.height = CLAY_SIZING_FIXED(paint.trackHeight);
      fillDecl.backgroundColor = toClay(paint.fillColor);
      fillDecl.cornerRadius = {trackRadius, 2.0f, trackRadius, 2.0f};
      Clay__ConfigureOpenElement(fillDecl);
      Clay__CloseElement();

      Clay__OpenElement();
      Clay_ElementDeclaration gapDecl = {};
      gapDecl.layout.sizing.width = CLAY_SIZING_FIXED(paint.trackGap * 2.0f);
      gapDecl.layout.sizing.height = CLAY_SIZING_GROW(0);
      gapDecl.layout.childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER};
      Clay__ConfigureOpenElement(gapDecl);

      Clay__OpenElement();
      Clay_ElementDeclaration thumbDecl = {};
      thumbDecl.layout.sizing.width = CLAY_SIZING_FIXED(paint.thumbWidth);
      thumbDecl.layout.sizing.height = CLAY_SIZING_FIXED(paint.thumbHeight);
      thumbDecl.backgroundColor = toClay(paint.thumbColor);
      thumbDecl.cornerRadius = {thumbRadius, thumbRadius, thumbRadius, thumbRadius};
      if (paint.thumbBorderWidth > 0.0f) {
        thumbDecl.border.color = toClay(paint.thumbBorderColor);
        uint16_t thumbBw = (uint16_t)paint.thumbBorderWidth;
        thumbDecl.border.width = {thumbBw, thumbBw, thumbBw, thumbBw, 0};
      }
      Clay__ConfigureOpenElement(thumbDecl);
      Clay__CloseElement(); // thumb

      Clay__CloseElement(); // gap

      Clay__OpenElement();
      Clay_ElementDeclaration trackDecl = {};
      trackDecl.layout.sizing.width = CLAY_SIZING_GROW(0);
      trackDecl.layout.sizing.height = CLAY_SIZING_FIXED(paint.trackHeight);
      trackDecl.backgroundColor = toClay(paint.trackColor);
      trackDecl.cornerRadius = {2.0f, trackRadius, 2.0f, trackRadius};
      Clay__ConfigureOpenElement(trackDecl);
      Clay__CloseElement();
    } else {
      Clay__OpenElement();
      Clay_ElementDeclaration barDecl = {};
      barDecl.layout.sizing.width = CLAY_SIZING_GROW(0);
      barDecl.layout.sizing.height = CLAY_SIZING_FIXED(paint.trackHeight);
      barDecl.layout.childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER};
      Clay__ConfigureOpenElement(barDecl);

      Clay__OpenElement();
      Clay_ElementDeclaration fillDecl = {};
      fillDecl.layout.sizing.width = CLAY_SIZING_PERCENT(fraction);
      fillDecl.layout.sizing.height = CLAY_SIZING_GROW(0);
      fillDecl.backgroundColor = toClay(paint.fillColor);
      fillDecl.cornerRadius = {trackRadius, trackRadius, trackRadius, trackRadius};
      Clay__ConfigureOpenElement(fillDecl);
      Clay__CloseElement();

      Clay__OpenElement();
      Clay_ElementDeclaration trackDecl = {};
      trackDecl.layout.sizing.width = CLAY_SIZING_GROW(0);
      trackDecl.layout.sizing.height = CLAY_SIZING_GROW(0);
      trackDecl.backgroundColor = toClay(paint.trackColor);
      trackDecl.cornerRadius = {trackRadius, trackRadius, trackRadius, trackRadius};
      Clay__ConfigureOpenElement(trackDecl);
      Clay__CloseElement();

      Clay__CloseElement(); // bar

      Clay_ElementData outerData = Clay_GetElementData(sliderTrackId(ordinal));
      float outerWidthPx = outerData.found ? outerData.boundingBox.width : 0.0f;
      float thumbHalfWidth = paint.thumbWidth / 2.0f;

      Clay__OpenElement();
      Clay_ElementDeclaration thumbWrapDecl = {};
      thumbWrapDecl.floating.attachTo = CLAY_ATTACH_TO_PARENT;
      thumbWrapDecl.floating.attachPoints = {CLAY_ATTACH_POINT_LEFT_CENTER, CLAY_ATTACH_POINT_LEFT_CENTER};
      thumbWrapDecl.floating.pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH;
      thumbWrapDecl.floating.offset = {fraction * outerWidthPx - thumbHalfWidth, 0.0f};
      Clay__ConfigureOpenElement(thumbWrapDecl);

      Clay__OpenElement();
      Clay_ElementDeclaration thumbDecl = {};
      thumbDecl.layout.sizing.width = CLAY_SIZING_FIXED(paint.thumbWidth);
      thumbDecl.layout.sizing.height = CLAY_SIZING_FIXED(paint.thumbHeight);
      thumbDecl.backgroundColor = toClay(paint.thumbColor);
      thumbDecl.cornerRadius = {thumbRadius, thumbRadius, thumbRadius, thumbRadius};
      if (paint.thumbBorderWidth > 0.0f) {
        thumbDecl.border.color = toClay(paint.thumbBorderColor);
        uint16_t thumbBw = (uint16_t)paint.thumbBorderWidth;
        thumbDecl.border.width = {thumbBw, thumbBw, thumbBw, thumbBw, 0};
      }
      Clay__ConfigureOpenElement(thumbDecl);
      Clay__CloseElement(); // thumb

      Clay__CloseElement(); // thumbWrap
    }
  }

  Clay__CloseElement(); // outer
}

} // namespace n8v::detail
