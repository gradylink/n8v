#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace n8v::detail::markdown {

struct InlineRun {
  std::string text;
  bool bold = false;
  bool italic = false;
  bool strikethrough = false;
  bool code = false;
  std::string linkUrl; // empty if this run isn't a link
};

enum class BlockKind {
  Heading,
  Paragraph,
  CodeBlock,
  BulletList,
  OrderedList,
  BlockQuote,
  ThematicBreak,
};

struct Block {
  BlockKind kind;
  int level = 0;                                 // Heading only
  std::vector<InlineRun> inlines;                // Heading/Paragraph only
  std::string codeText;                          // CodeBlock only
  std::string codeLang;                          // CodeBlock only
  std::vector<std::vector<InlineRun>> listItems; // BulletList/OrderedList only
  int orderedStart = 1;                          // OrderedList only
  std::vector<Block> children;                   // BlockQuote only
};

using Document = std::vector<Block>;

Document parse(std::string_view source);

struct CachedDocument {
  const std::string *source;
  const Document *document;
};
CachedDocument getOrParse(std::string_view source);

} // namespace n8v::detail::markdown
