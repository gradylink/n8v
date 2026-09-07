#pragma once

#include <n8v/backend.hpp>

#include <memory>

namespace n8v::detail {

std::unique_ptr<Backend> makeMilskoBackend();

} // namespace n8v::detail
