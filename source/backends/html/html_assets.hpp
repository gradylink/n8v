#pragma once

#include "core/image_loader.hpp"

#include <n8v/types.hpp>

#include <string>

namespace n8v::detail::html_assets {

std::string fontFaceCss();

const std::string &imageDataUri(const DecodedImage &image);

} // namespace n8v::detail::html_assets
