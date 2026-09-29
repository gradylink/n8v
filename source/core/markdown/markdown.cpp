#include "markdown.hpp"

#include <md4c.h>

#include <unordered_map>

namespace n8v::detail::markdown {

namespace {

void appendEntity(std::string &out, std::string_view entity) {
  if (entity == "&amp;") {
    out += '&';
  } else if (entity == "&lt;") {
    out += '<';
  } else if (entity == "&gt;") {
    out += '>';
  } else if (entity == "&quot;") {
    out += '"';
  } else if (entity == "&apos;" || entity == "&#39;") {
    out += '\'';
  } else if (entity == "&nbsp;") {
    out += ' ';
  } else {
    out += entity;
  }
}

struct Builder {
  Document root;
  std::vector<std::vector<Block> *> containerStack{&root};
  std::vector<Block *> quoteStack;
  std::vector<Block *> listStack;
  std::vector<Block> pendingBlockStack;
  std::vector<std::vector<InlineRun> *> targetStack;

  int listItemDepth = 0;
  bool inCodeBlock = false;
  std::string codeAccum;
  std::string codeLangAccum;

  int boldDepth = 0;
  int italicDepth = 0;
  int strikeDepth = 0;
  int codeSpanDepth = 0;
  std::vector<std::string> linkStack;

