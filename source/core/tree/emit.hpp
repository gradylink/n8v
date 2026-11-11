#pragma once

#include <n8v/n8v_c.h>

#include <string_view>

namespace n8v::detail::emit {

void openFlex(const n8v_flex_options &options);
void closeFlex();
void openFloating(const n8v_floating_options &options);
void closeFloating();
void openPanel(const n8v_panel_options &options, int ordinal);
void closePanel();
void openSidebar(const n8v_sidebar_options &options, int ordinal);
void closeSidebar();
bool openPage(const n8v_page_options &options, int ordinal);
void sidebarSection(const n8v_sidebar_section_options &options, int ordinal);
void sidebarDivider(int ordinal);
void closePage();

void button(const n8v_button_options &options, std::string_view label, int ordinal);
void text(const n8v_text_options &options, std::string_view label, int ordinal);
void checkbox(const n8v_checkbox_options &options, std::string_view label, int ordinal);
void toggle(const n8v_toggle_options &options, std::string_view label, int ordinal);
void radio(const n8v_radio_options &options, std::string_view label, int ordinal);
void entry(const n8v_entry_options &options, int ordinal);
void dropdown(const n8v_dropdown_options &options, int ordinal);
void slider(const n8v_slider_options &options, int ordinal);
void image(const n8v_image_options &options, int ordinal);
void icon(const n8v_icon_options &options, int ordinal);
void document(const n8v_document_options &options, int ordinal);

void releaseNode(int ordinal);

} // namespace n8v::detail::emit
