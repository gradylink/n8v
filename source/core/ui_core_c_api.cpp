#include <n8v/n8v_c.h>

#include "core/backend.hpp"
#include "core/style.hpp"

#include "core/ui_core_internal.hpp"
#include "styles/paint_conversions.hpp"
#include "styles/style_registry.hpp"

#include <cstdlib>
#include <cstring>

namespace {

const n8v::Paint &bundledPaintForFamily(n8v_style_family family) {
  switch (family) {
  case N8V_STYLE_FAMILY_MATERIAL:
    return n8v::detail::materialPaint();
  case N8V_STYLE_FAMILY_CUPERTINO:
    return n8v::detail::cupertinoPaint();
  case N8V_STYLE_FAMILY_FLUENT:
    return n8v::detail::fluentPaint();
  case N8V_STYLE_FAMILY_CUSTOM:
  case N8V_STYLE_FAMILY_PLAIN:
    break;
  }
  return n8v::detail::plainPaint();
}

} // namespace

extern "C" {

void n8v_string_buf_init(n8v_string_buf *buf) {
  if (!buf) return;
  buf->data = static_cast<char *>(std::malloc(1));
  if (buf->data) buf->data[0] = '\0';
  buf->length = 0;
  buf->capacity = buf->data ? 1 : 0;
}

void n8v_string_buf_free(n8v_string_buf *buf) {
  if (!buf) return;
  std::free(buf->data);
  buf->data = nullptr;
  buf->length = 0;
  buf->capacity = 0;
}

const char *n8v_string_buf_cstr(const n8v_string_buf *buf) { return (buf && buf->data) ? buf->data : ""; }

bool n8v_initialize(int width, int height, const char *title) { return n8v::activeBackend().initialize(width, height, n8v::detail::ui_internal::toView(title)); }

bool n8v_pump_events(void) { return n8v::activeBackend().pumpEvents(); }

void n8v_shutdown(void) { n8v::activeBackend().shutdown(); }

void n8v_set_style_family(n8v_style_family family) { n8v::setStyleFamily(n8v::detail::ui_internal::toStyleFamily(family)); }

n8v_style_family n8v_active_style_family(void) { return n8v::detail::ui_internal::fromStyleFamily(n8v::activeStyleFamily()); }

void n8v_set_custom_paint(const n8v_custom_paint_vtable *vtable, void *userdata) {
  n8v::detail::setCustomPaintVTable(vtable, userdata);
  n8v::setStyleFamily(n8v::StyleFamily::Custom);
}

n8v_button_paint n8v_style_button_paint(n8v_style_family family, n8v_button_style style, bool hovered, bool pressed) {
  return n8v::detail::toC(bundledPaintForFamily(family).button(n8v::detail::fromC(style), hovered, pressed));
}

n8v_text_paint n8v_style_text_paint(n8v_style_family family, n8v_text_options options) {
  return n8v::detail::toC(bundledPaintForFamily(family).text(n8v::detail::fromC(options)));
}

n8v_checkbox_paint n8v_style_checkbox_paint(n8v_style_family family, bool checked, bool hovered, bool pressed) {
  return n8v::detail::toC(bundledPaintForFamily(family).checkbox(checked, hovered, pressed));
}

n8v_radio_paint n8v_style_radio_paint(n8v_style_family family, bool selected, bool hovered, bool pressed) {
  return n8v::detail::toC(bundledPaintForFamily(family).radio(selected, hovered, pressed));
}

n8v_toggle_paint n8v_style_toggle_paint(n8v_style_family family, bool on, bool hovered, bool pressed) {
  return n8v::detail::toC(bundledPaintForFamily(family).toggle(on, hovered, pressed));
}

n8v_entry_paint n8v_style_entry_paint(n8v_style_family family, bool focused, bool has_value) {
  return n8v::detail::toC(bundledPaintForFamily(family).entry(focused, has_value));
}

n8v_dropdown_paint n8v_style_dropdown_paint(n8v_style_family family, bool open, bool has_selection, bool hovered, bool pressed) {
  return n8v::detail::toC(bundledPaintForFamily(family).dropdown(open, has_selection, hovered, pressed));
}

n8v_slider_paint n8v_style_slider_paint(n8v_style_family family, bool hovered, bool pressed) {
  return n8v::detail::toC(bundledPaintForFamily(family).slider(hovered, pressed));
}

n8v_image_paint n8v_style_image_paint(n8v_style_family family) { return n8v::detail::toC(bundledPaintForFamily(family).image()); }

n8v_icon_paint n8v_style_icon_paint(n8v_style_family family) { return n8v::detail::toC(bundledPaintForFamily(family).icon()); }

n8v_sidebar_paint n8v_style_sidebar_paint(n8v_style_family family) { return n8v::detail::toC(bundledPaintForFamily(family).sidebar()); }

} // extern "C"
