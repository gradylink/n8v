#pragma once

#include "core/image_loader.hpp"

#include <n8v/types.hpp>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace n8v::detail::html_assets {

using FontUrlFn = std::function<std::string(const std::string &fileName, const std::vector<unsigned char> &bytes)>;

std::string fontFaceCss(FontFamily family, const FontUrlFn &urlFor);

const std::vector<uint8_t> &imagePng(const DecodedImage &image);

const std::string &imageDataUri(const DecodedImage &image);

std::string contentHash(const void *data, size_t size);

} // namespace n8v::detail::html_assets
