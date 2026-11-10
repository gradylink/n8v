#include "core/tree/dispatch.hpp"

#include "core/open_url.hpp"
#include "core/tree/props.hpp"
#include "core/ui_core_internal.hpp"

#include <string>

namespace n8v::detail::tree::dispatch {
namespace {

template <typename T> T *optionsOf(PNode &node) { return std::get_if<T>(&node.current.props); }

void refreshBound(PNode &node) { node.bound = readBound(node.current.props); }

} // namespace

void activate(PNode &node) {
  if (auto *button = optionsOf<n8v_button_options>(node)) {
    if (button->on_click) button->on_click(button->on_click_userdata);
  } else if (auto *text = optionsOf<n8v_text_options>(node)) {
    if (text->on_click) {
      text->on_click(text->on_click_userdata);
    } else if (text->url && *text->url) {
      openUrl(text->url);
    }
  }
}

void setChecked(PNode &node, bool checked) {
  bool *target = nullptr;
  n8v_bool_change_fn onChange = nullptr;
  void *userdata = nullptr;
  if (auto *o = optionsOf<n8v_checkbox_options>(node)) {
    target = o->checked, onChange = o->on_change, userdata = o->on_change_userdata;
  } else if (auto *o = optionsOf<n8v_toggle_options>(node)) {
    target = o->checked, onChange = o->on_change, userdata = o->on_change_userdata;
  } else {
    return;
  }
  if (!target || *target == checked) return;
  *target = checked;
  refreshBound(node);
  if (onChange) onChange(checked, userdata);
}

void select(PNode &node, int index) {
  int *target = nullptr;
  n8v_int_change_fn onChange = nullptr;
  void *userdata = nullptr;
  if (auto *o = optionsOf<n8v_radio_options>(node)) {
    index = o->value;
    target = o->selected, onChange = o->on_change, userdata = o->on_change_userdata;
  } else if (auto *o = optionsOf<n8v_dropdown_options>(node)) {
    target = o->selected, onChange = o->on_change, userdata = o->on_change_userdata;
  } else if (auto *o = optionsOf<n8v_sidebar_options>(node)) {
    target = o->selected, onChange = o->on_change, userdata = o->on_change_userdata;
  } else {
    return;
  }
  if (!target || *target == index) return;
  *target = index;
  refreshBound(node);
  if (onChange) onChange(index, userdata);
}

void selectPage(PNode &page) {
  if (!std::holds_alternative<n8v_page_options>(page.current.props) || !page.parent) return;
  int index = 0;
  for (const auto &sibling : page.parent->children) {
    if (sibling.get() == &page) break;
    if (std::holds_alternative<n8v_page_options>(sibling->current.props)) ++index;
  }
  select(*page.parent, index);
}

void setSlider(PNode &node, float value) {
  auto *o = optionsOf<n8v_slider_options>(node);
  if (!o || !o->value || *o->value == value) return;
  *o->value = value;
  refreshBound(node);
  if (o->on_change) o->on_change(value, o->on_change_userdata);
}

void setText(PNode &node, std::string_view text) {
  auto *o = optionsOf<n8v_entry_options>(node);
  if (!o) return;
  if (o->value) ui_internal::writeToStringBuf(text, *o->value);
  refreshBound(node);
  if (!o->on_change) return;
  if (o->value) {
    o->on_change(o->value->data, o->value->length, o->on_change_userdata);
  } else {
    std::string copy(text);
    o->on_change(copy.c_str(), copy.size(), o->on_change_userdata);
  }
}

void submit(PNode &node) {
  auto *o = optionsOf<n8v_entry_options>(node);
  if (o && o->on_submit) o->on_submit(o->on_submit_userdata);
}

void hover(PNode &node, bool hovered) {
  auto *o = optionsOf<n8v_flex_options>(node);
  if (o && o->on_hover) o->on_hover(hovered, o->on_hover_userdata);
}

} // namespace n8v::detail::tree::dispatch
