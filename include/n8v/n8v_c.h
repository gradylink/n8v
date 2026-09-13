#ifndef N8V_C_H
#define N8V_C_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#define N8V_API __declspec(dllexport)
#elif defined(__GNUC__) || defined(__clang__)
#define N8V_API __attribute__((visibility("default")))
#else
#define N8V_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum n8v_direction {
  N8V_DIRECTION_HORIZONTAL,
  N8V_DIRECTION_VERTICAL,
} n8v_direction;

typedef enum n8v_align {
  N8V_ALIGN_START,
  N8V_ALIGN_CENTER,
  N8V_ALIGN_END,
} n8v_align;

typedef enum n8v_button_style {
  N8V_BUTTON_STYLE_PRIMARY,
  N8V_BUTTON_STYLE_SECONDARY,
} n8v_button_style;

typedef enum n8v_sizing_mode {
  N8V_SIZING_FIT,
  N8V_SIZING_GROW,
  N8V_SIZING_FIXED,
  N8V_SIZING_PERCENT,
} n8v_sizing_mode;

typedef enum n8v_style_family {
  N8V_STYLE_FAMILY_PLAIN,
  N8V_STYLE_FAMILY_MATERIAL,
  N8V_STYLE_FAMILY_CUPERTINO,
  N8V_STYLE_FAMILY_FLUENT,
} n8v_style_family;

typedef struct n8v_sizing {
  n8v_sizing_mode mode;
  float value;
  float min;
  float max;
} n8v_sizing;

static inline n8v_sizing n8v_sizing_fit(float min, float max) {
  n8v_sizing s;
  s.mode = N8V_SIZING_FIT;
  s.value = 0.0f;
  s.min = min;
  s.max = max;
  return s;
}

static inline n8v_sizing n8v_sizing_grow(float min, float max) {
  n8v_sizing s;
  s.mode = N8V_SIZING_GROW;
  s.value = 0.0f;
  s.min = min;
  s.max = max;
  return s;
}

static inline n8v_sizing n8v_sizing_fixed(float pixels) {
  n8v_sizing s;
  s.mode = N8V_SIZING_FIXED;
  s.value = pixels;
  s.min = 0.0f;
  s.max = 0.0f;
  return s;
}

static inline n8v_sizing n8v_sizing_percent(float fraction) {
  n8v_sizing s;
  s.mode = N8V_SIZING_PERCENT;
  s.value = fraction;
  s.min = 0.0f;
  s.max = 0.0f;
  return s;
}

typedef struct n8v_color {
  float r, g, b, a;
} n8v_color;

typedef struct n8v_padding {
  uint16_t left, right, top, bottom;
} n8v_padding;

typedef struct n8v_corner_radius {
  float top_left, top_right, bottom_left, bottom_right;
} n8v_corner_radius;

typedef enum n8v_rounding_mode {
  N8V_ROUNDING_STYLE_DEFAULT,
  N8V_ROUNDING_NONE,
  N8V_ROUNDING_FIXED,
} n8v_rounding_mode;

typedef struct n8v_rounding {
  n8v_rounding_mode mode;
  n8v_corner_radius radius; /** FIXED only */
} n8v_rounding;

static inline n8v_rounding n8v_rounding_style_default(void) {
  n8v_rounding r;
  r.mode = N8V_ROUNDING_STYLE_DEFAULT;
  r.radius.top_left = 0.0f;
  r.radius.top_right = 0.0f;
  r.radius.bottom_left = 0.0f;
  r.radius.bottom_right = 0.0f;
  return r;
}

static inline n8v_rounding n8v_rounding_none(void) {
  n8v_rounding r = n8v_rounding_style_default();
  r.mode = N8V_ROUNDING_NONE;
  return r;
}

static inline n8v_rounding n8v_rounding_fixed(float top_left, float top_right, float bottom_left, float bottom_right) {
  n8v_rounding r;
  r.mode = N8V_ROUNDING_FIXED;
  r.radius.top_left = top_left;
  r.radius.top_right = top_right;
  r.radius.bottom_left = bottom_left;
  r.radius.bottom_right = bottom_right;
  return r;
}

typedef struct n8v_string_buf {
  char *data;
  size_t length;
  size_t capacity;
} n8v_string_buf;

N8V_API void n8v_string_buf_init(n8v_string_buf *buf);
N8V_API void n8v_string_buf_free(n8v_string_buf *buf);
N8V_API const char *n8v_string_buf_cstr(const n8v_string_buf *buf);

