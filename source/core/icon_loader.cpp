#include "core/icon_loader.hpp"

#include "core/icon_registry.hpp"
#include "core/style.hpp"

#include "bundle.hpp"
#include "bundle_n8v_icons.h"

#include <plutosvg.h>
#include <plutovg.h>

#include <cstdint>
#include <fstream>
#include <map>
#include <optional>
#include <regex>
#include <string>
#include <tuple>

namespace n8v::detail {
namespace {

struct IconKey {
  std::string name;
  n8v::StyleFamily family;
  uint16_t pixelSize;
  n8v::Color tint;
  bool operator<(const IconKey &other) const {
    return std::tie(name, family, pixelSize, tint.r, tint.g, tint.b, tint.a) <
           std::tie(other.name, other.family, other.pixelSize, other.tint.r, other.tint.g, other.tint.b, other.tint.a);
  }
};

std::map<IconKey, DecodedImage> iconCache;

struct FileIconKey {
  std::string path;
  uint16_t pixelSize;
  n8v::Color tint;
  bool operator<(const FileIconKey &other) const {
    return std::tie(path, pixelSize, tint.r, tint.g, tint.b, tint.a) < std::tie(other.path, other.pixelSize, other.tint.r, other.tint.g, other.tint.b, other.tint.a);
  }
};

std::map<FileIconKey, DecodedImage> fileIconCache;

void unpremultiplyToRgba(const unsigned char *argb, int stride, int width, int height, std::vector<uint8_t> &out) {
  out.resize((size_t)width * (size_t)height * 4);
  for (int y = 0; y < height; ++y) {
    const unsigned char *row = argb + (size_t)y * stride;
    for (int x = 0; x < width; ++x) {
      const unsigned char *px = row + (size_t)x * 4;
      uint8_t b = px[0], g = px[1], r = px[2], a = px[3];
      uint8_t *dst = out.data() + ((size_t)y * width + x) * 4;
      if (a == 0) {
        dst[0] = dst[1] = dst[2] = 0;
      } else {
        dst[0] = (uint8_t)((int)r * 255 / a);
        dst[1] = (uint8_t)((int)g * 255 / a);
        dst[2] = (uint8_t)((int)b * 255 / a);
      }
      dst[3] = a;
    }
  }
}

std::string neutralizeToCurrentColor(std::string svg) {
  static const std::regex hexColorAttr(R"(((?:fill|stroke|color)\s*=\s*["'])#[0-9a-fA-F]{3,8}(["']))");
  svg = std::regex_replace(svg, hexColorAttr, "$1currentColor$2");

  static const std::regex svgTag(R"(<svg\b)");
  if (svg.find("fill=") == std::string::npos) {
    svg = std::regex_replace(svg, svgTag, "<svg fill=\"currentColor\"", std::regex_constants::format_first_only);
  }
  return svg;
}

bool rasterizeSvg(const std::string &svgText, uint16_t pixelSize, n8v::Color tint, DecodedImage &out) {
  plutosvg_document_t *doc = plutosvg_document_load_from_data(svgText.data(), (int)svgText.size(), (float)pixelSize, (float)pixelSize, nullptr, nullptr);
  if (!doc) return false;

  plutovg_color_t color;
  plutovg_color_init_rgba8(&color, (int)tint.r, (int)tint.g, (int)tint.b, (int)tint.a);

  plutovg_surface_t *surface = plutosvg_document_render_to_surface(doc, nullptr, pixelSize, pixelSize, &color, nullptr, nullptr);
  plutosvg_document_destroy(doc);
  if (!surface) return false;

  unpremultiplyToRgba(plutovg_surface_get_data(surface), plutovg_surface_get_stride(surface), pixelSize, pixelSize, out.owned);
  plutovg_surface_destroy(surface);
  out.rgba = out.owned.data();
  out.width = pixelSize;
  out.height = pixelSize;
  return true;
}

} // namespace

const DecodedImage *getOrDecodeIcon(std::string_view name, uint16_t pixelSize, n8v::Color tint) {
  if (name.empty() || pixelSize == 0) return nullptr;
  if (!isKnownIconName(name)) return nullptr;

  n8v::StyleFamily family = n8v::activeStyleFamily();
  IconKey key{std::string(name), family, pixelSize, tint};
  auto it = iconCache.find(key);
  if (it != iconCache.end()) return &it->second;

  static const bundle::archive archive(bundle_n8v_icons_archive());

  std::optional<bundle::entry> entry = archive.find(iconAssetPath(family, name));
  if (!entry && family != n8v::StyleFamily::Plain) entry = archive.find(iconAssetPath(n8v::StyleFamily::Plain, name));
  if (!entry) return nullptr;
  std::vector<std::byte> svgBytes = archive.load(*entry);
  std::string svgText = neutralizeToCurrentColor(std::string(reinterpret_cast<const char *>(svgBytes.data()), svgBytes.size()));

  DecodedImage image;
  if (!rasterizeSvg(svgText, pixelSize, tint, image)) return nullptr;
  return &(iconCache[key] = std::move(image));
}

const DecodedImage *getOrDecodeIconFromFile(const std::string &filePath, uint16_t pixelSize, n8v::Color tint) {
  if (filePath.empty() || pixelSize == 0) return nullptr;

  FileIconKey key{filePath, pixelSize, tint};
  auto it = fileIconCache.find(key);
  if (it != fileIconCache.end()) return &it->second;

  std::ifstream file(filePath, std::ios::binary | std::ios::ate);
  if (!file) return nullptr;
  std::streamsize size = file.tellg();
  if (size <= 0) return nullptr;
  file.seekg(0, std::ios::beg);
  std::string svgText(size, '\0');
  if (!file.read(svgText.data(), size)) return nullptr;
  svgText = neutralizeToCurrentColor(std::move(svgText));

  DecodedImage image;
  if (!rasterizeSvg(svgText, pixelSize, tint, image)) return nullptr;
  return &(fileIconCache[key] = std::move(image));
}

} // namespace n8v::detail
