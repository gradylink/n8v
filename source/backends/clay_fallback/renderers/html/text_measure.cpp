#include "html_backend_impl.hpp"

#include <string>

namespace n8v::detail {

Clay_Dimensions HtmlBackend::measureText(std::string_view text, FontFamily family, uint16_t fontSize, bool bold, bool italic) const {
  if (text.empty()) return {0.0f, (float)fontSize * 1.2f};

  std::string cssFont;
  if (italic) cssFont += "italic ";
  if (bold) cssFont += "bold ";
  cssFont += std::to_string(fontSize) + "px " + htmlFontFamilyName(family);

  measureCtx_.set("font", cssFont);
  emscripten::val metrics = measureCtx_.call<emscripten::val>("measureText", std::string(text));

  float width = metrics["width"].as<float>();
  float height = (float)fontSize * 1.2f;
  emscripten::val ascent = metrics["fontBoundingBoxAscent"];
  emscripten::val descent = metrics["fontBoundingBoxDescent"];
  if (!ascent.isUndefined() && !descent.isUndefined()) {
    float measured = ascent.as<float>() + descent.as<float>();
    if (measured > 0.0f) height = measured;
  }

  return {width, height};
}

} // namespace n8v::detail
