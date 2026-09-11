#include "html_backend_impl.hpp"

#include "backends/clay_fallback/text/bundled_font.hpp"

#include <cstdint>
#include <vector>

namespace n8v::detail {

std::string htmlFontFamilyName(FontFamily family) {
  switch (family) {
  case FontFamily::Roboto: return "n8v-Roboto";
  case FontFamily::Inter: return "n8v-Inter";
  case FontFamily::Selawik: return "n8v-Selawik";
  case FontFamily::DejaVuSans:
  default: return "n8v-DejaVuSans";
  }
}

namespace {

std::string base64Encode(const std::vector<unsigned char> &data) {
  static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  out.reserve(((data.size() + 2) / 3) * 4);
  size_t i = 0;
  for (; i + 2 < data.size(); i += 3) {
    uint32_t n = ((uint32_t)data[i] << 16) | ((uint32_t)data[i + 1] << 8) | (uint32_t)data[i + 2];
    out += table[(n >> 18) & 0x3F];
    out += table[(n >> 12) & 0x3F];
    out += table[(n >> 6) & 0x3F];
    out += table[n & 0x3F];
  }
  size_t rem = data.size() - i;
  if (rem == 1) {
    uint32_t n = (uint32_t)data[i] << 16;
    out += table[(n >> 18) & 0x3F];
    out += table[(n >> 12) & 0x3F];
    out += "==";
  } else if (rem == 2) {
    uint32_t n = ((uint32_t)data[i] << 16) | ((uint32_t)data[i + 1] << 8);
    out += table[(n >> 18) & 0x3F];
    out += table[(n >> 12) & 0x3F];
    out += table[(n >> 6) & 0x3F];
    out += "=";
  }
  return out;
}

void appendFace(std::string &css, FontFamily family, bool bold, bool italic) {
  std::vector<unsigned char> bytes = bundledFontBytes(family, bold, italic);
  css += "@font-face{font-family:'" + htmlFontFamilyName(family) + "';font-weight:" + (bold ? "bold" : "normal") + ";font-style:" +
         (italic ? "italic" : "normal") + ";src:url(data:font/ttf;base64," + base64Encode(bytes) + ") format('truetype');}\n";
}

} // namespace

void HtmlBackend::injectFontFaces() {
  std::string css;
  const FontFamily families[] = {FontFamily::DejaVuSans, FontFamily::Roboto, FontFamily::Inter, FontFamily::Selawik};
  for (FontFamily family : families) {
    appendFace(css, family, false, false);
    appendFace(css, family, true, false);
    appendFace(css, family, false, true);
    appendFace(css, family, true, true);
  }

  emscripten::val style = doc_.call<emscripten::val>("createElement", std::string("style"));
  style.set("textContent", css);
  doc_["head"].call<void>("appendChild", style);
}

} // namespace n8v::detail
