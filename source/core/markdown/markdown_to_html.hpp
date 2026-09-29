#pragma once

#include "markdown.hpp"

#include <string>

namespace n8v::detail::markdown {

std::string toHtml(const Document &document);

} // namespace n8v::detail::markdown
