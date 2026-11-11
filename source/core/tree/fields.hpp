#pragma once

#include "core/tree/node.hpp"

#include <cstring>
#include <string>
#include <type_traits>

namespace n8v::detail::tree {

template <typename V> void visitFields(n8v_flex_options &o, V &v) {
  v.num(o.direction);
  v.num(o.gap);
  v.num(o.padding);
  v.num(o.h_align);
  v.num(o.v_align);
  v.num(o.width);
  v.num(o.height);
  v.num(o.clip_horizontal);
  v.num(o.clip_vertical);
  v.str(o.id);
  v.num(o.stick_to_bottom);
  v.ptr(o.on_hover);
  v.ptr(o.on_hover_userdata);
}

template <typename V> void visitFields(n8v_panel_options &o, V &v) {
  v.num(o.role);
  v.num(o.direction);
  v.num(o.gap);
  v.num(o.h_align);
  v.num(o.v_align);
  v.num(o.width);
  v.num(o.height);
  v.num(o.clip_horizontal);
  v.num(o.clip_vertical);
  v.str(o.id);
}

template <typename V> void visitFields(n8v_floating_options &o, V &v) {
  v.num(o.width);
  v.num(o.height);
  v.num(o.element_attach_point);
  v.num(o.parent_attach_point);
  v.num(o.offset_x);
  v.num(o.offset_y);
  v.str(o.id);
}

template <typename V> void visitFields(n8v_sidebar_options &o, V &v) {
  v.str(o.title);
  v.ptr(o.selected);
  v.ptr(o.on_change);
  v.ptr(o.on_change_userdata);
  v.num(o.width);
  v.num(o.min_width);
  v.num(o.max_width);
  v.num(o.compact);
}

template <typename V> void visitFields(n8v_page_options &o, V &v) {
  v.str(o.name);
  v.str(o.icon);
  v.str(o.image);
  v.num(o.image_rounding);
  v.num(o.notify);
  v.str(o.badge);
  v.num(o.badge_color);
}

template <typename V> void visitFields(n8v_button_options &o, V &v) {
  v.num(o.style);
  v.ptr(o.on_click);
  v.ptr(o.on_click_userdata);
  v.str(o.icon);
  v.num(o.icon_variant);
  v.num(o.icon_position);
  v.str(o.id);
  v.ptr(o.checked);
  v.ptr(o.on_change);
  v.ptr(o.on_change_userdata);
  v.str(o.image);
  v.num(o.image_height);
}

template <typename V> void visitFields(n8v_text_options &o, V &v) {
  v.num(o.bold);
  v.num(o.italic);
  v.num(o.strikethrough);
  v.str(o.url);
  v.num(o.color);
  v.ptr(o.on_click);
  v.ptr(o.on_click_userdata);
}

template <typename V> void visitFields(n8v_checkbox_options &o, V &v) {
  v.ptr(o.checked);
  v.ptr(o.on_change);
  v.ptr(o.on_change_userdata);
  v.str(o.id);
}

template <typename V> void visitFields(n8v_toggle_options &o, V &v) {
  v.ptr(o.checked);
  v.ptr(o.on_change);
  v.ptr(o.on_change_userdata);
  v.str(o.id);
}

template <typename V> void visitFields(n8v_radio_options &o, V &v) {
  v.ptr(o.selected);
  v.num(o.value);
  v.ptr(o.on_change);
  v.ptr(o.on_change_userdata);
  v.str(o.id);
}

template <typename V> void visitFields(n8v_entry_options &o, V &v) {
  v.ptr(o.value);
  v.str(o.placeholder);
  v.num(o.password);
  v.ptr(o.on_change);
  v.ptr(o.on_change_userdata);
  v.ptr(o.on_submit);
  v.ptr(o.on_submit_userdata);
  v.str(o.id);
  v.num(o.multiline);
  v.num(o.submit_mode);
  v.num(o.min_lines);
  v.num(o.max_lines);
  v.num(o.match_entry_style);
}

template <typename V> void visitFields(n8v_dropdown_options &o, V &v) {
  v.strArray(o.items, o.item_count);
  v.num(o.item_count);
  v.ptr(o.selected);
  v.str(o.placeholder);
  v.ptr(o.on_change);
  v.ptr(o.on_change_userdata);
  v.str(o.id);
}

template <typename V> void visitFields(n8v_slider_options &o, V &v) {
  v.ptr(o.value);
  v.num(o.min);
  v.num(o.max);
  v.ptr(o.on_change);
  v.ptr(o.on_change_userdata);
  v.str(o.id);
}

template <typename V> void visitFields(n8v_image_options &o, V &v) {
  v.num(o.source_kind);
  v.str(o.path);
  v.ptr(o.encoded_data);
  v.num(o.encoded_size);
  v.ptr(o.pixels);
  v.num(o.pixel_width);
  v.num(o.pixel_height);
  v.num(o.width);
  v.num(o.height);
  v.num(o.rounding);
}

template <typename V> void visitFields(n8v_icon_options &o, V &v) {
  v.str(o.name);
  v.num(o.variant);
  v.num(o.width);
  v.num(o.height);
  v.num(o.tint);
}

template <typename V> void visitFields(n8v_document_options &o, V &v) {
  v.str(o.markdown);
  v.num(o.width);
  v.num(o.height);
  v.str(o.id);
  v.ptr(o.link_style);
  v.ptr(o.link_style_userdata);
  v.ptr(o.image_resolver);
  v.ptr(o.image_resolver_userdata);
  v.str(o.image_state);
}

template <typename V> void visitFields(n8v_sidebar_section_options &o, V &v) {
  v.str(o.title);
}

template <typename V> void visitFields(n8v_sidebar_divider_options &o, V &v) {
  v.num(o.reserved);
}

template <typename V> void visitFields(std::monostate &, V &) {}

template <typename V> void visitProps(Node::Props &props, V &v) {
  std::visit([&](auto &options) { visitFields(options, v); }, props);
}

} // namespace n8v::detail::tree
