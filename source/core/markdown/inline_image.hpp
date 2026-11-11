#pragma once

#include "core/image_loader.hpp"
#include "image_state.hpp"

#include <algorithm>
#include <cmath>

namespace n8v::detail::markdown {

inline const DecodedImage *inlineImageFor(const InlineRun &run, const char *imageState, float defaultHeight, float &width, float &height) {
  const std::string path = lookupImage(imageState, run.imageUrl);
  if (path.empty()) return nullptr;
  n8v_image_options options{};
  options.source_kind = N8V_IMAGE_SOURCE_PATH;
  options.path = path.c_str();
  const DecodedImage *image = getOrDecodeImage(options);
  if (!image || image->width <= 0 || image->height <= 0) return nullptr;
  height = run.imageSize > 0 ? (float)run.imageSize : defaultHeight;
  width = std::max(1.0f, std::round((float)image->width * height / (float)image->height));
  return image;
}

} // namespace n8v::detail::markdown