typedef void (*n8v_click_fn)(void *userdata);
typedef void (*n8v_bool_change_fn)(bool value, void *userdata);
typedef void (*n8v_int_change_fn)(int value, void *userdata);
typedef void (*n8v_text_change_fn)(const char *text, size_t length, void *userdata);
typedef void (*n8v_float_change_fn)(float value, void *userdata);

typedef struct n8v_flex_options {
  n8v_direction direction;
  uint16_t gap;
  n8v_padding padding;
  n8v_align h_align;
  n8v_align v_align;
  n8v_sizing width;
  n8v_sizing height;
  bool clip_horizontal;
  bool clip_vertical;
} n8v_flex_options;

typedef struct n8v_text_options {
  bool bold;
  bool italic;
  const char *url;
  n8v_color color;
} n8v_text_options;

typedef enum n8v_icon_variant {
  N8V_ICON_VARIANT_OUTLINE,
  N8V_ICON_VARIANT_FILLED,
} n8v_icon_variant;

typedef enum n8v_icon_position {
  N8V_ICON_POSITION_LEADING,
  N8V_ICON_POSITION_TRAILING,
} n8v_icon_position;

typedef struct n8v_button_options {
  n8v_button_style style;
  n8v_click_fn on_click;
  void *on_click_userdata;
  const char *icon;
  n8v_icon_variant icon_variant;
  n8v_icon_position icon_position;
} n8v_button_options;

typedef struct n8v_checkbox_options {
  bool *checked;
  n8v_bool_change_fn on_change;
  void *on_change_userdata;
} n8v_checkbox_options;

typedef struct n8v_toggle_options {
  bool *checked;
  n8v_bool_change_fn on_change;
  void *on_change_userdata;
} n8v_toggle_options;

typedef struct n8v_radio_options {
  int *selected;
  int value;
  n8v_int_change_fn on_change;
  void *on_change_userdata;
} n8v_radio_options;

typedef struct n8v_entry_options {
  n8v_string_buf *value;
  const char *placeholder;
  bool password;
  n8v_text_change_fn on_change;
  void *on_change_userdata;
} n8v_entry_options;

typedef struct n8v_dropdown_options {
  const char *const *items;
  size_t item_count;
  int *selected; /** out of range shows placeholder */
  const char *placeholder;
  n8v_int_change_fn on_change;
  void *on_change_userdata;
} n8v_dropdown_options;

typedef struct n8v_slider_options {
  float *value;
  float min;
  float max;
  n8v_float_change_fn on_change;
  void *on_change_userdata;
} n8v_slider_options;

typedef enum n8v_image_source_kind {
  N8V_IMAGE_SOURCE_PATH,
  N8V_IMAGE_SOURCE_BUNDLE,
  N8V_IMAGE_SOURCE_ENCODED,
  N8V_IMAGE_SOURCE_RGBA,
} n8v_image_source_kind;

typedef struct n8v_image_options {
  n8v_image_source_kind source_kind;
  const char *path;
  const uint8_t *encoded_data;
  size_t encoded_size;
  const uint8_t *pixels;
  int pixel_width;
  int pixel_height;
  n8v_sizing width;
  n8v_sizing height;
  n8v_rounding rounding;
} n8v_image_options;

typedef bool (*n8v_image_bundle_lookup_fn)(const char *virtual_path, const void **out_data, size_t *out_size, void *userdata);

typedef struct n8v_icon_options {
  const char *name;
  n8v_icon_variant variant;
  n8v_sizing width;
  n8v_sizing height;
  n8v_color tint;
} n8v_icon_options;

N8V_API bool n8v_initialize(int width, int height, const char *title);
N8V_API bool n8v_pump_events(void);
N8V_API void n8v_shutdown(void);

N8V_API void n8v_set_style_family(n8v_style_family family);
N8V_API n8v_style_family n8v_active_style_family(void);

N8V_API void n8v_begin_frame(void);
N8V_API void n8v_end_frame(void);
N8V_API void n8v_open_flex(n8v_flex_options options);
N8V_API void n8v_close_flex(void);

#define N8V_UI() for (uint8_t n8v_c_uiLatch = (n8v_begin_frame(), 0); n8v_c_uiLatch < 1; n8v_c_uiLatch = 1, n8v_end_frame())

#define n8v_flex(...) for (uint8_t n8v_c_flexLatch = (n8v_open_flex(__VA_ARGS__), 0); n8v_c_flexLatch < 1; n8v_c_flexLatch = 1, n8v_close_flex())

