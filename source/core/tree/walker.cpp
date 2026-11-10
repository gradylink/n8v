#include "core/tree/emit.hpp"
#include "core/tree/node.hpp"
#include "core/tree/persistent.hpp"
#include "core/tree/reconcile.hpp"

namespace n8v::detail::tree {
namespace {

template <typename... Ts> struct Overloaded : Ts... {
  using Ts::operator()...;
};
template <typename... Ts> Overloaded(Ts...) -> Overloaded<Ts...>;

void emitNode(const PNode &node);

void emitChildren(const PNode &node) {
  for (const auto &child : node.children) emitNode(*child);
}

void emitNode(const PNode &node) {
  const int ordinal = node.ordinal;
  const std::string_view label = node.current.label;
  std::visit(Overloaded{
               [&](const std::monostate &) { emitChildren(node); },
               [&](const n8v_flex_options &o) {
                 emit::openFlex(o);
                 emitChildren(node);
                 emit::closeFlex();
               },
               [&](const n8v_panel_options &o) {
                 emit::openPanel(o, ordinal);
                 emitChildren(node);
                 emit::closePanel();
               },
               [&](const n8v_floating_options &o) {
                 emit::openFloating(o);
                 emitChildren(node);
                 emit::closeFloating();
               },
               [&](const n8v_sidebar_options &o) {
                 emit::openSidebar(o, ordinal);
                 emitChildren(node);
                 emit::closeSidebar();
               },
               [&](const n8v_page_options &o) {
                 if (emit::openPage(o, ordinal)) {
                   emitChildren(node);
                   emit::closePage();
                 }
               },
               [&](const n8v_button_options &o) { emit::button(o, label, ordinal); },
               [&](const n8v_text_options &o) { emit::text(o, label, ordinal); },
               [&](const n8v_checkbox_options &o) { emit::checkbox(o, label, ordinal); },
               [&](const n8v_toggle_options &o) { emit::toggle(o, label, ordinal); },
               [&](const n8v_radio_options &o) { emit::radio(o, label, ordinal); },
               [&](const n8v_entry_options &o) { emit::entry(o, ordinal); },
               [&](const n8v_dropdown_options &o) { emit::dropdown(o, ordinal); },
               [&](const n8v_slider_options &o) { emit::slider(o, ordinal); },
               [&](const n8v_image_options &o) { emit::image(o, ordinal); },
               [&](const n8v_icon_options &o) { emit::icon(o, ordinal); },
               [&](const n8v_document_options &o) { emit::document(o, ordinal); },
             },
             node.current.props);
}

} // namespace

void emitTree() { emitNode(reconciler().root()); }

} // namespace n8v::detail::tree
