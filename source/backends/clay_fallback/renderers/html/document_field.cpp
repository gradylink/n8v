#include "html_backend_impl.hpp"

#include "core/markdown/markdown_to_html.hpp"
#include "core/native_widget_meta.hpp"

namespace n8v::detail {

void HtmlBackend::renderDocument(NativeWidgetMeta &meta, const Clay_RenderCommand &command) {
  if (!meta.documentAst) return;

  ElementKey key = elementKey(command.id, CLAY_RENDER_COMMAND_TYPE_RECTANGLE);
  bool created = false;
  emscripten::val el = getOrCreateElement(command.id, CLAY_RENDER_COMMAND_TYPE_RECTANGLE, "div", created);
  positionElement(el, elementLastBox_[key], command.boundingBox);

  if (created) {
    el["style"].set("overflow", std::string("auto"));
    el["style"].set("boxSizing", std::string("border-box"));
    el.call<void>("setAttribute", std::string("class"), std::string("n8v-document"));
  }

  const void *&lastSource = elementDocumentSource_[key];
  if (lastSource == meta.documentAst) return;
  lastSource = meta.documentAst;
  el.set("innerHTML", n8v::detail::markdown::toHtml(*meta.documentAst));
}

} // namespace n8v::detail
