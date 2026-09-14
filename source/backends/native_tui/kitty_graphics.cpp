#include "kitty_graphics.hpp"

#include "core/image_loader.hpp"

#include <algorithm>

namespace n8v::detail {

namespace {

std::string base64Encode(const uint8_t *data, size_t len) {
  static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  out.reserve((len + 2) / 3 * 4);
  size_t i = 0;
  for (; i + 2 < len; i += 3) {
    uint32_t n = ((uint32_t)data[i] << 16) | ((uint32_t)data[i + 1] << 8) | data[i + 2];
    out += table[(n >> 18) & 0x3F];
    out += table[(n >> 12) & 0x3F];
    out += table[(n >> 6) & 0x3F];
    out += table[n & 0x3F];
  }
  size_t rem = len - i;
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

} // namespace

std::string kittyMoveCursor(int col, int row) { return "\x1b[" + std::to_string(row + 1) + ";" + std::to_string(col + 1) + "H"; }

std::string kittyTransmitAndPlace(const DecodedImage &image, uint32_t imageId, int cols, int rows) {
  std::string b64 = base64Encode(image.rgba, (size_t)image.width * (size_t)image.height * 4);
  std::string out;
  const size_t chunkSize = 4096;
  size_t offset = 0;
  bool first = true;
  while (offset < b64.size() || first) {
    size_t n = std::min(chunkSize, b64.size() - offset);
    bool more = (offset + n) < b64.size();
    out += "\x1b_G";
    if (first) {
      out += "a=T,f=32,i=" + std::to_string(imageId) + ",s=" + std::to_string(image.width) + ",v=" + std::to_string(image.height) + ",c=" + std::to_string(cols) +
             ",r=" + std::to_string(rows) + ",z=-2,q=2,m=" + (more ? "1" : "0");
    } else {
      out += "m=" + std::string(more ? "1" : "0");
    }
    out += ";";
    out += b64.substr(offset, n);
    out += "\x1b\\";
    offset += n;
    first = false;
  }
  return out;
}

std::string kittyPut(uint32_t imageId, int cols, int rows) {
  return "\x1b_Ga=p,i=" + std::to_string(imageId) + ",c=" + std::to_string(cols) + ",r=" + std::to_string(rows) + ",z=-1,q=2\x1b\\";
}

std::string kittyDelete(uint32_t imageId) { return "\x1b_Ga=d,d=i,i=" + std::to_string(imageId) + ",q=2\x1b\\"; }

} // namespace n8v::detail
