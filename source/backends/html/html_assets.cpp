#include "html_assets.hpp"

#include "backends/clay_fallback/text/bundled_font.hpp"
#include "html_css.hpp"

#include <plutovg.h>

#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace n8v::detail::html_assets {
namespace {

void writePng(void *closure, void *data, int size) {
  auto *out = static_cast<std::vector<uint8_t> *>(closure);
  const auto *bytes = static_cast<const uint8_t *>(data);
  out->insert(out->end(), bytes, bytes + size);
}

std::vector<uint8_t> encodePng(const DecodedImage &image) {
  std::vector<uint8_t> argb((size_t)image.width * (size_t)image.height * 4);
  plutovg_convert_rgba_to_argb(argb.data(), image.rgba, image.width, image.height, image.width * 4);
  plutovg_surface_t *surface = plutovg_surface_create_for_data(argb.data(), image.width, image.height, image.width * 4);
  std::vector<uint8_t> png;
  if (surface) {
    plutovg_surface_write_to_png_stream(surface, writePng, &png);
    plutovg_surface_destroy(surface);
  }
  return png;
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

void appendFace(std::string &css, FontFamily family, bool bold, bool italic, const FontUrlFn &urlFor) {
  std::vector<unsigned char> bytes = bundledFontBytes(family, bold, italic);
  std::string stack = html_css::cssFontFamily(family);
  std::string name = stack.substr(0, stack.find(','));
  std::string bare = name;
  bare.erase(std::remove(bare.begin(), bare.end(), '\''), bare.end());
  const std::string file = bare + (bold ? "-bold" : "") + (italic ? "-italic" : "") + ".ttf";
  css += "@font-face{font-family:" + name + ";font-weight:" + (bold ? "bold" : "normal") + ";font-style:" + (italic ? "italic" : "normal") + ";src:url(" + urlFor(file, bytes) +
         ") format('truetype');}\n";
}

} // namespace

std::string fontFaceCss(FontFamily family, const FontUrlFn &urlFor) {
  std::string css;
  appendFace(css, family, false, false, urlFor);
  appendFace(css, family, true, false, urlFor);
  appendFace(css, family, false, true, urlFor);
  appendFace(css, family, true, true, urlFor);
  return css;
}

const std::vector<uint8_t> &imagePng(const DecodedImage &image) {
  static std::unordered_map<const DecodedImage *, std::vector<uint8_t>> cache;
  std::vector<uint8_t> &png = cache[&image];
  if (png.empty()) png = encodePng(image);
  return png;
}

const std::string &imageDataUri(const DecodedImage &image) {
  static std::unordered_map<const DecodedImage *, std::string> cache;
  std::string &uri = cache[&image];
  if (uri.empty()) uri = "data:image/png;base64," + base64Encode(imagePng(image));
  return uri;
}

std::string contentHash(const void *data, size_t size) {
  uint64_t h = 1469598103934665603ull;
  const auto *bytes = static_cast<const uint8_t *>(data);
  for (size_t i = 0; i < size; ++i) h = (h ^ bytes[i]) * 1099511628211ull;
  char hex[24];
  std::snprintf(hex, sizeof hex, "%016llx", (unsigned long long)h);
  return hex;
}

} // namespace n8v::detail::html_assets
