#pragma once

#include <n8v/style.hpp>

namespace n8v {

const Paint &activePaint();

/** Defaults to the N8V_STYLE env var (plain/material/cupertino/fluent/custom), else Plain. */
void setStyleFamily(StyleFamily family);
StyleFamily activeStyleFamily();

} // namespace n8v
