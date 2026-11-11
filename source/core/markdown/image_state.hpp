#pragma once

#include "markdown.hpp"

#include <n8v/n8v_c.h>

#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace n8v::detail::markdown {

inline void forEachInlineRun(const std::vector<Block> &blocks, const std::function<void(const InlineRun &)> &fn) {
  for (const Block &block : blocks) {
    for (const InlineRun &run : block.inlines) fn(run);
    for (const std::vector<InlineRun> &item : block.listItems) {
      for (const InlineRun &run : item) fn(run);
    }
    forEachInlineRun(block.children, fn);
  }
}

inline std::string buildImageState(const Document &document, n8v_image_resolve_fn resolve, void *userdata) {
  std::string state;
  if (!resolve) return state;
  forEachInlineRun(document, [&](const InlineRun &run) {
    if (run.imageUrl.empty()) return;
    const char *path = resolve(run.imageUrl.c_str(), userdata);
    if (path && *path) state += run.imageUrl + "\t" + path + "\n";
  });
  return state;
}

inline std::string lookupImage(const char *state, const std::string &src) {
  if (!state || !*state) return {};
  std::string_view all(state);
  size_t pos = 0;
  while (pos < all.size()) {
    size_t end = all.find('\n', pos);
    if (end == std::string_view::npos) end = all.size();
    std::string_view line = all.substr(pos, end - pos);
    size_t tab = line.find('\t');
    if (tab != std::string_view::npos && line.substr(0, tab) == src) return std::string(line.substr(tab + 1));
    pos = end + 1;
  }
  return {};
}

} // namespace n8v::detail::markdown
