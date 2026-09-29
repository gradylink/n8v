#include "markdown_to_html.hpp"

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

void appendInlines(std::string &out, const std::vector<InlineRun> &runs) {
  for (const InlineRun &run : runs) {
    bool link = !run.linkUrl.empty();
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

void appendBlocks(std::string &out, const std::vector<Block> &blocks) {
  for (const Block &block : blocks) {
    switch (block.kind) {
    case BlockKind::Heading: {
      int level = block.level >= 1 && block.level <= 6 ? block.level : 1;
      out += "<h" + std::to_string(level) + ">";
      appendInlines(out, block.inlines);
      out += "</h" + std::to_string(level) + ">";
      break;
    }
    case BlockKind::Paragraph: {
      out += "<p>";
      appendInlines(out, block.inlines);
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
        appendInlines(out, item);
        out += "</li>";
      }
      out += "</ul>";
      break;
    }
    case BlockKind::OrderedList: {
      out += "<ol start=\"" + std::to_string(block.orderedStart) + "\">";
      for (const auto &item : block.listItems) {
        out += "<li>";
        appendInlines(out, item);
        out += "</li>";
      }
      out += "</ol>";
      break;
    }
    case BlockKind::BlockQuote: {
      out += "<blockquote>";
      appendBlocks(out, block.children);
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

std::string toHtml(const Document &document) {
  std::string out;
  appendBlocks(out, document);
  return out;
}

} // namespace n8v::detail::markdown
