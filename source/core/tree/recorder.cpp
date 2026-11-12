#include <n8v/n8v_c.h>

#include "core/markdown/image_state.hpp"
#include "core/tree/node.hpp"
#include "core/ui_core_internal.hpp"

#include <cstdio>
#include <deque>
#include <string_view>
#include <vector>

namespace {

using namespace n8v::detail::ui_internal;
using n8v::detail::tree::Node;

std::deque<Node> nodes;
Node root;
std::vector<Node *> openStack;
std::deque<std::vector<const char *>> itemArrays;

struct SidebarFrame {
  const int *selected = nullptr;
  int nextPage = 0;
};
std::vector<SidebarFrame> sidebarFrames;

Node &parent() { return openStack.empty() ? root : *openStack.back(); }

template <typename Options> Node &append(Options options) {
  Node &node = nodes.emplace_back();
  node.props = std::move(options);
  parent().children.push_back(&node);
  return node;
}

const char *own(const char *s) { return s ? internCString(s) : nullptr; }

void ownLabel(Node &node, const char *label) { node.label = label ? std::string_view(internCString(label)) : std::string_view{}; }

template <typename Options> Node &appendContainer(Options options) {
  Node &node = append(std::move(options));
  openStack.push_back(&node);
  return node;
}

void closeContainer() {
  if (!openStack.empty()) openStack.pop_back();
}

n8v_text_options pendingTextOpts;
n8v_button_options pendingButtonOpts;
n8v_checkbox_options pendingCheckboxOpts;
n8v_toggle_options pendingToggleOpts;
n8v_radio_options pendingRadioOpts;

} // namespace

namespace n8v::detail::tree {

void beginRecording() {
  nodes.clear();
  root = Node{};
  openStack.clear();
  itemArrays.clear();
  sidebarFrames.clear();
}

const Node &recordedRoot() { return root; }

} // namespace n8v::detail::tree

extern "C" {

void n8v_open_flex(n8v_flex_options options) {
  options.id = own(options.id);
  appendContainer(options);
}
void n8v_close_flex(void) { closeContainer(); }

void n8v_open_floating(n8v_floating_options options) {
  options.id = own(options.id);
  appendContainer(options);
}
void n8v_close_floating(void) { closeContainer(); }

void n8v_open_panel(n8v_panel_options options) {
  options.id = own(options.id);
  appendContainer(options);
}
void n8v_close_panel(void) { closeContainer(); }

void n8v_open_sidebar(n8v_sidebar_options options) {
  options.title = own(options.title);
  appendContainer(options);
  sidebarFrames.push_back(SidebarFrame{options.selected, 0});
}

void n8v_sidebar_section(n8v_sidebar_section_options options) {
  if (sidebarFrames.empty()) {
    std::fprintf(stderr, "[n8v] sidebarSection() called outside of sidebar()\n");
    return;
  }
  options.title = own(options.title);
  append(options);
}

void n8v_sidebar_divider(void) {
  if (sidebarFrames.empty()) {
    std::fprintf(stderr, "[n8v] sidebarDivider() called outside of sidebar()\n");
    return;
  }
  append(n8v_sidebar_divider_options{});
}

void n8v_close_sidebar(void) {
  if (!sidebarFrames.empty()) sidebarFrames.pop_back();
  closeContainer();
}

bool n8v_open_page(n8v_page_options options) {
  if (sidebarFrames.empty()) {
    std::fprintf(stderr, "[n8v] page() called outside of sidebar()\n");
    return false;
  }
  SidebarFrame &sidebar = sidebarFrames.back();
  const int index = sidebar.nextPage++;
  const bool selected = sidebar.selected ? (*sidebar.selected == index) : (index == 0);

  options.name = own(options.name);
  options.icon = own(options.icon);
  options.image = own(options.image);
  options.badge = own(options.badge);
  Node &node = append(options);

  if (!selected) return false;
  openStack.push_back(&node);
  return true;
}

void n8v_close_page(void) { closeContainer(); }

void _n8v_set_button_opts(n8v_button_options opts) { pendingButtonOpts = opts; }
void _n8v_button_commit(const char *label) {
  n8v_button_options opts = pendingButtonOpts;
  opts.icon = own(opts.icon);
  opts.image = own(opts.image);
  opts.id = own(opts.id);
  ownLabel(append(opts), label);
}

void _n8v_set_text_opts(n8v_text_options opts) { pendingTextOpts = opts; }
void _n8v_text_commit(const char *label) {
  n8v_text_options opts = pendingTextOpts;
  opts.url = own(opts.url);
  ownLabel(append(opts), label);
}

void _n8v_set_checkbox_opts(n8v_checkbox_options opts) { pendingCheckboxOpts = opts; }
void _n8v_checkbox_commit(const char *label) {
  n8v_checkbox_options opts = pendingCheckboxOpts;
  opts.id = own(opts.id);
  ownLabel(append(opts), label);
}

void _n8v_set_toggle_opts(n8v_toggle_options opts) { pendingToggleOpts = opts; }
void _n8v_toggle_commit(const char *label) {
  n8v_toggle_options opts = pendingToggleOpts;
  opts.id = own(opts.id);
  ownLabel(append(opts), label);
}

void _n8v_set_radio_opts(n8v_radio_options opts) { pendingRadioOpts = opts; }
void _n8v_radio_commit(const char *label) {
  n8v_radio_options opts = pendingRadioOpts;
  opts.id = own(opts.id);
  ownLabel(append(opts), label);
}

void n8v_entry(n8v_entry_options options) {
  options.placeholder = own(options.placeholder);
  options.id = own(options.id);
  append(options);
}

void n8v_dropdown(n8v_dropdown_options options) {
  std::vector<const char *> &items = itemArrays.emplace_back();
  items.reserve(options.item_count);
  for (size_t i = 0; i < options.item_count; ++i) items.push_back(own(options.items[i]));
  options.items = items.data();
  options.placeholder = own(options.placeholder);
  options.id = own(options.id);
  append(options);
}

void n8v_slider(n8v_slider_options options) {
  options.id = own(options.id);
  append(options);
}

void n8v_image(n8v_image_options options) {
  options.path = own(options.path);
  append(options);
}

void n8v_divider(n8v_divider_options options) {
  options.id = own(options.id);
  append(options);
}

void n8v_audio(n8v_audio_options options) {
  options.source = own(options.source);
  options.id = own(options.id);
  append(options);
}

void n8v_video(n8v_video_options options) {
  options.source = own(options.source);
  options.id = own(options.id);
  append(options);
}

void n8v_icon(n8v_icon_options options) {
  options.name = own(options.name);
  append(options);
}

void n8v_document(n8v_document_options options) {
  options.markdown = own(options.markdown);
  options.id = own(options.id);
  options.image_state = nullptr;
  if (options.image_resolver) {
    const n8v::detail::markdown::CachedDocument cached = n8v::detail::markdown::getOrParse(options.markdown ? options.markdown : "");
    const std::string state = n8v::detail::markdown::buildImageState(*cached.document, options.image_resolver, options.image_resolver_userdata);
    options.image_state = state.empty() ? nullptr : own(state.c_str());
  }
  append(options);
}

} // extern "C"
