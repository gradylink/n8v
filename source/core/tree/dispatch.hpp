#pragma once

#include "core/tree/persistent.hpp"

#include <string_view>

namespace n8v::detail::tree::dispatch {

void activate(PNode &node);

void setChecked(PNode &node, bool checked);

void select(PNode &node, int index);

void selectPage(PNode &page);

void setSlider(PNode &node, float value);

void setText(PNode &node, std::string_view text);

void submit(PNode &node);

void hover(PNode &node, bool hovered);

} // namespace n8v::detail::tree::dispatch
