#include <n8v/n8v_c.h>

#include "core/backend.hpp"
#include "core/clay_convert.hpp"
#include "core/markdown/markdown.hpp"
#include "core/native_widget_meta.hpp"
#include "core/style.hpp"
#include "core/ui_core_internal.hpp"

#include <clay.h>

#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace n8v::detail::ui_internal;

float measureDocumentHeight(const n8v::detail::markdown::Document &doc, n8v::FontFamily family, uint16_t baseFontSize, float maxWidth) {
  using n8v::detail::markdown::Block;
  using n8v::detail::markdown::BlockKind;
  using n8v::detail::markdown::InlineRun;

  if (maxWidth <= 0.0f) return baseFontSize * 1.4f;

  n8v::Backend &backend = n8v::activeBackend();
  auto lineHeightFor = [](uint16_t fontSize) { return fontSize * 1.4f; };

  float x = 0.0f;
  float y = 0.0f;

  auto newline = [&](uint16_t fontSize) {
    x = 0.0f;
    y += lineHeightFor(fontSize);
  };

  auto layoutRun = [&](const InlineRun &run, uint16_t fontSize) {
    size_t pos = 0;
    while (pos <= run.text.size()) {
      size_t next = run.text.find(' ', pos);
      std::string_view word = next == std::string::npos ? std::string_view(run.text).substr(pos) : std::string_view(run.text).substr(pos, next - pos);
      if (!word.empty()) {
        float w = backend.measureText(word, family, fontSize, run.bold, run.italic).width;
        if (x > 0.0f && x + w > maxWidth) newline(fontSize);
        float spaceWidth = backend.measureText(" ", family, fontSize, run.bold, run.italic).width;
        x += w + spaceWidth;
      }
      if (next == std::string::npos) break;
      pos = next + 1;
    }
  };

  auto layoutInlines = [&](const std::vector<InlineRun> &runs, uint16_t fontSize) {
    for (const InlineRun &run : runs) layoutRun(run, fontSize);
  };

  std::function<void(const std::vector<Block> &)> layoutBlocks;
  layoutBlocks = [&](const std::vector<Block> &blocks) {
    for (size_t i = 0; i < blocks.size(); i++) {
      const Block &block = blocks[i];
      if (i > 0) {
        if (x > 0.0f) newline(baseFontSize);
        y += lineHeightFor(baseFontSize) * 0.5f;
      }
      switch (block.kind) {
      case BlockKind::Heading: {
        float scale = block.level <= 1 ? 1.8f : block.level == 2 ? 1.5f : block.level == 3 ? 1.3f : block.level == 4 ? 1.15f : 1.0f;
        uint16_t fontSize = (uint16_t)(baseFontSize * scale);
        layoutInlines(block.inlines, fontSize);
        newline(fontSize);
        break;
      }
      case BlockKind::Paragraph: {
        layoutInlines(block.inlines, baseFontSize);
        break;
      }
      case BlockKind::CodeBlock: {
        std::string_view text = block.codeText;
        while (!text.empty() && text.back() == '\n') text.remove_suffix(1);
        size_t lineCount = 1;
        for (char c : text)
          if (c == '\n') lineCount++;
        y += lineHeightFor(baseFontSize) * (float)lineCount;
        newline(baseFontSize);
        break;
      }
      case BlockKind::BulletList: {
        for (size_t j = 0; j < block.listItems.size(); j++) {
          if (j > 0) newline(baseFontSize);
          x += backend.measureText("\xE2\x80\xA2 ", family, baseFontSize, false, false).width;
          layoutInlines(block.listItems[j], baseFontSize);
        }
        break;
      }
      case BlockKind::OrderedList: {
        for (size_t j = 0; j < block.listItems.size(); j++) {
          if (j > 0) newline(baseFontSize);
          std::string marker = std::to_string(block.orderedStart + (int)j) + ". ";
          x += backend.measureText(marker, family, baseFontSize, false, false).width;
          layoutInlines(block.listItems[j], baseFontSize);
        }
        break;
      }
      case BlockKind::BlockQuote: {
        layoutBlocks(block.children);
        break;
      }
      case BlockKind::ThematicBreak: {
        newline(baseFontSize);
        break;
      }
      }
    }
  };

  layoutBlocks(doc);
  return y + lineHeightFor(baseFontSize);
}

} // namespace

extern "C" {

void n8v_document(n8v_document_options options) {
  openElementMaybeWithId(options.id);

  const int ordinal = widgetOrdinal++;
  const n8v::detail::markdown::CachedDocument cached = n8v::detail::markdown::getOrParse(toView(options.markdown));

  Clay_ElementDeclaration decl = {};
  decl.layout.sizing.width = n8v::detail::toClay(toSizing(options.width));

  n8v::Sizing heightSizing = toSizing(options.height);
  if (heightSizing.mode == n8v::SizingMode::Fit && options.id && *options.id) {
    Clay_ElementId elementId = Clay_GetElementId(Clay_String{false, (int32_t)std::strlen(options.id), options.id});
    Clay_ElementData previous = Clay_GetElementData(elementId);
    const n8v::TextPaint measurePaint = n8v::activePaint().text(n8v::TextOptions{});
    float measuredHeight =
      previous.found ? measureDocumentHeight(*cached.document, measurePaint.font, measurePaint.fontSize, previous.boundingBox.width) : measurePaint.fontSize * 1.4f;
    heightSizing = n8v::Sizing::fixed(measuredHeight);
  }
  decl.layout.sizing.height = n8v::detail::toClay(heightSizing);

  decl.backgroundColor = {0, 0, 0, 1.0f / 255.0f};

  widgetMetaStorage.push_back(n8v::detail::NativeWidgetMeta{});
  n8v::detail::NativeWidgetMeta &meta = widgetMetaStorage.back();
  meta.kind = n8v::NativeWidgetKind::Document;
  meta.ordinal = ordinal;
  if (options.id) meta.id = options.id;
  meta.documentSource = cached.source;
  meta.documentAst = cached.document;
  const n8v::TextPaint basePaint = n8v::activePaint().text(n8v::TextOptions{});
  const n8v::TextPaint linkPaint = n8v::activePaint().text(n8v::TextOptions{.url = "#"});
  meta.documentFont = basePaint.font;
  meta.documentFontSize = basePaint.fontSize;
  meta.documentTextColor = basePaint.color;
  meta.documentLinkColor = linkPaint.color;
  decl.userData = &meta;

  Clay__ConfigureOpenElement(decl);
  Clay__CloseElement();
}

} // extern "C"
