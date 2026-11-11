#include "sdl2_backend_impl.hpp"

#include "core/markdown/inline_image.hpp"
#include "core/markdown/markdown.hpp"
#include "core/native_widget_meta.hpp"
#include "core/text_style_flags.hpp"

#include "backends/clay_fallback/text/line_layout.hpp"

#include <algorithm>
#include <functional>
#include <string>
#include <string_view>

namespace n8v::detail {

namespace {

using n8v::detail::markdown::Block;
using n8v::detail::markdown::BlockKind;
using n8v::detail::markdown::InlineRun;

struct DocCursor {
  float originX = 0.0f;
  float maxWidth = 0.0f;
  float x = 0.0f;
  float y = 0.0f;
};

} // namespace

void Sdl2Backend::renderDocument(NativeWidgetMeta &meta, const Clay_BoundingBox &box) {
  if (!meta.documentAst) return;

  FontFamily family = meta.documentFont;
  n8v::Color baseColor = meta.documentTextColor;
  n8v::Color linkColor = meta.documentLinkColor;
  uint16_t baseFontSize = meta.documentFontSize;

  DocCursor cursor{box.x, box.width, box.x, box.y};

  auto lineHeightFor = [](uint16_t fontSize) { return fontSize * 1.4f; };

  float lineExtra = 0.0f; // how much taller than usual the current line is because of an inline image

  auto newline = [&](uint16_t fontSize) {
    cursor.x = cursor.originX;
    cursor.y += lineHeightFor(fontSize) + lineExtra;
    lineExtra = 0.0f;
  };

  auto drawWord = [&](std::string_view word, uint16_t fontSize, bool bold, bool italic, bool strike, bool link, float wordWidth, const n8v_link_style *styled = nullptr) {
    TextStyleFlags flags{};
    flags.font = family;
    flags.bold = bold;
    flags.italic = italic;
    flags.strikethrough = strike;
    flags.underline = link && !styled;
    flags.ownedByWidget = false;

    float glyphHeight = measureLine(word, family, fontSize, bold, italic).height;

    Clay_RenderCommand cmd{};
    cmd.boundingBox = {cursor.x, cursor.y, wordWidth, glyphHeight};
    cmd.renderData.text.stringContents = Clay_StringSlice{(int32_t)word.size(), word.data(), word.data()};
    cmd.renderData.text.fontSize = fontSize;
    n8v::Color c = link && !styled ? linkColor : baseColor;
    if (styled && styled->foreground.a > 0.0f) c = n8v::Color{styled->foreground.r, styled->foreground.g, styled->foreground.b, styled->foreground.a};
    cmd.renderData.text.textColor = Clay_Color{c.r, c.g, c.b, c.a};
    cmd.userData = &flags;
    drawText(cmd);
  };

  auto layoutRun = [&](const InlineRun &run, uint16_t fontSize) {
    if (!run.imageUrl.empty()) {
      float imageWidth = 0.0f, imageHeight = 0.0f;
      if (const DecodedImage *image = n8v::detail::markdown::inlineImageFor(run, meta.documentImageState, lineHeightFor(fontSize) * 0.9f, imageWidth, imageHeight)) {
        if (cursor.x > cursor.originX && cursor.x + imageWidth > cursor.originX + cursor.maxWidth) newline(fontSize);
        lineExtra = std::max(lineExtra, imageHeight - lineHeightFor(fontSize));
        const float lineHeight = lineHeightFor(fontSize);
        drawInlineImage(image, Clay_BoundingBox{cursor.x, cursor.y + (std::max(lineHeight, imageHeight) - imageHeight) * 0.5f, imageWidth, imageHeight});
        cursor.x += imageWidth + 2.0f;
        return;
      }
    }
    bool link = !run.linkUrl.empty();
    n8v_link_style style{};
    const bool styled = link && meta.documentLinkStyle && meta.documentLinkStyle(run.linkUrl.c_str(), &style, meta.documentLinkStyleUserdata);
    const bool bold = run.bold || (styled && style.bold);
    size_t pos = 0;
    while (pos <= run.text.size()) {
      size_t next = run.text.find(' ', pos);
      std::string_view word = next == std::string::npos ? std::string_view(run.text).substr(pos) : std::string_view(run.text).substr(pos, next - pos);
      if (!word.empty()) {
        float w = measureLine(word, family, fontSize, bold, run.italic).width;
        if (cursor.x > cursor.originX && cursor.x + w > cursor.originX + cursor.maxWidth) newline(fontSize);
        float spaceWidth = measureLine(" ", family, fontSize, bold, run.italic).width;
        if (styled && style.background.a > 0.0f) {
          const bool first = pos == 0;
          const bool continues = next != std::string::npos && next + 1 < run.text.size();
          const float padLeft = first ? 2.0f : 0.0f;
          const float padRight = continues ? spaceWidth : 2.0f;
          drawFilledRect(
            Clay_BoundingBox{cursor.x - padLeft, cursor.y, w + padLeft + padRight, lineHeightFor(fontSize)},
            Clay_Color{style.background.r, style.background.g, style.background.b, style.background.a}
          );
        }
        drawWord(word, fontSize, bold, run.italic, run.strikethrough, link, w, styled ? &style : nullptr);
        cursor.x += w + spaceWidth;
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
        if (cursor.x > cursor.originX) newline(baseFontSize);
        cursor.y += lineHeightFor(baseFontSize) * 0.5f;
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

        float lineH = lineHeightFor(baseFontSize);
        size_t lineCount = 1;
        for (char c : text)
          if (c == '\n') lineCount++;
        Clay_BoundingBox codeBox{cursor.originX, cursor.y, cursor.maxWidth, lineH * (float)lineCount};
        drawFilledRect(codeBox, Clay_Color{0, 0, 0, 20});
        size_t pos = 0;
        while (true) {
          size_t next = text.find('\n', pos);
          std::string_view line = next == std::string_view::npos ? text.substr(pos) : text.substr(pos, next - pos);
          if (!line.empty()) drawWord(line, baseFontSize, false, false, false, false, measureLine(line, family, baseFontSize, false, false).width);
          if (next == std::string_view::npos) break;
          newline(baseFontSize);
          pos = next + 1;
        }
        newline(baseFontSize);
        break;
      }
      case BlockKind::BulletList: {
        for (size_t j = 0; j < block.listItems.size(); j++) {
          if (j > 0) newline(baseFontSize);
          std::string bullet = "• ";
          drawWord(bullet, baseFontSize, false, false, false, false, measureLine(bullet, family, baseFontSize, false, false).width);
          cursor.x += measureLine(bullet, family, baseFontSize, false, false).width;
          layoutInlines(block.listItems[j], baseFontSize);
        }
        break;
      }
      case BlockKind::OrderedList: {
        for (size_t j = 0; j < block.listItems.size(); j++) {
          if (j > 0) newline(baseFontSize);
          std::string marker = std::to_string(block.orderedStart + (int)j) + ". ";
          drawWord(marker, baseFontSize, false, false, false, false, measureLine(marker, family, baseFontSize, false, false).width);
          cursor.x += measureLine(marker, family, baseFontSize, false, false).width;
          layoutInlines(block.listItems[j], baseFontSize);
        }
        break;
      }
      case BlockKind::BlockQuote: {
        layoutBlocks(block.children);
        break;
      }
      case BlockKind::ThematicBreak: {
        Clay_BoundingBox line{cursor.originX, cursor.y + lineHeightFor(baseFontSize) * 0.5f, cursor.maxWidth, 1.0f};
        drawFilledRect(line, Clay_Color{baseColor.r, baseColor.g, baseColor.b, 80});
        newline(baseFontSize);
        break;
      }
      }
    }
  };

  layoutBlocks(*meta.documentAst);
}

} // namespace n8v::detail
