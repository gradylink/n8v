#pragma once

#include "core/tree/persistent.hpp"

#include <n8v/n8v_c.h>

#include <string>
#include <string_view>
#include <vector>

namespace n8v::detail::tree::dispatch {

void activate(PNode &node);

void dismiss(PNode &node);

void setChecked(PNode &node, bool checked);

void select(PNode &node, int index);

void selectPage(PNode &page);

void setSlider(PNode &node, float value);

void setText(PNode &node, std::string_view text);

void submit(PNode &node);

bool key(PNode &node, n8v_key key, unsigned mods);

void pasteFiles(PNode &node, const std::vector<std::string> &paths);

void hover(PNode &node, bool hovered);

} // namespace n8v::detail::tree::dispatch
