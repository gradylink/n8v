#pragma once

#include "core/style.hpp"

#include <n8v/n8v_c.h>

namespace n8v::detail {

const Paint &plainPaint();
const Paint &materialPaint();
const Paint &cupertinoPaint();
const Paint &fluentPaint();
const Paint &customPaint();

void setCustomPaintVTable(const n8v_custom_paint_vtable *vtable, void *userdata);

void suppressEnvStyleDefault();

} // namespace n8v::detail
