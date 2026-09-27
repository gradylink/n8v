#include <n8v/n8v_ffi.h>

#include <stdlib.h>
#include <string.h>

void n8v_ffi_string_buf_set(n8v_string_buf *buf, const char *text, size_t len) {
  size_t needed = len + 1;
  if (needed > buf->capacity) {
    size_t newCap = buf->capacity == 0 ? 16 : buf->capacity;
    while (newCap < needed) newCap *= 2;
    char *newData = (char *)realloc(buf->data, newCap);
    if (!newData) return;
    buf->data = newData;
    buf->capacity = newCap;
  }
  memcpy(buf->data, text, len);
  buf->data[len] = '\0';
  buf->length = len;
}

void n8v_ffi_open_flex(const n8v_flex_options *options) { n8v_open_flex(*options); }
void n8v_ffi_open_panel(const n8v_panel_options *options) { n8v_open_panel(*options); }
void n8v_ffi_open_sidebar(const n8v_sidebar_options *options) { n8v_open_sidebar(*options); }
bool n8v_ffi_open_page(const n8v_page_options *options) { return n8v_open_page(*options); }

void n8v_ffi_set_button_opts(const n8v_button_options *opts) { _n8v_set_button_opts(*opts); }
void n8v_ffi_set_text_opts(const n8v_text_options *opts) { _n8v_set_text_opts(*opts); }
void n8v_ffi_set_checkbox_opts(const n8v_checkbox_options *opts) { _n8v_set_checkbox_opts(*opts); }
void n8v_ffi_set_toggle_opts(const n8v_toggle_options *opts) { _n8v_set_toggle_opts(*opts); }
void n8v_ffi_set_radio_opts(const n8v_radio_options *opts) { _n8v_set_radio_opts(*opts); }

void n8v_ffi_entry(const n8v_entry_options *options) { n8v_entry(*options); }
void n8v_ffi_dropdown(const n8v_dropdown_options *options) { n8v_dropdown(*options); }
void n8v_ffi_slider(const n8v_slider_options *options) { n8v_slider(*options); }
void n8v_ffi_image(const n8v_image_options *options) { n8v_image(*options); }
void n8v_ffi_icon(const n8v_icon_options *options) { n8v_icon(*options); }
