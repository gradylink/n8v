#include <n8v/n8v_c.h>

#include "core/backend.hpp"
#include "core/style.hpp"

#include "core/clay_convert.hpp"
#include "core/native_widget_meta.hpp"
#include "core/ui_core_internal.hpp"

#include <clay.h>

namespace {

using namespace n8v::detail::ui_internal;

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
  if (meta->onSliderChange) meta->onSliderChange(newValue, meta->onSliderChangeUserdata);
}

} // namespace

namespace n8v::detail::ui_internal {

void resetSliderFrameState() {}

} // namespace n8v::detail::ui_internal

extern "C" {

void n8v_slider(n8v_slider_options options) {
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
          if (options.on_change) options.on_change(newValue, options.on_change_userdata);
        }
      }
    } else {
      draggingSliderOrdinal = -1;
    }
  }

  Clay__OpenElementWithId(sliderTrackId(ordinal));

  const bool hovered = Clay_Hovered();
  const bool pressed = hovered && n8v::activeBackend().pointerDown();
  const n8v::SliderPaint paint = n8v::activePaint().slider(hovered, pressed);

  Clay_Dimensions nativeSize = n8v::activeBackend().measureNativeChrome(n8v::NativeWidgetKind::Slider, {}, 0);
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

  widgetMetaStorage.push_back(n8v::detail::NativeWidgetMeta{});
  n8v::detail::NativeWidgetMeta &meta = widgetMetaStorage.back();
  meta.kind = n8v::NativeWidgetKind::Slider;
  meta.ordinal = ordinal;
  meta.sliderValue = options.value;
  meta.sliderMin = options.min;
  meta.sliderMax = options.max;
  meta.onSliderChange = options.on_change;
  meta.onSliderChangeUserdata = options.on_change_userdata;
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
      fillDecl.backgroundColor = n8v::detail::toClay(paint.fillColor);
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
      thumbDecl.backgroundColor = n8v::detail::toClay(paint.thumbColor);
      thumbDecl.cornerRadius = {thumbRadius, thumbRadius, thumbRadius, thumbRadius};
      if (paint.thumbBorderWidth > 0.0f) {
        thumbDecl.border.color = n8v::detail::toClay(paint.thumbBorderColor);
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
      trackDecl.backgroundColor = n8v::detail::toClay(paint.trackColor);
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
      fillDecl.backgroundColor = n8v::detail::toClay(paint.fillColor);
      fillDecl.cornerRadius = {trackRadius, trackRadius, trackRadius, trackRadius};
      Clay__ConfigureOpenElement(fillDecl);
      Clay__CloseElement();

      Clay__OpenElement();
      Clay_ElementDeclaration trackDecl = {};
      trackDecl.layout.sizing.width = CLAY_SIZING_GROW(0);
      trackDecl.layout.sizing.height = CLAY_SIZING_GROW(0);
      trackDecl.backgroundColor = n8v::detail::toClay(paint.trackColor);
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
      thumbDecl.backgroundColor = n8v::detail::toClay(paint.thumbColor);
      thumbDecl.cornerRadius = {thumbRadius, thumbRadius, thumbRadius, thumbRadius};
      if (paint.thumbBorderWidth > 0.0f) {
        thumbDecl.border.color = n8v::detail::toClay(paint.thumbBorderColor);
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

} // extern "C"
