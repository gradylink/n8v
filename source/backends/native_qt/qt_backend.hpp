#pragma once

#include "core/backend.hpp"

#include <memory>

namespace n8v::detail {

std::unique_ptr<Backend> makeQtBackend();

} // namespace n8v::detail
