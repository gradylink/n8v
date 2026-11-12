#pragma once

#include <n8v/n8v_c.h>

#include <string_view>
#include <variant>
#include <vector>

namespace n8v::detail::tree {

struct Node {
  using Props = std::variant<
    std::monostate,
    n8v_flex_options,
    n8v_panel_options,
    n8v_floating_options,
    n8v_sidebar_options,
    n8v_page_options,
    n8v_button_options,
    n8v_text_options,
    n8v_checkbox_options,
    n8v_toggle_options,
    n8v_radio_options,
    n8v_entry_options,
    n8v_dropdown_options,
    n8v_slider_options,
    n8v_image_options,
    n8v_icon_options,
    n8v_document_options,
    n8v_sidebar_section_options,
    n8v_sidebar_divider_options,
    n8v_video_options,
    n8v_audio_options>;

  Props props;
  std::string_view label;
  std::vector<Node *> children;
};

void beginRecording();

const Node &recordedRoot();

} // namespace n8v::detail::tree
