#include <n8v/n8v_c.h>

#include "core/clay_convert.hpp"
#include "core/icon_loader.hpp"
#include "core/native_widget_meta.hpp"
#include "core/style.hpp"
#include "core/ui_core_internal.hpp"

#include <clay.h>

namespace {

using namespace n8v::detail::ui_internal;

} // namespace

extern "C" {

void n8v_icon(n8v_icon_options options) {
  if (!options.name) return;

  const n8v::IconPaint iconPaint = n8v::activePaint().icon();
  bool explicitTint = options.tint.a > 0.0f;
  n8v::Color tint = explicitTint ? toColor(options.tint) : iconPaint.tint;

  n8v::Sizing width = toSizing(options.width);
  n8v::Sizing height = toSizing(options.height);
  bool widthIsPlainFit = width.mode == n8v::SizingMode::Fit && width.min <= 0.0f && width.max <= 0.0f;
  bool heightIsPlainFit = height.mode == n8v::SizingMode::Fit && height.min <= 0.0f && height.max <= 0.0f;
  float requestedSize = !widthIsPlainFit ? width.value : (!heightIsPlainFit ? height.value : iconPaint.defaultSize);
  uint16_t pixelSize = (uint16_t)(requestedSize > 0.0f ? requestedSize : iconPaint.defaultSize);

  const n8v::detail::DecodedImage *decoded = n8v::detail::getOrDecodeIcon(options.name, pixelSize, tint);
  if (!decoded) return;

  Clay__OpenElement();

  const int ordinal = widgetOrdinal++;

  Clay_ElementDeclaration decl = {};
  decl.image.imageData = const_cast<n8v::detail::DecodedImage *>(decoded);
  decl.layout.sizing.width = widthIsPlainFit ? CLAY_SIZING_FIXED((float)pixelSize) : n8v::detail::toClay(width);
  decl.layout.sizing.height = heightIsPlainFit ? CLAY_SIZING_FIXED((float)pixelSize) : n8v::detail::toClay(height);

  widgetMetaStorage.push_back(n8v::detail::NativeWidgetMeta{});
  n8v::detail::NativeWidgetMeta &meta = widgetMetaStorage.back();
  meta.kind = n8v::NativeWidgetKind::Icon;
  meta.ordinal = ordinal;
  meta.image = decoded;
  meta.iconName = internCString(options.name);
  meta.iconVariant = toIconVariant(options.variant);
  meta.iconTint = tint;
  decl.userData = &meta;

  Clay__ConfigureOpenElement(decl);
  Clay__CloseElement();
}

} // extern "C"
