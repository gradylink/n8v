#pragma once

#include "core/markdown/markdown.hpp"

#include <gtk/gtk.h>

#include <cstdio>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace n8v::detail::gtk_markup {

namespace markdown = n8v::detail::markdown;

inline std::string linkMarkup(std::string_view text, std::string_view url) {
  gchar *escapedText = g_markup_escape_text(std::string(text).c_str(), -1);
  gchar *escapedUrl = g_markup_escape_text(std::string(url).c_str(), -1);
  std::string result = std::string("<a href=\"") + escapedUrl + "\">" + escapedText + "</a>";
  g_free(escapedText);
  g_free(escapedUrl);
  return result;
}

inline std::string escapeMarkup(std::string_view text) {
  gchar *escaped = g_markup_escape_text(std::string(text).c_str(), -1);
  std::string result = escaped;
  g_free(escaped);
  return result;
}

using LinkStyler = std::function<bool(const std::string &url, n8v_link_style &style)>;

inline std::string colorAttributes(const char *name, const n8v_color &c, const char *alphaName) {
  char buf[96];
  std::snprintf(buf, sizeof buf, " %s=\"#%02x%02x%02x\" %s=\"%d%%\"", name, (int)c.r & 255, (int)c.g & 255, (int)c.b & 255, alphaName, (int)(c.a / 255.0f * 100.0f + 0.5f));
  return buf;
}

inline void appendInlineRunsMarkup(std::string &out, const std::vector<markdown::InlineRun> &runs, const LinkStyler *styler = nullptr) {
  for (const markdown::InlineRun &run : runs) {
    bool link = !run.linkUrl.empty();
    n8v_link_style style{};
    const bool styled = link && styler && *styler && (*styler)(run.linkUrl, style);
    if (styled) {
      out += "<span";
      if (style.foreground.a > 0.0f) out += colorAttributes("foreground", style.foreground, "fgalpha");
      if (style.background.a > 0.0f) out += colorAttributes("background", style.background, "bgalpha");
      if (style.bold) out += " weight=\"bold\"";
      out += ">";
      if (style.background.a > 0.0f) out += "\u2009";
      link = false;
    } else if (link) {
      out += "<a href=\"" + escapeMarkup(run.linkUrl) + "\">";
    }
    if (run.bold) out += "<b>";
    if (run.italic) out += "<i>";
    if (run.strikethrough) out += "<s>";
    if (run.code) out += "<span font_family=\"monospace\" background=\"#00000014\">";
    out += escapeMarkup(run.text);
    if (run.code) out += "</span>";
    if (run.strikethrough) out += "</s>";
    if (run.italic) out += "</i>";
    if (run.bold) out += "</b>";
    if (link) out += "</a>";
    if (styled) {
      if (style.background.a > 0.0f) out += "\u2009";
      out += "</span>";
    }
  }
}

inline void appendBlocksMarkup(std::string &out, const std::vector<markdown::Block> &blocks, const LinkStyler *styler = nullptr) {
  using markdown::BlockKind;
  for (size_t i = 0; i < blocks.size(); i++) {
    const markdown::Block &block = blocks[i];
    if (i > 0) out += "\n\n";
    switch (block.kind) {
    case BlockKind::Heading: {
      const char *size = block.level <= 1 ? "xx-large" : block.level == 2 ? "x-large" : block.level == 3 ? "large" : "medium";
      out += "<span size=\"" + std::string(size) + "\" weight=\"bold\">";
      appendInlineRunsMarkup(out, block.inlines, styler);
      out += "</span>";
      break;
    }
    case BlockKind::Paragraph: {
      appendInlineRunsMarkup(out, block.inlines, styler);
      break;
    }
    case BlockKind::CodeBlock: {
      out += "<span font_family=\"monospace\" background=\"#00000014\">" + escapeMarkup(block.codeText) + "</span>";
      break;
    }
    case BlockKind::BulletList: {
      for (size_t j = 0; j < block.listItems.size(); j++) {
        if (j > 0) out += "\n";
        out += "• ";
        appendInlineRunsMarkup(out, block.listItems[j], styler);
      }
      break;
    }
    case BlockKind::OrderedList: {
      for (size_t j = 0; j < block.listItems.size(); j++) {
        if (j > 0) out += "\n";
        out += std::to_string(block.orderedStart + (int)j) + ". ";
        appendInlineRunsMarkup(out, block.listItems[j], styler);
      }
      break;
    }
    case BlockKind::BlockQuote: {
      out += "<span alpha=\"75%\"><i>";
      appendBlocksMarkup(out, block.children, styler);
      out += "</i></span>";
      break;
    }
    case BlockKind::ThematicBreak: {
      out += "――――――――";
      break;
    }
    }
  }
}

inline std::string documentToPangoMarkup(const markdown::Document &doc, const LinkStyler *styler = nullptr) {
  std::string out;
  appendBlocksMarkup(out, doc, styler);
  return out;
}

inline bool documentHasLinks(const std::vector<markdown::Block> &blocks) {
  using markdown::BlockKind;
  auto runsHaveLink = [](const std::vector<markdown::InlineRun> &runs) {
    for (const markdown::InlineRun &run : runs)
      if (!run.linkUrl.empty()) return true;
    return false;
  };
  for (const markdown::Block &block : blocks) {
    switch (block.kind) {
    case BlockKind::Heading:
    case BlockKind::Paragraph:
      if (runsHaveLink(block.inlines)) return true;
      break;
    case BlockKind::BulletList:
    case BlockKind::OrderedList:
      for (const auto &item : block.listItems)
        if (runsHaveLink(item)) return true;
      break;
    case BlockKind::BlockQuote:
      if (documentHasLinks(block.children)) return true;
      break;
    case BlockKind::CodeBlock:
    case BlockKind::ThematicBreak:
      break;
    }
  }
  return false;
}

inline void setPlainLabelText(GtkWidget *label, std::string_view text, bool bold, bool italic, bool strikethrough = false) {
  if (!bold && !italic && !strikethrough) {
    gtk_label_set_text(GTK_LABEL(label), std::string(text).c_str());
    return;
  }
  gchar *escaped = g_markup_escape_text(std::string(text).c_str(), -1);
  std::string markup = escaped;
  g_free(escaped);
  if (strikethrough) markup = "<s>" + markup + "</s>";
  if (italic) markup = "<i>" + markup + "</i>";
  if (bold) markup = "<b>" + markup + "</b>";
  gtk_label_set_markup(GTK_LABEL(label), markup.c_str());
}

} // namespace n8v::detail::gtk_markup
