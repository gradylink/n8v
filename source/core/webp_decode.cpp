#include "core/webp_decode.hpp"

#include <cstdio>
#include <cstring>

#ifdef N8V_HAVE_WEBP
#include <webp_lazy_vars.h>
#include <webpdemux_lazy_vars.h>

extern "C" {
struct WebPData {
  const uint8_t *bytes;
  size_t size;
};
struct WebPAnimDecoder;
struct WebPAnimDecoderOptions {
  int color_mode; // MODE_RGBA is 1
  int use_threads;
  uint32_t padding[7];
};
struct WebPAnimInfo {
  uint32_t canvas_width;
  uint32_t canvas_height;
  uint32_t loop_count;
  uint32_t bgcolor;
  uint32_t frame_count;
  uint32_t pad[4];
};
uint8_t *WebPDecodeRGBA(const uint8_t *data, size_t data_size, int *width, int *height);
void WebPFree(void *ptr);
int WebPAnimDecoderOptionsInitInternal(WebPAnimDecoderOptions *, int);
WebPAnimDecoder *WebPAnimDecoderNewInternal(const WebPData *, const WebPAnimDecoderOptions *, int);
int WebPAnimDecoderGetInfo(const WebPAnimDecoder *, WebPAnimInfo *);
int WebPAnimDecoderGetNext(WebPAnimDecoder *, uint8_t **buf, int *timestamp);
void WebPAnimDecoderDelete(WebPAnimDecoder *);
}

namespace {

constexpr int kDemuxAbiVersion = 0x0107;
constexpr int kModeRgba = 1;

void saidOnce(const char *what) {
  static bool said = false;
  if (said) return;
  said = true;
  std::fprintf(stderr, "[n8v] WebP images need %s, which isn't installed; they won't be shown\n", what);
}

// The VP8X header says whether the file is animated (bit 1 of its flags).
bool isAnimated(const uint8_t *bytes, size_t size) { return size >= 21 && std::memcmp(bytes + 12, "VP8X", 4) == 0 && (bytes[20] & 0x02) != 0; }

bool decodeFirstFrame(const uint8_t *bytes, size_t size, std::vector<uint8_t> &rgba, int &width, int &height) {
  if (!lzy_webpdemux_lazy_is_available()) {
    saidOnce("libwebpdemux (for animated ones)");
    return false;
  }
  WebPAnimDecoderOptions options{};
  if (!WebPAnimDecoderOptionsInitInternal(&options, kDemuxAbiVersion)) return false;
  options.color_mode = kModeRgba;
  options.use_threads = 0;
  const WebPData data{bytes, size};
  WebPAnimDecoder *decoder = WebPAnimDecoderNewInternal(&data, &options, kDemuxAbiVersion);
  if (!decoder) return false;
  bool ok = false;
  WebPAnimInfo info{};
  uint8_t *frame = nullptr;
  int timestamp = 0;
  if (WebPAnimDecoderGetInfo(decoder, &info) && info.canvas_width > 0 && info.canvas_height > 0 && WebPAnimDecoderGetNext(decoder, &frame, &timestamp) && frame) {
    width = (int)info.canvas_width;
    height = (int)info.canvas_height;
    rgba.assign(frame, frame + (size_t)width * (size_t)height * 4); // the buffer is the decoder's, and goes with it
    ok = true;
  }
  WebPAnimDecoderDelete(decoder);
  return ok;
}

} // namespace

#endif // N8V_HAVE_WEBP

namespace n8v::detail {

bool looksLikeWebp(const uint8_t *bytes, size_t size) { return size >= 12 && std::memcmp(bytes, "RIFF", 4) == 0 && std::memcmp(bytes + 8, "WEBP", 4) == 0; }

bool decodeWebp(const uint8_t *bytes, size_t size, std::vector<uint8_t> &rgba, int &width, int &height) {
#ifdef N8V_HAVE_WEBP
  if (!looksLikeWebp(bytes, size)) return false;
  if (!lzy_webp_lazy_is_available()) {
    saidOnce("libwebp");
    return false;
  }
  if (isAnimated(bytes, size)) return decodeFirstFrame(bytes, size, rgba, width, height);

  int w = 0, h = 0;
  uint8_t *pixels = WebPDecodeRGBA(bytes, size, &w, &h);
  if (!pixels) return false;
  rgba.assign(pixels, pixels + (size_t)w * (size_t)h * 4);
  WebPFree(pixels);
  width = w;
  height = h;
  return true;
#else
  (void)bytes;
  (void)size;
  (void)rgba;
  (void)width;
  (void)height;
  return false;
#endif
}

} // namespace n8v::detail
