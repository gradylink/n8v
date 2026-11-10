#pragma once

#include <n8v/style.hpp>

#include <cstdint>

namespace n8v {

const Paint &activePaint();

/** Defaults to the N8V_STYLE env var (plain/material/cupertino/fluent/custom), else Plain. */
void setStyleFamily(StyleFamily family);
StyleFamily activeStyleFamily();

uint64_t activeStyleGeneration();

} // namespace n8v