N8V_API void _n8v_set_button_opts(n8v_button_options opts);
N8V_API void _n8v_button_commit(const char *label);
#define n8v_button(opts)                                                                                                                                                      \
  _n8v_set_button_opts(opts);                                                                                                                                                 \
  _n8v_button_commit

N8V_API void _n8v_set_text_opts(n8v_text_options opts);
N8V_API void _n8v_text_commit(const char *label);

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && !defined(__cplusplus)
static inline void (*_n8v_text_dispatch_opts(n8v_text_options opts))(const char *) {
  _n8v_set_text_opts(opts);
  return _n8v_text_commit;
}
static inline void _n8v_text_dispatch_label(const char *label) {
  n8v_text_options opts = {0};
  _n8v_set_text_opts(opts);
  _n8v_text_commit(label);
}
#define n8v_text(x) _Generic((x), n8v_text_options: _n8v_text_dispatch_opts, default: _n8v_text_dispatch_label)(x)
#elif !defined(__cplusplus)
#define n8v_text(opts)                                                                                                                                                        \
  _n8v_set_text_opts(opts);                                                                                                                                                   \
  _n8v_text_commit
#endif

N8V_API void _n8v_set_checkbox_opts(n8v_checkbox_options opts);
N8V_API void _n8v_checkbox_commit(const char *label);
#define n8v_checkbox(opts)                                                                                                                                                    \
  _n8v_set_checkbox_opts(opts);                                                                                                                                               \
  _n8v_checkbox_commit

N8V_API void _n8v_set_toggle_opts(n8v_toggle_options opts);
N8V_API void _n8v_toggle_commit(const char *label);
#define n8v_toggle(opts)                                                                                                                                                      \
  _n8v_set_toggle_opts(opts);                                                                                                                                                 \
  _n8v_toggle_commit

N8V_API void _n8v_set_radio_opts(n8v_radio_options opts);
N8V_API void _n8v_radio_commit(const char *label);
#define n8v_radio(opts)                                                                                                                                                       \
  _n8v_set_radio_opts(opts);                                                                                                                                                  \
  _n8v_radio_commit

N8V_API void n8v_entry(n8v_entry_options options);
N8V_API void n8v_dropdown(n8v_dropdown_options options);
N8V_API void n8v_slider(n8v_slider_options options);
N8V_API void n8v_image(n8v_image_options options);
N8V_API void n8v_set_image_bundle_lookup(n8v_image_bundle_lookup_fn fn, void *userdata);
N8V_API void n8v_icon(n8v_icon_options options);

#ifdef __cplusplus
}

inline void (*n8v_text(n8v_text_options opts))(const char *) {
  _n8v_set_text_opts(opts);
  return _n8v_text_commit;
}
inline void n8v_text(const char *label) {
  n8v_text_options opts{};
  _n8v_set_text_opts(opts);
  _n8v_text_commit(label);
}
#endif

#ifdef N8V_NO_PREFIX

#define UI N8V_UI
#define flex n8v_flex
#define button n8v_button
#define text n8v_text
#define checkbox n8v_checkbox
#define toggle n8v_toggle
#define radio n8v_radio
#define entry n8v_entry
#define dropdown n8v_dropdown
#define slider n8v_slider
#define image n8v_image
#define set_image_bundle_lookup n8v_set_image_bundle_lookup

#define set_style_family n8v_set_style_family
#define active_style_family n8v_active_style_family

#define sizing_fit n8v_sizing_fit
#define sizing_grow n8v_sizing_grow
#define sizing_fixed n8v_sizing_fixed
#define sizing_percent n8v_sizing_percent

#define rounding_style_default n8v_rounding_style_default
#define rounding_none n8v_rounding_none
#define rounding_fixed n8v_rounding_fixed

#define string_buf n8v_string_buf
#define string_buf_init n8v_string_buf_init
#define string_buf_free n8v_string_buf_free
#define string_buf_cstr n8v_string_buf_cstr

#define flex_options n8v_flex_options
#define text_options n8v_text_options
#define button_options n8v_button_options
#define checkbox_options n8v_checkbox_options
#define toggle_options n8v_toggle_options
#define radio_options n8v_radio_options
#define entry_options n8v_entry_options
#define dropdown_options n8v_dropdown_options
#define slider_options n8v_slider_options
#define image_options n8v_image_options
#define image_source_kind n8v_image_source_kind
#define icon_options n8v_icon_options

#endif

#endif
