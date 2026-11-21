#include <n8v/n8v_c.h>

#include "core/clay_convert.hpp"
#include "core/image_loader.hpp"
#include "core/native_widget_meta.hpp"
#include "core/pointer_input.hpp"
#include "core/style.hpp"
#include "core/tree/emit.hpp"
#include "core/ui_core_internal.hpp"

#include <clay.h>

#include <algorithm>
#include <cmath>

namespace {

using namespace n8v::detail::ui_internal;

struct ImageGesture {
  n8v::detail::NativeWidgetMeta *meta = nullptr;
  float startX = 0.0f, startY = 0.0f, lastX = 0.0f, lastY = 0.0f;
  bool moved = false;
};
ImageGesture imageGesture;

void dispatchImagePointer(Clay_ElementId elementId, Clay_PointerData pointer, void *userData) {
  auto *meta = static_cast<n8v::detail::NativeWidgetMeta *>(userData);
  if (!meta) return;
  const Clay_ElementData data = Clay_GetElementData(elementId);
  const float width = data.found ? data.boundingBox.width : 0.0f, height = data.found ? data.boundingBox.height : 0.0f;
  const float x = width > 0.0f ? std::clamp((pointer.position.x - data.boundingBox.x) / width, 0.0f, 1.0f) : 0.5f;
  const float y = height > 0.0f ? std::clamp((pointer.position.y - data.boundingBox.y) / height, 0.0f, 1.0f) : 0.5f;

  if (meta->onZoom && n8v::detail::ctrlWheelZoom != 0.0f) {
    const float factor = std::exp(0.1f * n8v::detail::ctrlWheelZoom);
    n8v::detail::ctrlWheelZoom = 0.0f;
    meta->onZoom(factor, x, y, meta->onZoomUserdata);
  }

  switch (pointer.state) {
  case CLAY_POINTER_DATA_PRESSED_THIS_FRAME:
    imageGesture = ImageGesture{meta, pointer.position.x, pointer.position.y, pointer.position.x, pointer.position.y, false};
    break;
  case CLAY_POINTER_DATA_PRESSED:
    if (imageGesture.meta != meta || !meta->onDrag) break;
    if (!imageGesture.moved && std::abs(pointer.position.x - imageGesture.startX) + std::abs(pointer.position.y - imageGesture.startY) < 4.0f) break;
    imageGesture.moved = true;
    meta->onDrag(pointer.position.x - imageGesture.lastX, pointer.position.y - imageGesture.lastY, meta->onDragUserdata);
    imageGesture.lastX = pointer.position.x;
    imageGesture.lastY = pointer.position.y;
    break;
  case CLAY_POINTER_DATA_RELEASED_THIS_FRAME:
    if (imageGesture.meta == meta && !imageGesture.moved) {
      if (meta->onClickAt) meta->onClickAt(x, y, meta->onClickAtUserdata);
      else if (meta->onClick) meta->onClick(meta->onClickUserdata);
    }
    if (imageGesture.meta == meta) imageGesture = ImageGesture{};
    break;
  case CLAY_POINTER_DATA_RELEASED:
    break;
  }
}

} // namespace

namespace n8v::detail::ui_internal {

void resetImageFrameState() {}

} // namespace n8v::detail::ui_internal

namespace n8v::detail::emit {

void image(const n8v_image_options &options, int ordinal) {
  const n8v::detail::DecodedImage *decoded = n8v::detail::getOrDecodeImage(options);
  if (!decoded) return;

  Clay__OpenElement();

  n8v::Rounding rounding = toRounding(options.rounding);
  n8v::CornerRadius effectiveRadius;
  switch (rounding.mode) {
  case n8v::RoundingMode::StyleDefault:
    effectiveRadius = n8v::activePaint().image().cornerRadius;
    break;
  case n8v::RoundingMode::None:
    effectiveRadius = {};
    break;
  case n8v::RoundingMode::Fixed:
    effectiveRadius = rounding.radius;
    break;
  }

  Clay_ElementDeclaration decl = {};
  decl.image.imageData = const_cast<n8v::detail::DecodedImage *>(decoded);
  decl.cornerRadius = n8v::detail::toClay(effectiveRadius);

  n8v::Sizing width = toSizing(options.width);
  n8v::Sizing height = toSizing(options.height);
  decl.layout.sizing.width =
    (width.mode == n8v::SizingMode::Fit && width.min <= 0.0f && width.max <= 0.0f) ? CLAY_SIZING_FIXED((float)decoded->width) : n8v::detail::toClay(width);
  decl.layout.sizing.height =
    (height.mode == n8v::SizingMode::Fit && height.min <= 0.0f && height.max <= 0.0f) ? CLAY_SIZING_FIXED((float)decoded->height) : n8v::detail::toClay(height);

  widgetMetaStorage.push_back(n8v::detail::NativeWidgetMeta{});
  n8v::detail::NativeWidgetMeta &meta = widgetMetaStorage.back();
  meta.kind = n8v::NativeWidgetKind::Image;
  meta.ordinal = ordinal;
  meta.image = decoded;
  meta.onClick = options.on_click;
  meta.onClickUserdata = options.on_click_userdata;
  meta.onClickAt = options.on_click_at;
  meta.onClickAtUserdata = options.on_click_at_userdata;
  meta.onZoom = options.on_zoom;
  meta.onZoomUserdata = options.on_zoom_userdata;
  meta.onDrag = options.on_drag;
  meta.onDragUserdata = options.on_drag_userdata;
  decl.userData = &meta;

  Clay__ConfigureOpenElement(decl);
  if (options.on_click || options.on_click_at || options.on_zoom || options.on_drag) {
    Clay_OnHover(dispatchImagePointer, &meta);
    if (Clay_Hovered() && (options.on_click || options.on_click_at || options.on_drag)) pendingCursor = n8v::CursorKind::Pointer;
  }
  Clay__CloseElement();
}

} // namespace n8v::detail::emit

extern "C" {

void n8v_set_image_bundle_lookup(n8v_image_bundle_lookup_fn fn, void *userdata) { n8v::detail::setImageBundleLookup(fn, userdata); }

} // extern "C"
