#pragma once

#include "core/markdown/markdown.hpp"

#include <gtk/gtk.h>

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

inline void appendInlineRunsMarkup(std::string &out, const std::vector<markdown::InlineRun> &runs) {
  for (const markdown::InlineRun &run : runs) {
    bool link = !run.linkUrl.empty();
    if (link) {
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
  }
}

inline void appendBlocksMarkup(std::string &out, const std::vector<markdown::Block> &blocks) {
  using markdown::BlockKind;
  for (size_t i = 0; i < blocks.size(); i++) {
    const markdown::Block &block = blocks[i];
    if (i > 0) out += "\n\n";
    switch (block.kind) {
    case BlockKind::Heading: {
      const char *size = block.level <= 1 ? "xx-large" : block.level == 2 ? "x-large" : block.level == 3 ? "large" : "medium";
      out += "<span size=\"" + std::string(size) + "\" weight=\"bold\">";
      appendInlineRunsMarkup(out, block.inlines);
      out += "</span>";
      break;
    }
    case BlockKind::Paragraph: {
      appendInlineRunsMarkup(out, block.inlines);
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
        appendInlineRunsMarkup(out, block.listItems[j]);
      }
      break;
    }
    case BlockKind::OrderedList: {
      for (size_t j = 0; j < block.listItems.size(); j++) {
        if (j > 0) out += "\n";
        out += std::to_string(block.orderedStart + (int)j) + ". ";
        appendInlineRunsMarkup(out, block.listItems[j]);
      }
      break;
    }
    case BlockKind::BlockQuote: {
      out += "<span alpha=\"75%\"><i>";
      appendBlocksMarkup(out, block.children);
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

inline std::string documentToPangoMarkup(const markdown::Document &doc) {
  std::string out;
  appendBlocksMarkup(out, doc);
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
