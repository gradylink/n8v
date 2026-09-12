#include "core/image_loader.hpp"

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#include <stb_image.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <map>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace n8v::detail {
namespace {

n8v_image_bundle_lookup_fn bundleLookupFn = nullptr;
void *bundleLookupUserdata = nullptr;

std::unordered_map<std::string, DecodedImage> pathCache;
std::unordered_map<std::string, DecodedImage> bundleCache;
std::unordered_map<const void *, DecodedImage> encodedCache;
std::unordered_map<const void *, DecodedImage> rgbaCache;

bool decodeInto(const uint8_t *bytes, size_t size, DecodedImage &out) {
  int w = 0, h = 0, channels = 0;
  unsigned char *decoded = stbi_load_from_memory(bytes, (int)size, &w, &h, &channels, 4);
  if (!decoded) return false;
  out.owned.assign(decoded, decoded + (size_t)w * (size_t)h * 4);
  stbi_image_free(decoded);
  out.rgba = out.owned.data();
  out.width = w;
  out.height = h;
  return true;
}

bool readFile(const std::string &path, std::vector<uint8_t> &out) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file) return false;
  std::streamsize size = file.tellg();
  if (size < 0) return false;
  file.seekg(0, std::ios::beg);
  out.resize((size_t)size);
  return (bool)file.read(reinterpret_cast<char *>(out.data()), size);
}

const DecodedImage *decodeFromPath(const std::string &path) {
  auto it = pathCache.find(path);
  if (it != pathCache.end()) return &it->second;

  std::vector<uint8_t> bytes;
  if (!readFile(path, bytes)) return nullptr;

  DecodedImage image;
  if (!decodeInto(bytes.data(), bytes.size(), image)) return nullptr;
  return &(pathCache[path] = std::move(image));
}

const DecodedImage *decodeFromBundle(const std::string &path) {
  auto it = bundleCache.find(path);
  if (it != bundleCache.end()) return &it->second;
  if (!bundleLookupFn) return nullptr;

  const void *data = nullptr;
  size_t size = 0;
  if (!bundleLookupFn(path.c_str(), &data, &size, bundleLookupUserdata)) return nullptr;

  DecodedImage image;
  if (!decodeInto(static_cast<const uint8_t *>(data), size, image)) return nullptr;
  return &(bundleCache[path] = std::move(image));
}

const DecodedImage *decodeFromEncoded(const uint8_t *data, size_t size) {
  auto it = encodedCache.find(data);
  if (it != encodedCache.end()) return &it->second;

  DecodedImage image;
  if (!decodeInto(data, size, image)) return nullptr;
  return &(encodedCache[data] = std::move(image));
}

const DecodedImage *viewRgba(const uint8_t *pixels, int width, int height) {
  auto it = rgbaCache.find(pixels);
  if (it != rgbaCache.end()) return &it->second;

  DecodedImage image;
  image.rgba = pixels;
  image.width = width;
  image.height = height;
  return &(rgbaCache[pixels] = std::move(image));
}

struct RoundKey {
  const void *source;
  int width, height;
  float topLeft, topRight, bottomLeft, bottomRight;
  bool operator<(const RoundKey &other) const {
    return std::tie(source, width, height, topLeft, topRight, bottomLeft, bottomRight) <
           std::tie(other.source, other.width, other.height, other.topLeft, other.topRight, other.bottomLeft, other.bottomRight);
  }
};

std::map<RoundKey, DecodedImage> roundedCache;

void resizeRgba(const uint8_t *src, int sw, int sh, std::vector<uint8_t> &dst, int dw, int dh) {
  dst.resize((size_t)dw * (size_t)dh * 4);
  for (int y = 0; y < dh; ++y) {
    float srcYf = (y + 0.5f) * sh / (float)dh - 0.5f;
    int y0 = (int)std::floor(srcYf);
    float fy = srcYf - (float)y0;
    int y1 = std::clamp(y0 + 1, 0, sh - 1);
    y0 = std::clamp(y0, 0, sh - 1);
    for (int x = 0; x < dw; ++x) {
      float srcXf = (x + 0.5f) * sw / (float)dw - 0.5f;
      int x0 = (int)std::floor(srcXf);
      float fx = srcXf - (float)x0;
      int x1 = std::clamp(x0 + 1, 0, sw - 1);
      x0 = std::clamp(x0, 0, sw - 1);

      const uint8_t *p00 = src + ((size_t)y0 * sw + x0) * 4;
      const uint8_t *p10 = src + ((size_t)y0 * sw + x1) * 4;
      const uint8_t *p01 = src + ((size_t)y1 * sw + x0) * 4;
      const uint8_t *p11 = src + ((size_t)y1 * sw + x1) * 4;
      uint8_t *out = dst.data() + ((size_t)y * dw + x) * 4;
      for (int c = 0; c < 4; ++c) {
        float top = p00[c] + (p10[c] - p00[c]) * fx;
        float bot = p01[c] + (p11[c] - p01[c]) * fx;
        out[c] = (uint8_t)std::clamp(top + (bot - top) * fy, 0.0f, 255.0f);
      }
    }
  }
}