  std::vector<InlineRun> *currentTarget() { return targetStack.empty() ? nullptr : targetStack.back(); }
};

std::string_view attrView(const MD_ATTRIBUTE &attr) { return std::string_view(attr.text, attr.size); }

int enterBlock(MD_BLOCKTYPE type, void *detail, void *userdata) {
  auto &b = *static_cast<Builder *>(userdata);
  switch (type) {
  case MD_BLOCK_H: {
    Block block{};
    block.kind = BlockKind::Heading;
    block.level = (int)static_cast<MD_BLOCK_H_DETAIL *>(detail)->level;
    if (b.listItemDepth == 0) {
      b.pendingBlockStack.push_back(std::move(block));
      b.targetStack.push_back(&b.pendingBlockStack.back().inlines);
    }
    break;
  }
  case MD_BLOCK_P: {
    if (b.listItemDepth == 0) {
      Block block{};
      block.kind = BlockKind::Paragraph;
      b.pendingBlockStack.push_back(std::move(block));
      b.targetStack.push_back(&b.pendingBlockStack.back().inlines);
    }
    break;
  }
  case MD_BLOCK_CODE: {
    b.inCodeBlock = true;
    b.codeAccum.clear();
    b.codeLangAccum.clear();
    auto *d = static_cast<MD_BLOCK_CODE_DETAIL *>(detail);
    if (d->lang.text) b.codeLangAccum.assign(attrView(d->lang));
    break;
  }
  case MD_BLOCK_QUOTE: {
    b.containerStack.back()->push_back(Block{});
    Block *quote = &b.containerStack.back()->back();
    quote->kind = BlockKind::BlockQuote;
    b.quoteStack.push_back(quote);
    b.containerStack.push_back(&quote->children);
    break;
  }
  case MD_BLOCK_UL: {
    b.containerStack.back()->push_back(Block{});
    Block *list = &b.containerStack.back()->back();
    list->kind = BlockKind::BulletList;
    b.listStack.push_back(list);
    break;
  }
  case MD_BLOCK_OL: {
    b.containerStack.back()->push_back(Block{});
    Block *list = &b.containerStack.back()->back();
    list->kind = BlockKind::OrderedList;
    list->orderedStart = (int)static_cast<MD_BLOCK_OL_DETAIL *>(detail)->start;
    b.listStack.push_back(list);
    break;
  }
  case MD_BLOCK_LI: {
    if (!b.listStack.empty()) {
      Block *list = b.listStack.back();
      list->listItems.emplace_back();
      b.targetStack.push_back(&list->listItems.back());
      b.listItemDepth++;
    }
    break;
  }
  case MD_BLOCK_HR: {
    b.containerStack.back()->push_back(Block{});
    b.containerStack.back()->back().kind = BlockKind::ThematicBreak;
    break;
  }
  default:
    break;
  }
  return 0;
}

int leaveBlock(MD_BLOCKTYPE type, void * /*detail*/, void *userdata) {
  auto &b = *static_cast<Builder *>(userdata);
  switch (type) {
  case MD_BLOCK_H:
  case MD_BLOCK_P: {
    if (b.listItemDepth == 0 && !b.pendingBlockStack.empty()) {
      b.targetStack.pop_back();
      Block block = std::move(b.pendingBlockStack.back());
      b.pendingBlockStack.pop_back();
      b.containerStack.back()->push_back(std::move(block));
    }
    break;
  }
  case MD_BLOCK_CODE: {
    b.inCodeBlock = false;
    Block block{};
    block.kind = BlockKind::CodeBlock;
    block.codeText = b.codeAccum;
    block.codeLang = b.codeLangAccum;
    b.containerStack.back()->push_back(std::move(block));
    break;
  }
  case MD_BLOCK_QUOTE: {
    if (!b.quoteStack.empty()) b.quoteStack.pop_back();
    if (b.containerStack.size() > 1) b.containerStack.pop_back();
    break;
  }
  case MD_BLOCK_UL:
  case MD_BLOCK_OL: {
    if (!b.listStack.empty()) b.listStack.pop_back();
    break;
  }
  case MD_BLOCK_LI: {
    if (b.listItemDepth > 0) {
      b.targetStack.pop_back();
      b.listItemDepth--;
    }
    break;
  }
  default:
    break;
  }
  return 0;
}

int enterSpan(MD_SPANTYPE type, void *detail, void *userdata) {
  auto &b = *static_cast<Builder *>(userdata);
  switch (type) {
  case MD_SPAN_STRONG:
    b.boldDepth++;
    break;
  case MD_SPAN_EM:
    b.italicDepth++;
    break;
  case MD_SPAN_DEL:
    b.strikeDepth++;
    break;
  case MD_SPAN_CODE:
    b.codeSpanDepth++;
    break;
  case MD_SPAN_A: {
    auto *d = static_cast<MD_SPAN_A_DETAIL *>(detail);
    b.linkStack.emplace_back(attrView(d->href));
    break;
  }
  default:
    break;
  }
  return 0;
}

int leaveSpan(MD_SPANTYPE type, void * /*detail*/, void *userdata) {
  auto &b = *static_cast<Builder *>(userdata);
  switch (type) {
  case MD_SPAN_STRONG:
    b.boldDepth--;
    break;
  case MD_SPAN_EM:
    b.italicDepth--;
    break;
  case MD_SPAN_DEL:
    b.strikeDepth--;
    break;
  case MD_SPAN_CODE:
    b.codeSpanDepth--;
    break;
  case MD_SPAN_A:
    if (!b.linkStack.empty()) b.linkStack.pop_back();
    break;
  default:
    break;
  }
  return 0;
}

int onText(MD_TEXTTYPE type, const MD_CHAR *text, MD_SIZE size, void *userdata) {
  auto &b = *static_cast<Builder *>(userdata);
  std::string_view view(text, size);

  if (b.inCodeBlock) {
    b.codeAccum.append(view);
    return 0;
  }

  std::vector<InlineRun> *target = b.currentTarget();
  if (!target) return 0;

  switch (type) {
  case MD_TEXT_BR:
  case MD_TEXT_SOFTBR: {
    if (!target->empty()) target->back().text += ' ';
    break;
  }
  case MD_TEXT_ENTITY: {
    std::string decoded;
    appendEntity(decoded, view);
    InlineRun run{};
    run.text = std::move(decoded);
    run.bold = b.boldDepth > 0;
    run.italic = b.italicDepth > 0;
    run.strikethrough = b.strikeDepth > 0;
    run.code = b.codeSpanDepth > 0;
    if (!b.linkStack.empty()) run.linkUrl = b.linkStack.back();
    target->push_back(std::move(run));
    break;
  }
  case MD_TEXT_NORMAL:
  case MD_TEXT_CODE:
  default: {
    InlineRun run{};
    run.text.assign(view);
    run.bold = b.boldDepth > 0;
    run.italic = b.italicDepth > 0;
    run.strikethrough = b.strikeDepth > 0;
    run.code = b.codeSpanDepth > 0 || type == MD_TEXT_CODE;
    if (!b.linkStack.empty()) run.linkUrl = b.linkStack.back();
    target->push_back(std::move(run));
    break;
  }
  }
  return 0;
}

} // namespace

Document parse(std::string_view source) {
  Builder builder{};

  MD_PARSER parser{};
  parser.abi_version = 0;
  parser.flags = MD_FLAG_STRIKETHROUGH | MD_FLAG_PERMISSIVEAUTOLINKS;
  parser.enter_block = enterBlock;
  parser.leave_block = leaveBlock;
  parser.enter_span = enterSpan;
  parser.leave_span = leaveSpan;
  parser.text = onText;

  md_parse(source.data(), (MD_SIZE)source.size(), &parser, &builder);

  return std::move(builder.root);
}

CachedDocument getOrParse(std::string_view source) {
  static std::unordered_map<std::string, Document> cache;
  std::string key(source);
  auto it = cache.find(key);
  if (it == cache.end()) it = cache.emplace(std::move(key), parse(source)).first;
  return CachedDocument{&it->first, &it->second};
}

} // namespace n8v::detail::markdown
