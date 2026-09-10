#include <n8v/n8v_c.h>

#include "core/backend.hpp"
#include "core/style.hpp"

#include "core/ui_core_internal.hpp"

#include <cstdlib>
#include <cstring>

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

} // extern "C"