void applyRoundedMask(std::vector<uint8_t> &rgba, int w, int h, float tl, float tr, float bl, float br) {
  float limit = std::min(w, h) * 0.5f;
  tl = std::clamp(tl, 0.0f, limit);
  tr = std::clamp(tr, 0.0f, limit);
  bl = std::clamp(bl, 0.0f, limit);
  br = std::clamp(br, 0.0f, limit);

  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      float px = x + 0.5f, py = y + 0.5f;
      float r = 0.0f, cx = 0.0f, cy = 0.0f;
      if (px < tl && py < tl) {
        r = tl;
        cx = tl;
        cy = tl;
      } else if (px >= w - tr && py < tr) {
        r = tr;
        cx = w - tr;
        cy = tr;
      } else if (px < bl && py >= h - bl) {
        r = bl;
        cx = bl;
        cy = h - bl;
      } else if (px >= w - br && py >= h - br) {
        r = br;
        cx = w - br;
        cy = h - br;
      } else {
        continue;
      }
      if (r <= 0.0f) continue;

      float dist = std::sqrt((px - cx) * (px - cx) + (py - cy) * (py - cy));
      float coverage = std::clamp(r - dist + 0.5f, 0.0f, 1.0f);
      uint8_t *alpha = &rgba[((size_t)y * w + x) * 4 + 3];
      *alpha = (uint8_t)((float)*alpha * coverage);
    }
  }
}

} // namespace

const DecodedImage *getOrDecodeImage(const n8v_image_options &options) {
  switch (options.source_kind) {
  case N8V_IMAGE_SOURCE_PATH:
    if (!options.path) return nullptr;
    return decodeFromPath(options.path);
  case N8V_IMAGE_SOURCE_BUNDLE:
    if (!options.path) return nullptr;
    return decodeFromBundle(options.path);
  case N8V_IMAGE_SOURCE_ENCODED:
    if (!options.encoded_data || options.encoded_size == 0) return nullptr;
    return decodeFromEncoded(options.encoded_data, options.encoded_size);
  case N8V_IMAGE_SOURCE_RGBA:
    if (!options.pixels || options.pixel_width <= 0 || options.pixel_height <= 0) return nullptr;
    return viewRgba(options.pixels, options.pixel_width, options.pixel_height);
  }
  return nullptr;
}

void setImageBundleLookup(n8v_image_bundle_lookup_fn fn, void *userdata) {
  bundleLookupFn = fn;
  bundleLookupUserdata = userdata;
  bundleCache.clear();
}

const DecodedImage *getOrBakeRoundedImage(
  const DecodedImage *source, int targetWidth, int targetHeight, float radiusTopLeft, float radiusTopRight, float radiusBottomLeft, float radiusBottomRight
) {
  if (!source || targetWidth <= 0 || targetHeight <= 0) return source;
  if (radiusTopLeft <= 0.0f && radiusTopRight <= 0.0f && radiusBottomLeft <= 0.0f && radiusBottomRight <= 0.0f) return source;

  RoundKey key{source, targetWidth, targetHeight, radiusTopLeft, radiusTopRight, radiusBottomLeft, radiusBottomRight};
  auto it = roundedCache.find(key);
  if (it != roundedCache.end()) return &it->second;

  DecodedImage baked;
  if (targetWidth == source->width && targetHeight == source->height) {
    baked.owned.assign(source->rgba, source->rgba + (size_t)targetWidth * (size_t)targetHeight * 4);
  } else {
    resizeRgba(source->rgba, source->width, source->height, baked.owned, targetWidth, targetHeight);
  }
  applyRoundedMask(baked.owned, targetWidth, targetHeight, radiusTopLeft, radiusTopRight, radiusBottomLeft, radiusBottomRight);
  baked.rgba = baked.owned.data();
  baked.width = targetWidth;
  baked.height = targetHeight;

  return &(roundedCache[key] = std::move(baked));
}

} // namespace n8v::detail
