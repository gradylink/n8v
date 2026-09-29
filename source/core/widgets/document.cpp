#include <n8v/n8v_c.h>

#include "core/backend.hpp"
#include "core/clay_convert.hpp"
#include "core/markdown/markdown.hpp"
#include "core/native_widget_meta.hpp"
#include "core/style.hpp"
#include "core/ui_core_internal.hpp"

#include <clay.h>

namespace {

using namespace n8v::detail::ui_internal;

} // namespace

extern "C" {

void n8v_document(n8v_document_options options) {
  openElementMaybeWithId(options.id);

  const int ordinal = widgetOrdinal++;
  const n8v::detail::markdown::CachedDocument cached = n8v::detail::markdown::getOrParse(toView(options.markdown));

  Clay_ElementDeclaration decl = {};
  decl.layout.sizing.width = n8v::detail::toClay(toSizing(options.width));
  decl.layout.sizing.height = n8v::detail::toClay(toSizing(options.height));
  decl.backgroundColor = {0, 0, 0, 1.0f / 255.0f};

  widgetMetaStorage.push_back(n8v::detail::NativeWidgetMeta{});
  n8v::detail::NativeWidgetMeta &meta = widgetMetaStorage.back();
  meta.kind = n8v::NativeWidgetKind::Document;
  meta.ordinal = ordinal;
  if (options.id) meta.id = options.id;
  meta.documentSource = cached.source;
  meta.documentAst = cached.document;
  const n8v::TextPaint basePaint = n8v::activePaint().text(n8v::TextOptions{});
  const n8v::TextPaint linkPaint = n8v::activePaint().text(n8v::TextOptions{.url = "#"});
  meta.documentFont = basePaint.font;
  meta.documentFontSize = basePaint.fontSize;
  meta.documentTextColor = basePaint.color;
  meta.documentLinkColor = linkPaint.color;
  decl.userData = &meta;

  Clay__ConfigureOpenElement(decl);
  Clay__CloseElement();
}

} // extern "C"
