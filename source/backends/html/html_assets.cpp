#include "html_assets.hpp"

#include "backends/clay_fallback/text/bundled_font.hpp"
#include "html_css.hpp"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace n8v::detail::html_assets {
namespace {

void put32(std::vector<uint8_t> &buf, uint32_t v) {
  buf.push_back((uint8_t)(v & 0xFF));
  buf.push_back((uint8_t)((v >> 8) & 0xFF));
  buf.push_back((uint8_t)((v >> 16) & 0xFF));
  buf.push_back((uint8_t)((v >> 24) & 0xFF));
}

void put16(std::vector<uint8_t> &buf, uint16_t v) {
  buf.push_back((uint8_t)(v & 0xFF));
  buf.push_back((uint8_t)((v >> 8) & 0xFF));
}

std::vector<uint8_t> encodeBmp(const DecodedImage &image) {
  const uint32_t width = (uint32_t)image.width;
  const uint32_t height = (uint32_t)image.height;
  const uint32_t pixelBytes = width * height * 4;
  const uint32_t headerBytes = 14 + 108;

  std::vector<uint8_t> buf;
  buf.reserve(headerBytes + pixelBytes);

  // File header
  buf.push_back('B');
  buf.push_back('M');
  put32(buf, headerBytes + pixelBytes);
  put32(buf, 0);
  put32(buf, headerBytes);

  // BITMAPV4HEADER
  put32(buf, 108);
  put32(buf, width);
  put32(buf, (uint32_t)(-(int32_t)height));
  put16(buf, 1);
  put16(buf, 32);
  put32(buf, 3); // BI_BITFIELDS
  put32(buf, pixelBytes);
  put32(buf, 0);
  put32(buf, 0);
  put32(buf, 0);
  put32(buf, 0);
  put32(buf, 0x00FF0000);                    // R mask
  put32(buf, 0x0000FF00);                    // G mask
  put32(buf, 0x000000FF);                    // B mask
  put32(buf, 0xFF000000);                    // A mask
  put32(buf, 0x73524742);                    // LCS_sRGB
  for (int i = 0; i < 9; ++i) put32(buf, 0); // CIEXYZTRIPLE endpoints: 3 colours x 3 components (was 12, which pushed the header to 120 bytes)
  put32(buf, 0);
  put32(buf, 0);
  put32(buf, 0);

  const uint8_t *rgba = image.rgba;
  for (uint32_t i = 0; i < width * height; ++i) {
    const uint8_t *px = rgba + (size_t)i * 4;
    buf.push_back(px[2]); // B
    buf.push_back(px[1]); // G
    buf.push_back(px[0]); // R
    buf.push_back(px[3]); // A
  }

  return buf;
}

std::string base64Encode(const std::vector<uint8_t> &data) {
  static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  out.reserve(((data.size() + 2) / 3) * 4);

  size_t i = 0;
  for (; i + 3 <= data.size(); i += 3) {
    uint32_t chunk = (uint32_t)data[i] << 16 | (uint32_t)data[i + 1] << 8 | (uint32_t)data[i + 2];
    out.push_back(table[(chunk >> 18) & 0x3F]);
    out.push_back(table[(chunk >> 12) & 0x3F]);
    out.push_back(table[(chunk >> 6) & 0x3F]);
    out.push_back(table[chunk & 0x3F]);
  }
  size_t remaining = data.size() - i;
  if (remaining == 1) {
    uint32_t chunk = (uint32_t)data[i] << 16;
    out.push_back(table[(chunk >> 18) & 0x3F]);
    out.push_back(table[(chunk >> 12) & 0x3F]);
    out.push_back('=');
    out.push_back('=');
  } else if (remaining == 2) {
    uint32_t chunk = (uint32_t)data[i] << 16 | (uint32_t)data[i + 1] << 8;
    out.push_back(table[(chunk >> 18) & 0x3F]);
    out.push_back(table[(chunk >> 12) & 0x3F]);
    out.push_back(table[(chunk >> 6) & 0x3F]);
    out.push_back('=');
  }
  return out;
}

void appendFace(std::string &css, FontFamily family, bool bold, bool italic) {
  std::vector<unsigned char> bytes = bundledFontBytes(family, bold, italic);
  std::string stack = html_css::cssFontFamily(family);
  std::string name = stack.substr(0, stack.find(','));
  css += "@font-face{font-family:" + name + ";font-weight:" + (bold ? "bold" : "normal") + ";font-style:" + (italic ? "italic" : "normal") + ";src:url(data:font/ttf;base64," +
         base64Encode(std::vector<uint8_t>(bytes.begin(), bytes.end())) + ") format('truetype');}\n";
}

} // namespace

std::string fontFaceCss() {
  std::string css;
  const FontFamily families[] = {FontFamily::DejaVuSans, FontFamily::Roboto, FontFamily::Inter, FontFamily::Selawik};
  for (FontFamily family : families) {
    appendFace(css, family, false, false);
    appendFace(css, family, true, false);
    appendFace(css, family, false, true);
    appendFace(css, family, true, true);
  }
  return css;
}

const std::string &imageDataUri(const DecodedImage &image) {
  static std::unordered_map<const DecodedImage *, std::string> cache;
  std::string &uri = cache[&image];
  if (uri.empty()) uri = "data:image/bmp;base64," + base64Encode(encodeBmp(image));
  return uri;
}

} // namespace n8v::detail::html_assets
