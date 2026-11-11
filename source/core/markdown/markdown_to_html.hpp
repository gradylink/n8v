#pragma once

#include "markdown.hpp"

#include <n8v/n8v_c.h>

#include <functional>
#include <string>

namespace n8v::detail::markdown {

using LinkStyler = std::function<bool(const std::string &url, n8v_link_style &out)>;

enum class HtmlFlavor {
  Browser,
  QtRichText,
  Basic,
};

struct HtmlOptions {
  LinkStyler linkStyler;
  HtmlFlavor flavor = HtmlFlavor::Browser;
  n8v_color backdrop = {0, 0, 0, 255};
  std::function<std::string(const std::string &src)> imageResolver;
  int defaultImageHeight = 20;
};

inline LinkStyler makeLinkStyler(n8v_link_style_fn fn, void *userdata) {
  if (!fn) return {};
  return [fn, userdata](const std::string &url, n8v_link_style &out) { return fn(url.c_str(), &out, userdata); };
}

std::string toHtml(const Document &document, const HtmlOptions &options = {});

} // namespace n8v::detail::markdown
