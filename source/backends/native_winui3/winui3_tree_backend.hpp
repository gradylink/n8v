#pragma once

#include "core/backend.hpp"

#include <memory>

namespace n8v::detail {

bool winui3Available();

std::unique_ptr<Backend> makeWinUI3TreeBackend();

} // namespace n8v::detail
