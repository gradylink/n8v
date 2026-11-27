#pragma once

#include "core/tree/persistent.hpp"

#include <n8v/n8v_c.h>

#include <string>
#include <string_view>
#include <vector>

namespace n8v::detail::tree::dispatch {

void activate(PNode &node);
/** For an image: where in it (0 to 1 on each axis) the click landed. Anything else is a plain activate. */
void activateAt(PNode &node, float x, float y);
/** A zoom gesture over an image: the change since the last call, and where it is centered (0 to 1 on each axis). */
void zoom(PNode &node, float factor, float x, float y);
/** A drag over an image: the change in position since the last call, in pixels. */
void drag(PNode &node, float dx, float dy);

void dismiss(PNode &node);

void setChecked(PNode &node, bool checked);

void select(PNode &node, int index);

void selectPage(PNode &page);

void setSlider(PNode &node, float value);

void requestFullscreen(PNode &node, bool fullscreen);

void setText(PNode &node, std::string_view text);

void submit(PNode &node);

bool key(PNode &node, n8v_key key, unsigned mods);

void pasteFiles(PNode &node, const std::vector<std::string> &paths);

void hover(PNode &node, bool hovered);

} // namespace n8v::detail::tree::dispatch
