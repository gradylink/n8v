#include <n8v/n8v_c.h>

#include "core/clay_convert.hpp"
#include "core/image_loader.hpp"
#include "core/native_widget_meta.hpp"
#include "core/style.hpp"
#include "core/ui_core_internal.hpp"

#include <clay.h>

namespace {

using namespace n8v::detail::ui_internal;

} // namespace

namespace n8v::detail::ui_internal {

void resetImageFrameState() {}

} // namespace n8v::detail::ui_internal

extern "C" {

void n8v_image(n8v_image_options options) {
  const n8v::detail::DecodedImage *decoded = n8v::detail::getOrDecodeImage(options);
  if (!decoded) return;

  Clay__OpenElement();

  const int ordinal = widgetOrdinal++;

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
  decl.userData = &meta;

  Clay__ConfigureOpenElement(decl);
  Clay__CloseElement();
}

void n8v_set_image_bundle_lookup(n8v_image_bundle_lookup_fn fn, void *userdata) { n8v::detail::setImageBundleLookup(fn, userdata); }

} // extern "C"
