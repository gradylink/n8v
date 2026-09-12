#pragma once

#include <n8v/n8v_c.h>

#include <cstdint>
#include <vector>

namespace n8v::detail {

struct DecodedImage {
  std::vector<uint8_t> owned;
  const uint8_t *rgba = nullptr;
  int width = 0;
  int height = 0;
};

const DecodedImage *getOrDecodeImage(const n8v_image_options &options);

void setImageBundleLookup(n8v_image_bundle_lookup_fn fn, void *userdata);

const DecodedImage *getOrBakeRoundedImage(
  const DecodedImage *source, int targetWidth, int targetHeight, float radiusTopLeft, float radiusTopRight, float radiusBottomLeft, float radiusBottomRight
);

} // namespace n8v::detail
