#include "markdown_to_html.hpp"

#include <cstdio>

namespace n8v::detail::markdown {

namespace {

void escapeInto(std::string &out, std::string_view text) {
  for (char c : text) {
    switch (c) {
    case '&':
      out += "&amp;";
      break;
    case '<':
      out += "&lt;";
      break;
    case '>':
      out += "&gt;";
      break;
    default:
      out += c;
    }
  }
}

void escapeAttrInto(std::string &out, std::string_view text) {
  for (char c : text) {
    switch (c) {
    case '&':
      out += "&amp;";
      break;
    case '"':
      out += "&quot;";
      break;
    case '<':
      out += "&lt;";
      break;
    case '>':
      out += "&gt;";
      break;
    default:
      out += c;
    }
  }
}

int channel(float value) { return value < 0.0f ? 0 : value > 255.0f ? 255 : (int)(value + 0.5f); }

std::string hex(float r, float g, float b) {
  char buffer[16];
  std::snprintf(buffer, sizeof(buffer), "#%02X%02X%02X", channel(r), channel(g), channel(b));
  return buffer;
}

n8v_color flatten(const n8v_color &top, const n8v_color &under) {
  const float a = top.a / 255.0f;
  return {top.r * a + under.r * (1.0f - a), top.g * a + under.g * (1.0f - a), top.b * a + under.b * (1.0f - a), 255.0f};
}

std::string openStyledLink(std::string &out, const n8v_link_style &style, const HtmlOptions &options) {
  const bool hasForeground = style.foreground.a > 0.0f;
  const bool hasBackground = style.background.a > 0.0f;
  char buffer[160];
  switch (options.flavor) {
  case HtmlFlavor::Browser: {
    out += "<span style=\"";
    if (hasForeground) {
      std::snprintf(
        buffer,
        sizeof(buffer),
        "color:rgba(%d,%d,%d,%.2f);",
        channel(style.foreground.r),
        channel(style.foreground.g),
        channel(style.foreground.b),
        style.foreground.a / 255.0f
      );
      out += buffer;
    }
    if (hasBackground) {
      std::snprintf(
        buffer,
        sizeof(buffer),
        "background-color:rgba(%d,%d,%d,%.2f);padding:0 .25em;border-radius:.3em;",
        channel(style.background.r),
        channel(style.background.g),
        channel(style.background.b),
        style.background.a / 255.0f
      );
      out += buffer;
    }
    if (style.bold) out += "font-weight:bold;";
    out += "\">";
    return "</span>";
  }
  case HtmlFlavor::QtRichText: {
    out += "<span style=\"";
    if (hasForeground) out += "color:" + hex(style.foreground.r, style.foreground.g, style.foreground.b) + ";";
    if (hasBackground) {
      const n8v_color flat = flatten(style.background, options.backdrop);
      out += "background-color:" + hex(flat.r, flat.g, flat.b) + ";";
    }
    if (style.bold) out += "font-weight:bold;";
    out += "\">";
    if (hasBackground) out += "\u2009";
    return hasBackground ? "\u2009</span>" : "</span>";
  }
  case HtmlFlavor::Basic: {
    const n8v_color text = hasForeground ? style.foreground : style.background;
    std::string closing;
    if (hasForeground || hasBackground) {
      out += "<font color=\"" + hex(text.r, text.g, text.b) + "\">";
      closing = "</font>";
    }
    if (style.bold) {
      out += "<b>";
      closing = "</b>" + closing;
    }
    return closing;
  }
  }
  return {};
}

void appendInlines(std::string &out, const std::vector<InlineRun> &runs, const HtmlOptions &options) {
  for (const InlineRun &run : runs) {
    bool link = !run.linkUrl.empty();
    n8v_link_style style{};
    if (link && options.linkStyler && options.linkStyler(run.linkUrl, style)) {
      const std::string closing = openStyledLink(out, style, options);
      if (run.bold) out += "<b>";
      if (run.italic) out += "<i>";
      escapeInto(out, run.text);
      if (run.italic) out += "</i>";
      if (run.bold) out += "</b>";
      out += closing;
      continue;
    }
    if (link) {
      out += "<a href=\"";
      escapeAttrInto(out, run.linkUrl);
      out += "\">";
    }
    if (run.bold) out += "<b>";
    if (run.italic) out += "<i>";
    if (run.strikethrough) out += "<s>";
    if (run.code) out += "<code>";
    escapeInto(out, run.text);
    if (run.code) out += "</code>";
    if (run.strikethrough) out += "</s>";
    if (run.italic) out += "</i>";
    if (run.bold) out += "</b>";
    if (link) out += "</a>";
  }
}

void appendBlocks(std::string &out, const std::vector<Block> &blocks, const HtmlOptions &options) {
  for (const Block &block : blocks) {
    switch (block.kind) {
    case BlockKind::Heading: {
      int level = block.level >= 1 && block.level <= 6 ? block.level : 1;
      out += "<h" + std::to_string(level) + ">";
      appendInlines(out, block.inlines, options);
      out += "</h" + std::to_string(level) + ">";
      break;
    }
    case BlockKind::Paragraph: {
      out += "<p>";
      appendInlines(out, block.inlines, options);
      out += "</p>";
      break;
    }
    case BlockKind::CodeBlock: {
      out += "<pre><code>";
      escapeInto(out, block.codeText);
      out += "</code></pre>";
      break;
    }
    case BlockKind::BulletList: {
      out += "<ul>";
      for (const auto &item : block.listItems) {
        out += "<li>";
        appendInlines(out, item, options);
        out += "</li>";
      }
      out += "</ul>";
      break;
    }
    case BlockKind::OrderedList: {
      out += "<ol start=\"" + std::to_string(block.orderedStart) + "\">";
      for (const auto &item : block.listItems) {
        out += "<li>";
        appendInlines(out, item, options);
        out += "</li>";
      }
      out += "</ol>";
      break;
    }
    case BlockKind::BlockQuote: {
      out += "<blockquote>";
      appendBlocks(out, block.children, options);
      out += "</blockquote>";
      break;
    }
    case BlockKind::ThematicBreak: {
      out += "<hr>";
      break;
    }
    }
  }
}

} // namespace

std::string toHtml(const Document &document, const HtmlOptions &options) {
  std::string out;
  appendBlocks(out, document, options);
  return out;
}

} // namespace n8v::detail::markdown
