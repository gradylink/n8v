#pragma once

#include "core/tree/node.hpp"
#include "core/tree/persistent.hpp"

namespace n8v::detail::tree {

OwnedProps adopt(const Node &recorded);

const char *idOf(const Node::Props &props);

BoundValue readBound(const Node::Props &props);

} // namespace n8v::detail::tree
