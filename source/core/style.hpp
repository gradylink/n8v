#pragma once

#include <n8v/style.hpp>

#include <cstdint>

namespace n8v {

const Paint &activePaint();

/** Defaults to the N8V_STYLE env var (plain/material/cupertino/fluent/custom), else Plain. */
void setStyleFamily(StyleFamily family);
StyleFamily activeStyleFamily();

uint64_t activeStyleGeneration();

/**
 * Whether the desktop prefers a dark appearance: N8V_THEME=dark|light overrides, else the GNOME/GTK/KDE settings are consulted.
 * Only the backends that draw everything themselves (the Clay ones) look at this; native backends follow their toolkit.
 */
bool activeThemeIsDark();

} // namespace n8v
