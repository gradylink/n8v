#ifndef N8V_FFI_H
#define N8V_FFI_H

/**
 * This is for some bindings (e.g. JavaScript)
 *
 * This header is not meant for C/C++ consumers, who should use n8v_c.h (or the C++ API) directly.
 */

#include "n8v_c.h"

#ifdef __cplusplus
extern "C" {
#endif

N8V_API void n8v_ffi_open_flex(const n8v_flex_options *options);
N8V_API void n8v_ffi_open_panel(const n8v_panel_options *options);
N8V_API void n8v_ffi_open_sidebar(const n8v_sidebar_options *options);
/** Returns true if this page is the selected one - matches n8v_open_page. */
N8V_API bool n8v_ffi_open_page(const n8v_page_options *options);

N8V_API void n8v_ffi_set_button_opts(const n8v_button_options *opts);
N8V_API void n8v_ffi_set_text_opts(const n8v_text_options *opts);
N8V_API void n8v_ffi_set_checkbox_opts(const n8v_checkbox_options *opts);
N8V_API void n8v_ffi_set_toggle_opts(const n8v_toggle_options *opts);
N8V_API void n8v_ffi_set_radio_opts(const n8v_radio_options *opts);

N8V_API void n8v_ffi_string_buf_set(n8v_string_buf *buf, const char *text, size_t len);

N8V_API void n8v_ffi_entry(const n8v_entry_options *options);
N8V_API void n8v_ffi_dropdown(const n8v_dropdown_options *options);
N8V_API void n8v_ffi_slider(const n8v_slider_options *options);
N8V_API void n8v_ffi_image(const n8v_image_options *options);
N8V_API void n8v_ffi_icon(const n8v_icon_options *options);

#ifdef __cplusplus
}
#endif

#endif
