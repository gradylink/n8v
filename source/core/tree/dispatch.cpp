#include "core/tree/dispatch.hpp"

#include "core/open_url.hpp"
#include "core/tree/props.hpp"
#include "core/ui_core_internal.hpp"

#include <algorithm>
#include <string>

namespace n8v::detail::tree::dispatch {
namespace {

template <typename T> T *optionsOf(PNode &node) { return std::get_if<T>(&node.current.props); }

void refreshBound(PNode &node) { node.bound = readBound(node.current.props); }

} // namespace

void activate(PNode &node) {
  if (auto *button = optionsOf<n8v_button_options>(node)) {
    if (button->checked) {
      *button->checked = !*button->checked;
      const bool checked = *button->checked;
      refreshBound(node);
      if (button->on_change) button->on_change(checked, button->on_change_userdata);
    }
    if (button->on_click) button->on_click(button->on_click_userdata);
  } else if (auto *image = optionsOf<n8v_image_options>(node)) {
    if (image->on_click_at) image->on_click_at(0.5f, 0.5f, image->on_click_at_userdata);
    else if (image->on_click) image->on_click(image->on_click_userdata);
  } else if (auto *text = optionsOf<n8v_text_options>(node)) {
    if (text->on_click) {
      text->on_click(text->on_click_userdata);
    } else if (text->url && *text->url) {
      openUrl(text->url);
    }
  }
}

void activateAt(PNode &node, float x, float y) {
  if (auto *image = optionsOf<n8v_image_options>(node); image && image->on_click_at) {
    image->on_click_at(std::clamp(x, 0.0f, 1.0f), std::clamp(y, 0.0f, 1.0f), image->on_click_at_userdata);
    return;
  }
  activate(node);
}

void zoom(PNode &node, float factor, float x, float y) {
  if (auto *image = optionsOf<n8v_image_options>(node); image && image->on_zoom)
    image->on_zoom(factor, std::clamp(x, 0.0f, 1.0f), std::clamp(y, 0.0f, 1.0f), image->on_zoom_userdata);
}

void drag(PNode &node, float dx, float dy) {
  if (auto *image = optionsOf<n8v_image_options>(node); image && image->on_drag) image->on_drag(dx, dy, image->on_drag_userdata);
}

void dismiss(PNode &node) {
  if (auto *floating = optionsOf<n8v_floating_options>(node); floating && floating->on_dismiss) floating->on_dismiss(floating->on_dismiss_userdata);
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
  if (const auto *action = std::get_if<n8v_page_options>(&page.current.props); action && action->action) {
    if (action->on_click) action->on_click(action->on_click_userdata);
    return;
  }
  int index = 0;
  for (const auto &sibling : page.parent->children) {
    if (sibling.get() == &page) break;
    if (const auto *other = std::get_if<n8v_page_options>(&sibling->current.props); other && !other->action) ++index;
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

bool key(PNode &node, n8v_key key, unsigned mods) {
  auto *o = optionsOf<n8v_entry_options>(node);
  return o && o->on_key && o->on_key(key, mods, o->on_key_userdata);
}

void pasteFiles(PNode &node, const std::vector<std::string> &paths) {
  auto *o = optionsOf<n8v_entry_options>(node);
  if (!o || !o->on_paste_files) return;
  std::vector<const char *> pointers;
  pointers.reserve(paths.size());
  for (const std::string &p : paths) pointers.push_back(p.c_str());
  o->on_paste_files(pointers.data(), pointers.size(), o->on_paste_files_userdata);
}

void hover(PNode &node, bool hovered) {
  auto *o = optionsOf<n8v_flex_options>(node);
  if (o && o->on_hover) o->on_hover(hovered, o->on_hover_userdata);
}

} // namespace n8v::detail::tree::dispatch
